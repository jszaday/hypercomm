#ifndef __HYPERCOMM_COMPONENTS_CONNECTOR_HPP__
#define __HYPERCOMM_COMPONENTS_CONNECTOR_HPP__

#include <cstdint>
#include "../serialization/construction.hpp"
#include "../messaging/destination.hpp"

namespace hypercomm {

namespace detail {
template <typename T>
struct inv_;

template <typename T>
struct inv_<typed_value_ptr<T>> {
  static typed_value_ptr<T> get(void) { return typed_value_ptr<T>(); }
};

template <>
struct inv_<deliverable> {
  static deliverable get(void) { return deliverable((hyper_value*)nullptr); }
};
}  // namespace detail

namespace components {
template <typename T>
class connector_ {
  using storage_t = typename std::aligned_storage<sizeof(destination),
                                                  alignof(destination)>::type;
  storage_t storage_;
  destination* dst_;

 public:
  connector_(void) : dst_(nullptr) {}

  template <typename... Args>
  connector_(Args&&... args) : dst_((destination*)&storage_) {
    new (dst_) destination(std::forward<Args>(args)...);
  }

  ~connector_() {
    if (dst_) {
      dst_->~destination();
      dst_ = nullptr;
    }
  }

  // assumes T is a variant of unique_ptr
  inline bool relay(T& value) const {
    deliverable dev(std::move(value));
    if (passthru_context_(*(this->dst_), dev)) {
      return true;
    } else {
      using element_type = typename T::element_type;
      value.reset(static_cast<element_type*>(dev.release<hyper_value>()));
      return false;
    }
  }

  inline bool ready(void) const { return this->dst_ != nullptr; }

  inline void implode(void) {
    auto inv = detail::inv_<T>::get();
    this->relay(inv);
    this->~connector_();
  }
};

template <>
inline bool connector_<deliverable>::relay(deliverable& dev) const {
  return passthru_context_(*(this->dst_), dev);
}
}  // namespace components
}  // namespace hypercomm

#endif
