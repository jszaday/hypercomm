#ifndef __HYPERCOMM_CORE_GENLOC_HPP__
#define __HYPERCOMM_CORE_GENLOC_HPP__

#include "entry_port.hpp"

namespace hypercomm {
using component_port_t = std::pair<component::id_t, components::port_id_t>;

// TODO elevate this functionality to a more general purpose callback
//      ensure that the reference counting on the callback port is elided when
//      !kCallback
struct destination_ {
  enum type_ : uint8_t { kInvalid, kCallback, kComponentPort };

  type_ type;
  callback_ptr cb;
  component_port_t port;

  destination_(const component_port_t& _1) : type(kComponentPort), port(_1) {}
  destination_(const callback_ptr& _2) : type(kCallback), cb(_2) {}
};

using entry_port_map = comparable_map<entry_port_ptr, destination_>;
using component_map = std::unordered_map<component::id_t, component_ptr>;

//  {
//     access_context()->invalidate_port(*this);
//   }

//   virtual void on_invalidation(const component&) override {
//     access_context()->invalidate_port(*this);
//   }

struct generic_locality_ {
  entry_port_map entry_ports;
  component_map components;

  inline void invalidate_port(entry_port& port) {
    port.alive = port.alive && port.keep_alive();
    if (!port.alive) {
      auto end = std::end(this->entry_ports);
      auto search =
          std::find_if(std::begin(this->entry_ports), end,
                       [&](const typename entry_port_map::value_type& pair) {
                         return &port == pair.first.get();
                       });
      if (search != end) {
        this->entry_ports.erase(search);
      }
    }
  }

  inline void update_context(void);
};

namespace {
CtvDeclare(generic_locality_*, locality_);
}

inline void generic_locality_::update_context(void) {
  if (!CtvInitialized(locality_)) {
    CtvInitialize(generic_locality_*, locality_);
  }

  CtvAccess(locality_) = this;
}

inline generic_locality_* access_context(void) {
  auto& locality = *(&CtvAccess(locality_));
  CkAssert(locality && "locality must be valid");
  return locality;
}
}

#endif
