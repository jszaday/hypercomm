#ifndef __HYPERCOMM_CORE_TYPED_VALUE_HPP__
#define __HYPERCOMM_CORE_TYPED_VALUE_HPP__

#include "../reductions/contribution.hpp"
#include "../messaging/packing.hpp"
#include "value.hpp"

namespace hypercomm {

using unit_type = std::tuple<>;

template <typename T>
class typed_value : public hyper_value {
 protected:
  static constexpr auto is_contribution = std::is_same<contribution, T>::value;
 public:
  using type = T;
  virtual T* get(void) = 0;
  inline T& value(void) noexcept { return *(this->get()); }
  inline T* operator->(void) noexcept { return this->get(); }

  template<storage_scheme Scheme = kInline>
  static std::unique_ptr<typed_value<T>> from_message(message_type msg);
};

template <typename T, storage_scheme Scheme = kInline>
class typed_value_impl_ : public typed_value<T> {
  static constexpr auto is_contribution = typed_value<T>::is_contribution;
 public:
  temporary<T, Scheme> tmp;

  template <typename... Args>
  typed_value_impl_(Args... args) : tmp(std::forward<Args>(args)...) {}

  virtual T* get(void) override { return &(tmp.value()); }

  virtual bool recastable(void) const override { return false; }

  virtual hyper_value::message_type release(void) override {
    auto msg = pack_to_port({}, this->value());
    if (is_contribution) {
      msg->set_redn(true);
    }
    return msg;
  }
};


template <typename T> 
template <storage_scheme Scheme>
std::unique_ptr<typed_value<T>> typed_value<T>::from_message(message_type msg) {
  if (utilities::is_null_message(msg)) {
    return std::unique_ptr<typed_value<T>>();
  } else if (!is_contribution && utilities::is_reduction_message(msg)) {
    CkMessage* imsg;
    unpack(msg, imsg);
    return from_message(imsg);
  } else {
    auto result = make_value<typed_value_impl_<T, Scheme>>(tags::no_init{});
    unpack(msg, result->tmp);
    return std::move(result);
  }
}

template<typename T, typename... Args>
inline std::unique_ptr<typed_value<T>> make_typed_value(Args... args) {
  return make_value<typed_value_impl_<T>>(std::forward<Args>(args)...);
}

inline std::unique_ptr<typed_value<unit_type>> make_unit_value(void) {
  return make_typed_value<unit_type>(tags::no_init{});
}

template <typename T>
std::unique_ptr<typed_value<T>> value2typed(value_ptr&& ptr) {
  auto* value = ptr.release();
  auto* try_cast = dynamic_cast<typed_value<T>*>(value);
  if (try_cast) {
    return std::unique_ptr<typed_value<T>>(try_cast);
  } else if (value->recastable()) {
    auto typed = typed_value<T>::from_message(value->release());
    typed->source = value->source;
    delete value;
    return std::move(typed);
  } else {
    CkAbort("invalid cast!");
  }
}
}  // namespace hypercomm

#endif
