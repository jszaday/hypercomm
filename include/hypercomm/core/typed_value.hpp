#ifndef __HYPERCOMM_CORE_TYPED_VALUE_HPP__
#define __HYPERCOMM_CORE_TYPED_VALUE_HPP__

#include "../reductions/contribution.hpp"
#include "../messaging/packing.hpp"
#include "value.hpp"

namespace hypercomm {

using unit_type = std::tuple<>;

template <typename T>
class typed_value : public hyper_value {
  static constexpr auto is_contribution = std::is_same<contribution, T>::value;

 public:
  using type = T;

  temporary<T> tmp;

  template <typename... Args>
  typed_value(Args... args) : tmp(std::forward<Args>(args)...) {}

  virtual bool recastable(void) const override { return false; }

  inline T& value(void) noexcept { return tmp.value(); }

  inline T* operator->(void) noexcept { return &(this->value()); }

  virtual message_type release(void) override {
    auto msg = pack_to_port({}, this->value());
    if (is_contribution) {
      msg->set_redn(true);
    }
    return msg;
  }

  static std::unique_ptr<typed_value<T>> from_message(message_type msg) {
    if (utilities::is_null_message(msg)) {
      return std::unique_ptr<typed_value<T>>();
    } else if (!is_contribution && utilities::is_reduction_message(msg)) {
      CkMessage* imsg;
      unpack(msg, imsg);
      return from_message(imsg);
    } else {
      auto result = make_value<typed_value<T>>(tags::no_init{});
      unpack(msg, result->value());
      return std::move(result);
    }
  }
};

inline std::unique_ptr<typed_value<unit_type>> make_unit_value(void) {
  return make_value<typed_value<unit_type>>(tags::no_init{});
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
