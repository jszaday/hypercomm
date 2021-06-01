#ifndef __HYPERCOMM_CORE_PORT_OPENER_HPP__
#define __HYPERCOMM_CORE_PORT_OPENER_HPP__

#include "locality_base.hpp"

namespace hypercomm {
template<typename Index>
struct port_opener: public immediate_action<void(locality_base<Index>*)> {
  using index_type = array_proxy::index_type;
  using section_ptr = typename locality_base<Index>::section_ptr;

  entry_port_ptr port;
  callback_ptr cb;

  port_opener(PUP::reconstruct) {}

  port_opener(const entry_port_ptr& _1, const callback_ptr& _2)
  : port(_1), cb(_2) {}

  virtual void action(locality_base<Index>* _1) override {
    auto& locality = const_cast<locality_base<Index>&>(*_1);
    auto& fport = *(dynamic_cast<future_port*>(port.get()));
    locality.open(port, cb);
  }

  virtual void __pup__(serdes& s) override {
    s | port;
    s | cb;
  }
};
}

#endif
