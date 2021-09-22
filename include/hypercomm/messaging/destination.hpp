#ifndef __HYPERCOMM_MESSAGING_DESTINATION_HPP__
#define __HYPERCOMM_MESSAGING_DESTINATION_HPP__

#include "../core/common.hpp"
#include "../core/module.hpp"

namespace hypercomm {

class endpoint {
  inline static const int& demux(void) {
    return CkIndex_locality_base_::idx_demux_CkMessage();
  }

 public:
  const int idx_;
  const entry_port_ptr port_;

  endpoint(const int& _) : idx_(_), port_(nullptr) {}
  endpoint(const entry_port_ptr& _) : idx_(demux()), port_(_) {}

  inline bool valid(void) const { return this->port_ || (this->idx_ != demux()); }

  template <typename T>
  static constexpr bool constructible_from(void) {
    return std::is_constructible<endpoint, const T&>::value;
  }
};

template <typename T>
using is_valid_endpoint_t =
    typename std::enable_if<endpoint::constructible_from<T>()>::type;

class destination {
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

  ~destination() {
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

  destination(const callback_ptr& cb) : type(kCallback), options(cb) {}
  destination(const component_port_t& port)
      : type(kComponentPort), options(port) {}

  destination(const destination& dst) : type(dst.type) {
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
}  // namespace hypercomm

#endif
