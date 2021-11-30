#ifndef __HYPERCOMM_UTILITIES_UNSHARED_HPP__
#define __HYPERCOMM_UTILITIES_UNSHARED_HPP__

#include <memory>
#include "../serialization/construction.hpp"

namespace hypercomm {
namespace utilities {

// a bridge between shared and unique ptrs
// that can be released to shared ptrs?
template <typename T>
class unshared_ptr {
 public:
  enum kind_type { kUnique, kShared, kNil };
  using element_type = T;

 private:
  kind_type kind_;

  union u_storage_ {
    std::shared_ptr<T> shared_;
    std::unique_ptr<T> unique_;
    u_storage_(tags::no_init) {}
    u_storage_(const std::shared_ptr<T>& _) : shared_(_) {}
    u_storage_(std::unique_ptr<T>&& _)
        : unique_(std::forward<std::unique_ptr<T>>(_)) {}
    ~u_storage_() {}
  } storage_;

 public:
  unshared_ptr(void) : kind_(kNil), storage_(tags::no_init()) {}

  unshared_ptr(element_type* _) : unshared_ptr(std::unique_ptr<T>(_)) {}

  unshared_ptr(std::unique_ptr<T>&& _)
      : kind_(kUnique), storage_(std::forward<std::unique_ptr<T>>(_)) {}

  unshared_ptr(const std::shared_ptr<T>& _) : kind_(kShared), storage_(_) {}

  unshared_ptr(unshared_ptr<T>&& _)
      : kind_(_.kind_), storage_(tags::no_init()) {
    switch (this->kind_) {
      case kUnique:
        new (&this->storage_) u_storage_(std::move(_.storage_.unique_));
        break;

      case kShared:
        new (&this->storage_) u_storage_(std::move(_.storage_.shared_));
        break;

      default:
        break;
    }
  }

  ~unshared_ptr() {
    switch (this->kind_) {
      case kUnique:
        this->storage_.unique_.~unique_ptr();
        break;

      case kShared:
        this->storage_.shared_.~shared_ptr();
        break;

      default:
        break;
    }
  }

  inline unshared_ptr<T>& operator=(unshared_ptr<T>&& _) {
    this->~unshared_ptr();
    new (this) unshared_ptr<T>(std::move(_));
    return *this;
  }

  inline std::shared_ptr<T> release(void) {
    switch (this->kind_) {
      case kUnique:
        return std::shared_ptr<T>(this->storage_.unique_.release());
      case kShared:
        return std::move(this->storage_.shared_);
      default:
        return {};
    }
  }

  operator bool(void) const { return (this->operator->() != nullptr); }

  element_type* operator->(void) const {
    switch (this->kind_) {
      case kUnique:
        return this->storage_.unique_.get();
      case kShared:
        return this->storage_.shared_.get();
      default:
        return nullptr;
    }
  }

  element_type& operator*(void) const { return *(this->operator->()); }
};

}  // namespace utilities
}  // namespace hypercomm

#endif
