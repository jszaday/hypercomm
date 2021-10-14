#ifndef __HYPERCOMM_MESSAGING_DESTINATION_HPP__
#define __HYPERCOMM_MESSAGING_DESTINATION_HPP__

#include "endpoint.hpp"
#include "deliverable.hpp"

namespace hypercomm {

class destination {
 public:
  enum kind_ { kCallback, kComponent, kEndpoint };

  kind_ kind;

 private:
  template <kind_ Kind>
  struct initializer_;

  union u_storage_ {
    generic_callback_ptr cb;
    component_port_pair com;
    endpoint ep;
    u_storage_(void) {}
    ~u_storage_() {}
  } storage_;

 public:
  template <typename T>
  destination(const callback_ptr<T>& cb);
  destination(const generic_callback_ptr& cb);
  destination(component_id_t com, component_port_t port);
  destination(const entry_port_ptr& port);
  destination(UShort epIdx, const entry_port_ptr& port = nullptr);

  static_assert(std::is_trivially_destructible<component_port_pair>::value,
                "expected pair to be trivially destructible!");

  ~destination() {
    switch (this->kind) {
      case kCallback:
        this->storage_.cb.~generic_callback_ptr();
        break;
      case kEndpoint:
        this->storage_.ep.~endpoint();
        break;
      default:
        break;
    }
  }
};

template <>
struct destination::initializer_<destination::kCallback> {
  template <typename... Args>
  static void initialize(destination* self, Args... args) {
    self->kind = kCallback;
    new (&(self->storage_.cb))
        generic_callback_ptr(std::forward<Args>(args)...);
  }
};

template <>
struct destination::initializer_<destination::kComponent> {
  template <typename... Args>
  static void initialize(destination* self, Args... args) {
    self->kind = kComponent;
    new (&(self->storage_.com))
        component_port_pair(std::forward<Args>(args)...);
  }
};

template <>
struct destination::initializer_<destination::kEndpoint> {
  template <typename... Args>
  static void initialize(destination* self, Args... args) {
    self->kind = kEndpoint;
    new (&(self->storage_.ep)) endpoint(std::forward<Args>(args)...);
  }
};

destination::destination(const generic_callback_ptr& cb) {
  initializer_<kCallback>::initialize(this, cb);
}

template <typename T>
destination::destination(const callback_ptr<T>& cb) {
  initializer_<kCallback>::initialize(this, cb);
}

destination::destination(component_id_t com, component_port_t port) {
  initializer_<kComponent>::initialize(this, com, port);
}

destination::destination(UShort epIdx, const entry_port_ptr& port) {
  initializer_<kEndpoint>::initialize(this, epIdx, port);
}

destination::destination(const entry_port_ptr& port) {
  initializer_<kEndpoint>::initialize(this, port);
}
}  // namespace hypercomm

#endif
