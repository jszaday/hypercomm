#include <hypercomm/core/typed_value.hpp>
#include <hypercomm/core/generic_locality.hpp>
#include <hypercomm/reductions/reducer.hpp>

namespace hypercomm {
// TODO this is a temporary solution
struct connector_ : public callback {
  generic_locality_* self;
  const component_port_t dst;

  connector_(generic_locality_* _1, const component_port_t& _2)
      : self(_1), dst(_2) {}

  virtual return_type send(argument_type&& value) override {
    self->try_send(destination(dst), std::move(value));
  }

  virtual void __pup__(serdes& s) override { CkAbort("don't send me"); }
};

callback_ptr generic_locality_::make_connector(
    const component_id_t& com, const component::port_type& port) {
  return std::make_shared<connector_>(this, std::make_pair(com, port));
}

namespace {
CpvDeclare(generic_locality_*, locality_);
}

/* access the pointer of the currently executing locality
 * call at the start of EPs and after resume-from-sleep
 * NOTE ( this will be retired when mainline Charm PR#3426 is merged )
 */
void generic_locality_::update_context(void) {
  if (!CpvInitialized(locality_)) {
    CpvInitialize(generic_locality_*, locality_);
  }

  CpvAccess(locality_) = this;
}

generic_locality_* access_context_(void) {
  auto& locality = *(&CpvAccess(locality_));
  CkAssertMsg(locality, "locality must be valid");
  return locality;
}

void generic_locality_::invalidate_component(const component::id_t& id) {
  auto search = this->components.find(id);
  if (search != std::end(this->components)) {
    auto& com = search->second;
    auto was_alive = com->alive;
    com->alive = false;
    com->on_invalidation();
    if (was_alive) {
      this->components.erase(search);
    } else {
      this->invalidations.emplace_back(id);
    }
  }
}

bool generic_locality_::invalidated(const component::id_t& id) {
  auto search = std::find(std::begin(this->invalidations),
                          std::end(this->invalidations), id);
  if (search == std::end(this->invalidations)) {
    return false;
  } else {
    this->invalidations.erase(search);

    return true;
  }
}

void generic_locality_::invalidate_port(const entry_port_ptr& port) {
  port->alive = port->alive && port->keep_alive();
  if (!port->alive) {
    auto search = this->entry_ports.find(port);
    if (search != std::end(this->entry_ports)) {
      this->entry_ports.erase(search);
    }
  }
}

void generic_locality_::resync_port_queue(entry_port_iterator& it) {
  const auto port = it->first;
  auto search = port_queue.find(port);
  if (search != port_queue.end()) {
    auto& buffer = search->second;
    while (port->alive && !buffer.empty()) {
      auto& msg = buffer.front();
      this->try_send(it->second, std::move(msg));
      buffer.pop_front();
      QdProcess(1);
    }
    if (buffer.empty()) {
      port_queue.erase(search);
    }
  }
}

void generic_locality_::receive_value(const entry_port_ptr& port,
                                      component::value_type&& value) {
  // save this port as the source of the value
  if (value) value->source = port;
  // seek this port in our list of active ports
  auto search = this->entry_ports.find(port);
  if (search == std::end(this->entry_ports)) {
    // if it is not present, buffer it
    this->port_queue[port].push_back(std::move(value));
    QdCreate(1);
  } else {
    // otherwise, try to deliver it
    CkAssertMsg(search->first && search->first->alive,
                "entry port must be alive");
    this->try_send(search->second, std::move(value));
  }
}

bool generic_locality_::has_value(const entry_port_ptr& port) const {
  auto search = this->port_queue.find(port);
  if (search != std::end(this->port_queue)) {
    return !search->second.empty();
  } else {
    return false;
  }
}

generic_locality_::~generic_locality_() {
  // update context (just in case)
  this->update_context();
  // (I) destroy all our entry ports
  // TODO ensure graceful exit(s) via invalidations?
  this->entry_ports.clear();
  // (II) destroy all our components
  this->components.clear();
  // (III) dump port queue into the network
  for (auto& pair : this->port_queue) {
    auto& port = pair.first;
    for (auto& value : pair.second) {
      this->loopback(port, std::move(value));

      QdProcess(1);
    }
  }
}

void generic_locality_::receive_message(CkMessage* msg) {
  auto* env = UsrToEnv(msg);
  auto epIdx = env->getEpIdx();
  if (epIdx == CkIndex_locality_base_::idx_demux_CkMessage()) {
    CkAssert(env->getMsgIdx() == message::index());
    this->demux_message((message*)msg);
  } else {
    _entryTable[epIdx]->call(msg, dynamic_cast<CkMigratable*>(this));
  }
}

// TODO ( make this migration robust! )
struct zero_copy_payload_ {
 private:
  generic_locality_* parent_;
  entry_port_ptr dst_;

