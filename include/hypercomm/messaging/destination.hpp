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
    callback_ptr cb;
    com_port_pair_t com;
    endpoint ep;
    u_storage_(void) {}
    ~u_storage_() {}
  } storage_;

 public:
  destination(const callback_ptr& cb);
  destination(component_id_t com, component_port_t port);
  destination(const com_port_pair_t& pair)
      : destination(pair.first, pair.second) {}
  destination(const entry_port_ptr& port);
  destination(UShort epIdx, const entry_port_ptr& port = nullptr);

  destination(destination&& other);
  destination(const destination& other);

  destination& operator=(const destination& other) {
    if (this != &other) {
      this->~destination();
      new (this) destination(other);
    }

    return *this;
  }

  static_assert(std::is_trivially_destructible<com_port_pair_t>::value,
                "expected pair to be trivially destructible!");

  inline operator bool(void) const {
    switch (this->kind) {
      case kCallback:
        return (bool)this->cb();
      case kEndpoint:
        return (bool)this->ep();
      case kComponent:
        return true;
      default:
        return false;
    }
  }

  ~destination() {
    switch (this->kind) {
      case kCallback:
        this->storage_.cb.~callback_ptr();
        break;
      case kEndpoint:
        this->storage_.ep.~endpoint();
        break;
      default:
        break;
    }
  }

  inline const callback_ptr& cb(void) const {
    CkAssert(this->kind == kCallback);
    return this->storage_.cb;
  }

  inline const endpoint& ep(void) const {
    CkAssert(this->kind == kEndpoint);
    return this->storage_.ep;
  }

  inline const com_port_pair_t& com_port(void) const {
    CkAssert(this->kind == kComponent);
    return this->storage_.com;
  }

  inline std::string to_string(void) const {
    std::stringstream ss;
    ss << "destination(";
    switch (this->kind) {
      case kCallback:
        ss << "cb";
        break;
      case kEndpoint:
        ss << this->ep().to_string();
        break;
      case kComponent:
        auto& dst = this->com_port();
        ss << "com=(" << dst.first << "," << dst.second <<")";
        break;
      default:
        ss << "???";
        break;
    }
    ss << ")";
    return ss.str();
  }
};

template <>
struct destination::initializer_<destination::kCallback> {
  template <typename... Args>
  inline static void initialize(destination* self, Args&&... args) {
    self->kind = kCallback;
    new (&(self->storage_.cb)) callback_ptr(std::forward<Args>(args)...);
    CkEnforce(self->kind == kCallback);
  }
};

template <>
struct destination::initializer_<destination::kComponent> {
  template <typename... Args>
  inline static void initialize(destination* self, Args&&... args) {
    self->kind = kComponent;
    new (&(self->storage_.com)) com_port_pair_t(std::forward<Args>(args)...);
  }
};

template <>
struct destination::initializer_<destination::kEndpoint> {
  template <typename... Args>
  inline static void initialize(destination* self, Args&&... args) {
    self->kind = kEndpoint;
    new (&(self->storage_.ep)) endpoint(std::forward<Args>(args)...);
  }
};

inline destination::destination(const callback_ptr& cb) {
  initializer_<kCallback>::initialize(this, cb);
}

inline destination::destination(component_id_t com, component_port_t port) {
  initializer_<kComponent>::initialize(this, com, port);
}

inline destination::destination(UShort epIdx, const entry_port_ptr& port) {
  initializer_<kEndpoint>::initialize(this, epIdx, port);
}

inline destination::destination(const entry_port_ptr& port) {
  initializer_<kEndpoint>::initialize(this, port);
}

inline destination::destination(destination&& other) {
  switch (other.kind) {
    case kCallback:
      initializer_<kCallback>::initialize(this, std::move(other.storage_.cb));
      break;
    case kEndpoint:
      initializer_<kEndpoint>::initialize(this, std::move(other.storage_.ep));
      break;
    case kComponent:
      initializer_<kComponent>::initialize(this, other.storage_.com);
      break;
    default:
      break;
  }
}

inline destination::destination(const destination& other) {
  switch (other.kind) {
    case kCallback:
      initializer_<kCallback>::initialize(this, other.storage_.cb);
      break;
    case kEndpoint:
      initializer_<kEndpoint>::initialize(this, other.storage_.ep);
      break;
    case kComponent:
      initializer_<kComponent>::initialize(this, other.storage_.com);
      break;
    default:
      break;
  }
}
}  // namespace hypercomm

#endif
