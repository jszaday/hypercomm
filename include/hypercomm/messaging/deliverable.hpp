#ifndef __HYPERCOMM_MESSAGING_DELIVERABLE_HPP__
#define __HYPERCOMM_MESSAGING_DELIVERABLE_HPP__

#include "../core/value.hpp"
#include "../core/zero_copy_value.hpp"
#include "../serialization/construction.hpp"

namespace hypercomm {

struct deliverable {
  enum kind_ { kMessage, kValue, kDeferred };

  kind_ kind;

 private:
  template <typename T>
  struct kind_for_;

  void* storage_;

  entry_port_ptr port_;

 public:
  deliverable(CkMessage* msg) : kind(kMessage), storage_(msg) {}

  deliverable(hyper_value* val) : kind(kValue), storage_(val) {}

  deliverable(const entry_port_ptr& port, hyper_value* val)
      : kind(kValue), storage_(val), port_(port) {}

  deliverable(const entry_port_ptr& port, value_ptr&& val)
      : deliverable(port, val.release()) {}

  deliverable(zero_copy_value* zc) : kind(kDeferred), storage_(zc) {}

  template <typename T>
  deliverable(std::unique_ptr<T>&& ptr) : deliverable(ptr.release()) {}

  ~deliverable() {
    switch (kind) {
      case kMessage:
        if (this->storage_) delete (CkMessage*)this->storage_;
        break;
      case kValue:
        if (this->storage_) delete (hyper_value*)this->storage_;
        break;
      case kDeferred:
        if (this->storage_) delete (zero_copy_value*)this->storage_;
        break;
    }
  }

  deliverable(const deliverable&) = delete;

  deliverable(deliverable&& other) : kind(other.kind) {
    switch (kind) {
      case kMessage:
        this->storage_ = other.release<CkMessage>();
        break;
      case kValue:
        this->storage_ = other.release<hyper_value>();
        break;
      case kDeferred:
        this->storage_ = other.release<zero_copy_value>();
        break;
    }
  }

  template <typename T>
  inline T* release(void);

  template <typename T>
  inline T* peek(void);

  inline entry_port_ptr& entry_port(void) {
    switch (kind) {
      case kMessage: {
        auto* msg = (CkMessage*)this->storage_;
        if (msg && (UsrToEnv(msg)->getMsgIdx() == message::index())) {
          return ((message*)msg)->dst;
        } else {
          return this->port_;
        }
      }
      case kDeferred:
        return const_cast<entry_port_ptr&>(
            ((zero_copy_value*)this->storage_)->ep.port_);
      case kValue:
        return this->port_;
      default:
        CkAbort("unreachable~!");
        return this->port_;
    }
  }

  inline operator bool(void) const { return (this->storage_ != nullptr); }
};

template <>
struct deliverable::kind_for_<CkMessage> {
  static constexpr auto value = kMessage;
};

template <>
struct deliverable::kind_for_<hyper_value> {
  static constexpr auto value = kValue;
};

template <>
struct deliverable::kind_for_<zero_copy_value> {
  static constexpr auto value = kDeferred;
};

template <typename T>
inline T* deliverable::release(void) {
  auto expected = kind_for_<T>::value;
  auto* old = (T*)this->storage_;
  CkAssert(old && (this->kind == expected));
  this->storage_ = nullptr;
  return old;
}

template <typename T>
inline T* deliverable::peek(void) {
  auto expected = kind_for_<T>::value;
  if (this->kind == expected) {
    return (T*)this->storage_;
  } else {
    return nullptr;
  }
}
}  // namespace hypercomm

#endif
