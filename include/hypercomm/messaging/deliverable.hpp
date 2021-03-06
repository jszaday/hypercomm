#ifndef __HYPERCOMM_MESSAGING_DELIVERABLE_HPP__
#define __HYPERCOMM_MESSAGING_DELIVERABLE_HPP__

#include "../serialization/construction.hpp"
#include "../core/zero_copy_value.hpp"

namespace hypercomm {

struct delivery;

struct deliverable {
  friend class delivery;

  enum kind_ { kInvalid, kMessage, kValue, kDeferred };

  kind_ kind;

 private:
  template <typename T>
  struct kind_for_;

  void* storage_;

  hypercomm::endpoint ep_;

 public:
  deliverable(void)
      : kind(kInvalid), storage_(nullptr), ep_(tags::reconstruct()) {}

  deliverable(CkMessage* msg) : kind(kMessage), storage_(msg), ep_(msg) {}

  deliverable(zero_copy_value* zc)
      : kind(kDeferred), storage_(zc), ep_(std::move(zc->ep)) {}

  template <typename... Args>
  deliverable(hyper_value* val)
      : kind(kValue), storage_(val), ep_(std::move(val->source)) {}

  template <typename T>
  deliverable(std::unique_ptr<T>&& ptr) : deliverable(ptr.release()) {}

  template <typename T, typename... Args>
  deliverable(std::unique_ptr<T>&& ptr, Args&&... args);

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
      case kInvalid:
        break;
      default:
        not_implemented("unrecognized deliverable kind!");
    }
  }

  deliverable(const deliverable&) = delete;

  deliverable(deliverable&& other)
      : kind(other.kind), storage_(other.storage_), ep_(std::move(other.ep_)) {
    other.invalidate_();
  }

  deliverable& operator=(deliverable&& other) {
    if (this != &other) {
      if (other.ep_) {
        this->ep_ = std::move(other.ep_);
      }
      this->storage_ = other.storage_;
      this->kind = other.kind;
      other.invalidate_();
    }
    return *this;
  }

  template <typename T>
  inline T* release(void);

  template <typename T>
  inline T* peek(void);

  inline hypercomm::endpoint& endpoint(void) { return this->ep_; }

  template <typename... Args>
  inline void update_endpoint(Args&&... args) {
    hypercomm::endpoint ep(std::forward<Args>(args)...);
    this->ep_ = std::move(ep);
  }

  inline operator bool(void) const {
    switch (this->kind) {
      case kDeferred:
        return this->storage_ && ((zero_copy_value*)this->storage_)->ready();
      case kMessage:
        return this->storage_;
      case kValue:
        return true;
      default:
        return false;
    }
  }

  static CkMessage* to_message(deliverable&& dev);

  inline std::string to_string(void) const {
    std::stringstream ss;
    ss << "deliverable(";
    switch (this->kind) {
      case kDeferred:
        ss << "zc=";
        break;
      case kMessage:
        ss << "msg=";
        break;
      case kValue:
        ss << "val=";
        break;
      default:
        ss << "???";
        break;
    }
    ss << this->storage_ << ")";
    return ss.str();
  }

 private:
  inline void invalidate_(void) {
    this->kind = kInvalid;
    this->storage_ = nullptr;
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

template <typename T>
struct deliverable::kind_for_<typed_value<T>> {
  static constexpr auto value = kValue;
};

template <>
struct deliverable::kind_for_<zero_copy_value> {
  static constexpr auto value = kDeferred;
};

template <typename T, typename... Args>
inline deliverable::deliverable(std::unique_ptr<T>&& ptr, Args&&... args)
    : kind(kind_for_<T>::value),
      storage_(ptr.release()),
      ep_(std::forward<Args>(args)...) {}

template <typename T>
inline T* deliverable::release(void) {
  auto expected = kind_for_<T>::value;
  auto* old = (T*)this->storage_;
  CkAssert((this->kind == expected) && (old || (this->kind == kValue)));
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
