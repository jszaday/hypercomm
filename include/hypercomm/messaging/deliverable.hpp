#ifndef __HYPERCOMM_MESSAGING_DELIVERABLE_HPP__
#define __HYPERCOMM_MESSAGING_DELIVERABLE_HPP__

#include "../serialization/construction.hpp"
#include "../core/zero_copy_value.hpp"
#include "../core/value.hpp"
#include "endpoint.hpp"

namespace hypercomm {

struct delivery;

struct deliverable {
  friend class delivery;

  enum kind_ { kMessage, kValue, kDeferred };

  kind_ kind;

 private:
  template <typename T>
  struct kind_for_;

  void* storage_;

  endpoint ep_;

 public:
  deliverable(CkMessage* msg) : kind(kMessage), storage_(msg) {}

  deliverable(zero_copy_value* zc) : kind(kDeferred), storage_(zc) {}

  template <typename... Args>
  deliverable(hyper_value* val, Args... args)
      : kind(kValue), storage_(val), ep_(args...) {}

  template <typename T>
  deliverable(std::unique_ptr<T>&& ptr) : deliverable(ptr.release()) {}

  template <typename T, typename... Args>
  deliverable(std::unique_ptr<T>&& ptr, Args... args)
      : deliverable(ptr.release(), std::forward<Args>(args)...) {}

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

  deliverable(deliverable&& other)
      : kind(other.kind), storage_(other.storage_), ep_(std::move(other.ep_)) {
    other.storage_ = nullptr;
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
          return this->get_port_();
        }
      }
      case kDeferred:
        return const_cast<entry_port_ptr&>(
            ((zero_copy_value*)this->storage_)->ep.port_);
      case kValue:
        return this->get_port_();
      default:
        CkAbort("unreachable~!");
        return this->get_port_();
    }
  }

  inline operator bool(void) const { return (this->storage_ != nullptr); }

  static CkMessage* to_message(deliverable& dev);

 private:
  inline entry_port_ptr& get_port_(void) {
    return const_cast<entry_port_ptr&>((this->ep_).port_);
  }
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
