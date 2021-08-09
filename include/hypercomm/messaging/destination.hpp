#ifndef __HYPERCOMM_MESSAGING_DESTINATION_HPP__
#define __HYPERCOMM_MESSAGING_DESTINATION_HPP__

#include "../core/common.hpp"

namespace hypercomm {
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
