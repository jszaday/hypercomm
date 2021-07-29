#ifndef __HYPERCOMM_CORE_FWDCB_HPP__
#define __HYPERCOMM_CORE_FWDCB_HPP__

#include "common.hpp"

namespace hypercomm {

template <typename Index>
struct forwarding_callback : public core::callback {
  element_ptr<Index> proxy;
  entry_port_ptr port;

  forwarding_callback(PUP::reconstruct) {}

  forwarding_callback(const element_ptr<Index>& _1, const entry_port_ptr& _2)
      : proxy(_1), port(_2) {}

  virtual void send(core::callback::value_type&&) override;

  virtual void __pup__(serdes& s) override {
    s | proxy;
    s | port;
  }
};

template <typename Index>
inline callback_ptr forward_to(const element_ptr<Index>& elt,
                               const entry_port_ptr& port) {
  return std::make_shared<forwarding_callback<Index>>(elt, port);
}

template <typename Proxy>
inline callback_ptr forward_to(const Proxy& elt, const entry_port_ptr& port) {
  using proxy_type = element_ptr<impl_index_t<Proxy>>;
  return forward_to((proxy_type)make_proxy(elt), port);
}
}

#endif
