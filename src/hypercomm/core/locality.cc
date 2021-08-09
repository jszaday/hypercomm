#include "hypercomm/core/generic_locality.hpp"

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
    port_queue[port].push_back(std::move(value));
    QdCreate(1);
  } else {
    // otherwise, try to deliver it
    CkAssertMsg(search->first && search->first->alive,
                "entry port must be alive");
    this->try_send(search->second, std::move(value));
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
      auto* msg = repack_to_port(port, std::move(value));
      this->loopback(msg);
      QdProcess(1);
    }
  }
}

void generic_locality_::receive_message(message* msg) {
  auto* env = UsrToEnv(msg);
  auto idx = env->getEpIdx();

  if (idx == CkIndex_locality_base_::idx_demux_CkMessage()) {
    this->receive_value(msg->dst, msg2value(msg));
  } else {
    _entryTable[idx]->call(msg, dynamic_cast<CkMigratable*>(this));
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

}  // namespace hypercomm