 public:
  zero_copy_payload_(generic_locality_* _1, const entry_port_ptr& _2)
      : parent_(_1), dst_(_2) {}

  zero_copy_payload_(generic_locality_* _1, entry_port_ptr&& _2)
      : parent_(_1), dst_(std::move(_2)) {}

  static void action_(zero_copy_payload_* self, CkDataMsg* msg) {
    auto* buf = (CkNcpyBuffer*)(msg->data);
    auto* ptr = (std::shared_ptr<void>*)buf->getRef();
    auto val = make_value<buffer_value>(std::move(*ptr), buf->cnt);
    delete ptr;
    buf->deregisterMem();
    CkFreeMsg(msg);

    self->parent_->update_context();
    self->parent_->receive_value(self->dst_, std::move(val));

    delete self;
  }
};

template <typename... Args>
static inline void init_buffer_(CkNcpyBuffer* buffer,
                                const CkNcpyBufferPost& mode,
                                const std::size_t& size, Args... args) {
  auto* ptr = new std::shared_ptr<void>(std::forward<Args>(args)...);
  new (buffer) CkNcpyBuffer(ptr->get(), size, mode.regMode, mode.deregMode);
  buffer->setRef(ptr);
}

void generic_locality_::post_buffer(const entry_port_ptr& port,
                                    const std::shared_ptr<void>& buffer,
                                    const std::size_t& size,
                                    const CkNcpyBufferPost& mode) {
  using mapped_type = decltype(this->outstanding)::mapped_type;
  using inner_type = mapped_type::iterator;

  auto find_applicable = [&](mapped_type& queue) -> inner_type {
    return std::find_if(std::begin(queue), std::end(queue),
                        [&](const inner_type::value_type& buffer) {
                          return buffer->cnt <= size;
                        });
  };

  inner_type inner;
  auto outer = this->outstanding.find(port);
  // determine whether we fulfill any outstanding buffers
  if (outer == std::end(this->outstanding) ||
      (inner = find_applicable(outer->second)) == std::end(outer->second)) {
    // if not, save it for the future
    this->buffers[port].emplace_back(buffer, size, mode);
  } else {
    // create a CkNcpyBuffer to receive data into the buffer
    CkNcpyBuffer dest;
    init_buffer_(&dest, mode, size, buffer);
    dest.cb = CkCallback((CkCallbackFn)&zero_copy_payload_::action_,
                         new zero_copy_payload_(this, port));
    // send the value request
    dest.get(**inner);
    // clean up
    outer->second.erase(inner);
    QdProcess(1);
  }
}

using outstanding_iterator = generic_locality_::outstanding_iterator;
outstanding_iterator generic_locality_::poll_buffer(CkNcpyBuffer* buffer,
                                                    const entry_port_ptr& port,
                                                    const std::size_t& goal) {
  using value_type = typename decltype(this->buffers)::mapped_type::value_type;
  // seek the queue of buffers from the map
  auto outer = this->buffers.find(port);
  if (outer != std::end(this->buffers)) {
    auto& queue = outer->second;
    // and within it, seek a buffer large enough to accomodate the request
    auto inner = std::find_if(
        std::begin(queue), std::end(queue),
        [&](const value_type& info) { return (std::get<1>(info) >= goal); });
    if (inner != std::end(queue)) {
      // if found, take ownership of it (and form a buffer)
      init_buffer_(buffer, std::get<2>(*inner), std::get<1>(*inner),
                   std::move(std::get<0>(*inner)));
      // remove the buffer from the queue
      queue.erase(inner);
      // then, if it's empty, delete it (avoid future seeks)
      if (queue.empty()) {
        this->buffers.erase(outer);
      }
      return std::end(this->outstanding);
    }
  }
  auto search = this->outstanding.find(port);
  if (search == std::end(this->outstanding)) {
    // if nothing is avail., create a preregistered buffer to receive data
    auto* ptr = new std::shared_ptr<void>(CkRdmaAlloc(goal), CkRdmaFree);
    new (buffer) CkNcpyBuffer(ptr->get(), goal, CK_BUFFER_PREREG);
    buffer->setRef(ptr);
  }
  return search;
}

void generic_locality_::demux_message(message* msg) {
  this->update_context();
  auto port = std::move(msg->dst);
  if (msg->is_zero_copy()) {
    std::size_t bufferCount;
    PUP::fromMem p(msg->payload);
    p | bufferCount;

    CkEnforceMsg(bufferCount == 1,
                 "can only support one buffer at a time for now.\n");

    std::unique_ptr<CkNcpyBuffer> src(new CkNcpyBuffer);
    p | *src;
    CkFreeMsg(msg);

    CkNcpyBuffer dest;
    auto search = this->poll_buffer(&dest, port, src->cnt);
    if (search == std::end(this->outstanding)) {
      dest.cb = CkCallback((CkCallbackFn)&zero_copy_payload_::action_,
                           new zero_copy_payload_(this, std::move(port)));
      dest.get(*src);
    } else {
      search->second.emplace_back(std::move(src));
      QdCreate(1);
    }
  } else {
    this->receive_value(port, msg2value(msg));
  }
}

void generic_locality_::activate_component(const component_id_t& id) {
  auto search = this->components.find(id);
  if (search != std::end(this->components)) {
    if (this->invalidated(id)) {
      this->components.erase(search);
    } else {
      auto& com = search->second;
      com->activate();
      this->try_collect(com);
    }
  } else {
    CkAbort("fatal> unable to find com%lu.\n", id);
  }
}

void generic_locality_::try_send(const destination& dest,
                                 component::value_type&& value) {
  switch (dest.type) {
    case destination::type_::kCallback: {
      const auto& cb = dest.cb();
      CkAssertMsg(cb, "callback must be valid!");
      cb->send(std::move(value));
      break;
    }
    case destination::type_::kComponentPort:
      this->try_send(dest.port(), std::move(value));
      break;
    default:
      CkAbort("unknown destination type");
  }
}

void generic_locality_::try_send(const component_port_t& port,
                                 component::value_type&& value) {
  auto search = components.find(port.first);
#if CMK_ERROR_CHECKING
  if (search == std::end(components)) {
    std::stringstream ss;
    ss << "fatal> recvd msg for invalid destination com" << port.first << ":"
       << port.second << "!";
    CkAbort("%s", ss.str().c_str());
  }
#endif

  search->second->receive_value(port.second, std::move(value));

  this->try_collect(search->second);
}

reducer::value_set reducer::action(value_set&& accepted) {
  CkAssertMsg(this->n_dstream == 1, "reducers may only have one output");

  auto cmp = comparable_comparator<callback_ptr>();
  callback_ptr ourCb;

  auto& ourCmbnr = this->combiner;

  using contribution_type = typed_value<contribution>;
  typename combiner::argument_type args;
  for (auto& pair : accepted) {
    auto& raw = pair.second;
    auto contrib =
        raw ? value2typed<typename contribution_type::type>(std::move(raw))
            : std::shared_ptr<contribution_type>();
    if (contrib) {
      if ((*contrib)->msg_ != nullptr) {
        args.emplace_back(msg2value((*contrib)->msg_));
      }

      auto& theirCb = (*contrib)->callback_;
      if (theirCb) {
        if (ourCb) {
          // CkAssertMsg(cmp(cb, (*contrib)->callback_), "callbacks must
          // match");
        } else {
          ourCb = theirCb;
        }
      }

      auto& theirCmbnr = (*contrib)->combiner_;
      if (!ourCmbnr && theirCmbnr) {
        ourCmbnr = theirCmbnr;
      }
    }
  }

  auto result = ourCmbnr->send(std::move(args));
  auto contrib = make_typed_value<typename contribution_type::type>(
      std::move(result), ourCmbnr, ourCb);
  return component::make_set(0, std::move(contrib));
}

}  // namespace hypercomm
