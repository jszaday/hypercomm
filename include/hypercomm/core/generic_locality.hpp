#ifndef __HYPERCOMM_CORE_GENLOC_HPP__
#define __HYPERCOMM_CORE_GENLOC_HPP__

#include "entry_port.hpp"

namespace hypercomm {

using component_port_t = std::pair<component::id_t, component::port_type>;

class destination_ {
  union u_options {
    callback_ptr cb;
    component_port_t port;
    ~u_options() {}
    u_options(void) {}
    u_options(const callback_ptr& x) : cb(x) {}
    u_options(const component_port_t& x) : port(x) {}
  } options;

 public:
  enum type_ : uint8_t { kCallback, kComponentPort };

  const type_ type;

  ~destination_() {
    switch (type) {
      case kCallback: {
        this->options.cb.~callback_ptr();
        break;
      }
      case kComponentPort: {
        this->options.port.~component_port_t();
        break;
      }
    }
  }

  destination_(const callback_ptr& cb) : type(kCallback), options(cb) {}
  destination_(const component_port_t& port)
      : type(kComponentPort), options(port) {}

  destination_(const destination_& dst) : type(dst.type) {
    switch (type) {
      case kCallback: {
        ::new (&this->options) u_options(dst.cb());
        break;
      }
      case kComponentPort: {
        ::new (&this->options) u_options(dst.port());
        break;
      }
    }
  }

  const callback_ptr& cb(void) const {
    CkAssert(this->type == kCallback);
    return this->options.cb;
  }

  const component_port_t& port(void) const {
    CkAssert(this->type == kComponentPort);
    return this->options.port;
  }
};

using entry_port_map = comparable_map<entry_port_ptr, destination_>;
using component_map =
    std::unordered_map<component::id_t, std::unique_ptr<component>>;

extern message* repack_to_port(const entry_port_ptr& port,
                               component::value_type&& value);

template <typename Key>
using message_queue = comparable_map<Key, std::deque<component::value_type>>;

struct generic_locality_ {
  entry_port_map entry_ports;
  component_map components;
  message_queue<entry_port_ptr> port_queue;
  std::vector<component_id_t> invalidations;

  using entry_port_iterator = typename decltype(entry_ports)::iterator;

  generic_locality_(void) { this->update_context(); }

  // implemented in locality.hpp
  void loopback(message* msg);

  virtual ~generic_locality_() {
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

  /* TODO consider introducing a simplified connection API that
   *      utilizes "port authorities", aka port id counters, to
   *      remove src/dstPort for trivial, unordered connections
   */

  inline void connect(const component_id_t& src,
                      const components::port_id_t& srcPort,
                      const component_id_t& dst,
                      const components::port_id_t& dstPort) {
    this->components[src]->update_destination(
        srcPort, this->make_connector(dst, dstPort));
  }

  inline void connect(const component_id_t& src,
                      const components::port_id_t& srcPort,
                      const callback_ptr& cb) {
    this->components[src]->update_destination(srcPort, cb);
  }

  inline void connect(const entry_port_ptr& srcPort, const component_id_t& dst,
                      const components::port_id_t& dstPort) {
    this->components[dst]->add_listener(srcPort);
    this->open(srcPort, std::make_pair(dst, dstPort));
  }

  void receive_value(const entry_port_ptr& port,
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
      CkAssert(search->first && search->first->alive &&
               "entry port must be alive");
      this->try_send(search->second, std::move(value));
    }
  }

  void resync_port_queue(entry_port_iterator& it) {
    const auto port = it->first;
    auto search = port_queue.find(port);
    if (search != port_queue.end()) {
      auto& buffer = search->second;
      while (port->alive && !buffer.empty()) {
        auto& msg = buffer.front();
        this->try_send(it->second, std::move(msg));
        // this->try_collect(it);
        buffer.pop_front();
        QdProcess(1);
      }
      if (buffer.empty()) {
        port_queue.erase(search);
      }
    }
  }

  template <typename Destination>
  void open(const entry_port_ptr& ours, const Destination& theirs) {
    ours->alive = true;
    auto pair = this->entry_ports.emplace(ours, theirs);
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

  inline void invalidate_port(const entry_port_ptr& port) {
    port->alive = port->alive && port->keep_alive();
    if (!port->alive) {
      auto search = this->entry_ports.find(port);
      if (search != std::end(this->entry_ports)) {
        this->entry_ports.erase(search);
      }
    }
  }

  inline bool invalidated(const component::id_t& id) {
    auto search = std::find(std::begin(this->invalidations),
                            std::end(this->invalidations), id);
    if (search == std::end(this->invalidations)) {
      return false;
    } else {
      this->invalidations.erase(search);

      return true;
    }
  }

  // forces termination of component, regardless of resilience
  inline void invalidate_component(const component::id_t& id) {
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

  inline void update_context(void);

  inline callback_ptr make_connector(const component_id_t& com,
                                     const component::port_type& port);

  virtual void try_send(const destination_& dest,
                        component::value_type&& value) = 0;
};

// TODO this is a temporary solution
struct connector_ : public callback {
  generic_locality_* self;
  const component_port_t dst;

  connector_(generic_locality_* _1, const component_port_t& _2)
      : self(_1), dst(_2) {}

  virtual return_type send(argument_type&& value) override {
    self->try_send(destination_(dst), std::move(value));
  }

  virtual void __pup__(serdes& s) override { CkAbort("don't send me"); }
};

inline callback_ptr generic_locality_::make_connector(
    const component_id_t& com, const component::port_type& port) {
  return std::make_shared<connector_>(this, std::make_pair(com, port));
}

namespace {
CpvDeclare(generic_locality_*, locality_);
}

inline void generic_locality_::update_context(void) {
  if (!CpvInitialized(locality_)) {
    CpvInitialize(generic_locality_*, locality_);
  }

  CpvAccess(locality_) = this;
}

inline generic_locality_* access_context(void) {
  auto& locality = *(&CpvAccess(locality_));
  CkAssert(locality && "locality must be valid");
  return locality;
}

void locally_invalidate_(const component::id_t& which) {
  access_context()->invalidate_component(which);
}

callback_ptr local_connector_(const component_id_t& com,
                              const component::port_type& port) {
  return access_context()->make_connector(com, port);
}

void entry_port::take_back(std::shared_ptr<hyper_value>&& value) {
  access_context()->receive_value(this->shared_from_this(), std::move(value));
}

void entry_port::on_completion(const component&) {
  access_context()->invalidate_port(this->shared_from_this());
}

void entry_port::on_invalidation(const component&) {
  access_context()->invalidate_port(this->shared_from_this());
}
}

#endif
