#ifndef __HYPERCOMM_CORE_TYPED_VALUE_HPP__
#define __HYPERCOMM_CORE_TYPED_VALUE_HPP__

#include "../messaging/packing.hpp"

#include "value.hpp"

namespace hypercomm {

using unit_type = std::tuple<>;

template <typename T>
class typed_value : public hyper_value {
 public:
  temporary<T> tmp;

  typed_value(const T& _1) : tmp(_1) {}

  typed_value(message_type msg) {
    if (msg) unpack(msg, tmp.value());
    else if (!std::is_same<unit_type, T>::value) {
      CkAbort("null pointer exception!");
    }
  }

  virtual bool recastable(void) const override { return false; }

  inline const T& value(void) const { return tmp.value(); }

  inline T& value(void) { return tmp.value(); }

  virtual message_type release(void) override {
    return pack_to_port({}, this->value());
  }
};

inline std::shared_ptr<typed_value<unit_type>> make_unit_value(void) {
  return std::make_shared<typed_value<unit_type>>(nullptr);
}

template <typename T>
std::shared_ptr<typed_value<T>> value2typed(
    std::shared_ptr<hyper_value>&& value) {
  auto try_cast = std::dynamic_pointer_cast<typed_value<T>>(value);
  if (try_cast) {
    return try_cast;
  } else if (value->recastable()) {
    return std::make_shared<typed_value<T>>(std::move(value->release()));
  } else {
    throw std::runtime_error("invalid cast!");
  }
}
}

#endif