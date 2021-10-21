#include <hypercomm/core/typed_value.hpp>
#include <hypercomm/core/generic_locality.hpp>
#include <hypercomm/core/future.hpp>
#include <hypercomm/reductions/reducer.hpp>

namespace hypercomm {

namespace {
CpvDeclare(generic_locality_*, locality_);
}

void try_return(deliverable&& dev) {
  CkAssertMsg(dev, "invalid deliverable!");
  if (dev.kind == deliverable::kValue) {
    // TODO ( should this use the default return path? )
    auto* val = dev.release<hyper_value>();
    val->source = std::move(dev.endpoint());
    try_return(value_ptr(val));
  } else {
    CkAssertMsg(dev.endpoint(), "invalid destination!");
    auto* ctx = access_context_();
    auto& aid = ctx->ckGetArrayID();
    auto& idx = ctx->ckGetArrayIndex();
    interceptor::send_async(aid, idx, std::move(dev));
  }
}

// TODO ( do not assume array-issuedness )
// TODO ( send immediately looking forward! )
void send2future(const future& f, deliverable&& dev) {
  auto& src = f.source;
  auto& proxy =
      (dynamic_cast<element_proxy<CkArrayIndex>*>(src.get()))->c_proxy();
  auto& base = static_cast<const CProxyElement_ArrayElement&>(proxy);
  auto port = std::make_shared<future_port>(f);
  auto* ctx = access_context_();
  dev.update_endpoint(std::move(port));
  interceptor::send_async(base, std::move(dev));
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

void generic_locality_::invalidate_component(const component_id_t& id) {
  auto search = this->components.find(id);
  if (search != std::end(this->components)) {
    auto& com = search->second;
    auto was_alive = com->is_active();
    com->deactivate(components::status_::kInvalidation);
    if (was_alive) {
      this->components.erase(search);
    } else {
      this->invalidations.emplace_back(id);
    }
  }
}

bool generic_locality_::invalidated(const component_id_t& id) {
  if (this->invalidations.empty()) {
    return false;
  } else {
    auto search = std::find(std::begin(this->invalidations),
                            std::end(this->invalidations), id);
    if (search == std::end(this->invalidations)) {
      return false;
    } else {
      this->invalidations.erase(search);

      return true;
    }
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
  auto& port = it->first;
  auto search = port_queue.find(port);
  if (search != port_queue.end()) {
    auto& buffer = search->second;
    while (port->alive && !buffer.empty()) {
      this->passthru(it->second, std::move(buffer.front()));
      buffer.pop_front();
      QdProcess(1);
    }
    if (buffer.empty()) {
      port_queue.erase(search);
    }
  }
}

void generic_locality_::passthru(const destination& dst, deliverable&& dev) {
  switch (dst.kind) {
    case destination::kEndpoint:
      dev.update_endpoint(dst.ep());
      this->receive(std::move(dev));
      break;
    case destination::kComponent:
      this->passthru(dst.com_port(), std::move(dev));
      break;
    case destination::kCallback:
      // TODO set source of value?
      dst.cb()->send(std::move(dev));
      break;
    default:
      unreachable("invalid destination");
  }
}

void generic_locality_::receive(deliverable&& dev) {
  // check how we should handle it
  auto& ep = dev.endpoint();
  CkAssertMsg((bool)ep && dev, "invalid delivery!");
  if (ep.idx_ == CkIndex_locality_base_::idx_demux_CkMessage()) {
    auto port = ep.port_;  // copy the ptr to the port
                           // since we may move it later
    auto search = this->entry_ports.find(port);
    if (search == std::end(this->entry_ports)) {
      CkAssertMsg(port, "port should not be null!");
      this->port_queue[port].push_back(std::move(dev));
      QdCreate(1);
    } else {
      this->passthru(search->second, std::move(dev));
    }
  } else if (dev.kind == deliverable::kMessage) {
    auto* msg = dev.release<CkMessage>();
    ep.export_to(msg);
    this->receive_message(msg);
  } else {
    auto fn = ep.get_handler();
    fn(this, std::move(dev));
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
  auto fn = CkIndex_locality_base_::get_value_handler(epIdx);
  if (fn == nullptr) {
    _entryTable[epIdx]->call(msg, dynamic_cast<CkMigratable*>(this));
  } else {
    this->receive_value(msg, fn);
  }
}

// TODO ( make this migration robust! )
struct zero_copy_payload_ {
 private:
  generic_locality_* parent_;
  zero_copy_value* val_;
  CkNcpyBuffer* src_;

 public:
  zero_copy_payload_(generic_locality_* _1, zero_copy_value* _3,
                     CkNcpyBuffer* _4)
      : parent_(_1), val_(_3), src_(_4) {}

  static void action_(zero_copy_payload_* self, CkDataMsg* msg) {
    auto* buf = (CkNcpyBuffer*)(msg->data);
    auto* ptr = (std::shared_ptr<void>*)buf->getRef();
    buf->deregisterMem();
    CkFreeMsg(msg);

    auto rcvd = self->val_->receive(self->src_, std::move(*ptr));
    CkAssertMsg(rcvd, "buffer deregistration failed!");
    delete ptr;

    if (self->val_->ready()) {
      auto fn = self->val_->ep.get_handler();
      self->parent_->update_context();
      fn(self->parent_, deliverable(self->val_));
    }

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

void generic_locality_::post_buffer(const endpoint& ep,
                                    const std::shared_ptr<void>& buffer,
                                    const std::size_t& size,
                                    const CkNcpyBufferPost& mode) {
  using mapped_type = decltype(this->outstanding)::mapped_type;
  using inner_type = mapped_type::iterator;

  auto find_applicable = [&](mapped_type& queue) -> inner_type {
    return std::find_if(std::begin(queue), std::end(queue),
                        [&](const inner_type::value_type& buffer) {
                          return buffer.second->cnt <= size;
                        });
  };

  inner_type inner;
  auto outer = this->outstanding.find(ep);
  // determine whether we fulfill any outstanding buffers
  if (outer == std::end(this->outstanding) ||
      (inner = find_applicable(outer->second)) == std::end(outer->second)) {
    // if not, save it for the future
    this->buffers[ep].emplace_back(buffer, size, mode);
  } else {
    auto fn = CkIndex_locality_base_::default_value_handler();
    auto& pair = *inner;
    // create a CkNcpyBuffer to receive data into the buffer
    CkNcpyBuffer dest;
    init_buffer_(&dest, mode, size, buffer);
    dest.cb = CkCallback(
        (CkCallbackFn)&zero_copy_payload_::action_,
        new zero_copy_payload_(this, std::move(pair.first), pair.second));
    // send the value request
    dest.get(*pair.second);
    // clean up
    outer->second.erase(inner);
    QdProcess(1);
  }
}

using outstanding_iterator = generic_locality_::outstanding_iterator;
outstanding_iterator generic_locality_::poll_buffer(CkNcpyBuffer* buffer,
                                                    zero_copy_value* val,
                                                    const std::size_t& goal) {
  using value_type = typename decltype(this->buffers)::mapped_type::value_type;
  // seek the queue of buffers from the map
  auto& ep = val->ep;
  auto outer = this->buffers.find(ep);
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
  auto search = this->outstanding.find(ep);
  if (search == std::end(this->outstanding)) {
    // if nothing is avail., create a preregistered buffer to receive data
    auto* ptr = new std::shared_ptr<void>(CkRdmaAlloc(goal), CkRdmaFree);
    new (buffer) CkNcpyBuffer(ptr->get(), goal, CK_BUFFER_PREREG);
    buffer->setRef(ptr);
  }
  return search;
}

void generic_locality_::receive_value(CkMessage* raw,
                                      const value_handler_fn_& fn) {
  auto epIdx = UsrToEnv(raw)->getMsgIdx();
  message* msg = (epIdx == message::index()) ? (message*)raw : nullptr;
  if (msg && msg->is_zero_copy()) {
    // will be deleted by zero_copy_payload_
    auto* val = new zero_copy_value(msg);
    PUP::fromMem p(msg->payload);
    p | val->buffers;
    val->offset = p.get_current_pointer();

    for (auto& src : val->buffers) {
      CkNcpyBuffer dest;
      auto search = this->poll_buffer(&dest, val, src.cnt);
      if (search == std::end(this->outstanding)) {
        dest.cb = CkCallback((CkCallbackFn)&zero_copy_payload_::action_,
                             new zero_copy_payload_(this, val, &src));
        dest.get(src);
      } else {
        search->second.emplace_back(val, &src);
        QdCreate(1);
      }
    }
  } else {
    fn(this, deliverable(raw));
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

void generic_locality_::passthru(const com_port_pair_t& port,
                                 deliverable&& dev) {
  auto search = components.find(port.first);
#if CMK_ERROR_CHECKING
  if (search == std::end(components)) {
    std::stringstream ss;
    ss << "fatal> recvd msg for invalid destination com" << port.first << ":"
       << port.second << "!";
    CkAbort("%s", ss.str().c_str());
  }
#endif

  search->second->accept(port.second, std::move(dev));

  this->try_collect(search->second);
}

reducer::out_set reducer::action(reducer::in_set& set) {
  CkAssertMsg(this->n_dstream == 1, "reducers may only have one output");

  auto& dev = std::get<0>(set);
  this->devs_.emplace_back(std::move(dev));
  this->persistent = (this->devs_.size() < this->n_ustream);
  // if we're still waiting on values, suspend!
  if (this->persistent) {
    this->suspend();
  }

  callback_ptr ourCb;
  auto& ourCmbnr = this->combiner;
  // auto cmp = comparable_comparator<callback_ptr>();

  using contribution_type = typed_value<contribution>;
  typename combiner::argument_type args;
  for (auto it = std::begin(this->devs_); it != std::end(this->devs_);) {
    auto& raw = *it;
    auto contrib =
        raw ? dev2typed<typename contribution_type::type>(std::move(raw))
            : std::shared_ptr<contribution_type>();

    if (contrib) {
      if ((*contrib)->msg_ != nullptr) {
        args.emplace_back((*contrib)->msg_);
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

    it = this->devs_.erase(it);
  }

  auto result = (*ourCmbnr)(std::move(args));
  auto contrib = make_typed_value<typename contribution_type::type>(
      std::move(result), ourCmbnr, ourCb);
  return std::make_tuple(std::move(contrib));
}

void generic_locality_::open(const entry_port_ptr& ours, destination&& theirs) {
  ours->alive = true;
  auto pair = this->entry_ports.emplace(ours, std::move(theirs));
#if CMK_ERROR_CHECKING
  if (!pair.second) {
    std::stringstream ss;
    ss << "[";
    for (const auto& epp : this->entry_ports) {
      const auto& other_port = epp.first;
      if (comparable_comparator<entry_port_ptr>()(ours, other_port)) {
        ss << "{" << other_port->to_string() << "}, ";
      } else {
        ss << other_port->to_string() << ", ";
      }
    }
    ss << "]";

    CkAbort("fatal> adding non-unique port %s to:\n\t%s\n",
            ours->to_string().c_str(), ss.str().c_str());
  }
#endif
  this->resync_port_queue(pair.first);
}

}  // namespace hypercomm
