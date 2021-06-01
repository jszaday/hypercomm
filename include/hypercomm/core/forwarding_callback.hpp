#ifndef __HYPERCOMM_CORE_FWDCB_HPP__
#define __HYPERCOMM_CORE_FWDCB_HPP__

#include "callback.hpp"
#include "entry_port.hpp"
#include "../serialization/pup.hpp"

namespace hypercomm {

struct forwarding_callback : public core::callback {
  using proxy_ptr = std::shared_ptr<hypercomm::proxy>;
  using array_element_ptr = std::shared_ptr<array_element_proxy>;

  array_element_ptr proxy;
  entry_port_ptr port;

  forwarding_callback(PUP::reconstruct) {}

  forwarding_callback(const proxy_ptr& _1, const entry_port_ptr& _2)
        // TODO (make this more generic)
      : proxy(std::dynamic_pointer_cast<typename array_element_ptr::element_type>(_1)),
        port(_2) {}

  virtual void send(core::callback::value_type&&) override;

  virtual void __pup__(serdes& s) override {
    s | proxy;
    s | port;
  }
};

}

#endif
