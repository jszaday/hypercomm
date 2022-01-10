#ifndef __HYPERCOMM_CORE_FWDCB_HPP__
#define __HYPERCOMM_CORE_FWDCB_HPP__

#include "common.hpp"

namespace hypercomm {

template <typename Index>
struct forwarding_callback;

template <>
struct forwarding_callback<CkArrayIndex> : public core::callback {
  element_ptr<CkArrayIndex> proxy;
  endpoint ep;

  forwarding_callback(tags::reconstruct tag) : ep(tag) {}

  template <typename T>
  forwarding_callback(const element_ptr<CkArrayIndex>& _1, const T& _2)
      : proxy(_1), ep(_2) {}

  virtual void send(core::callback::value_type&&) override;

  virtual void __pup__(serdes& s) override {
    s | this->proxy;
    s | this->ep.idx_;
    s | this->ep.port_;
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
}  // namespace hypercomm

#endif
