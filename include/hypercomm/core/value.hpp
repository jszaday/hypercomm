#ifndef __HYPERCOMM_CORE_VALUE_HPP__
#define __HYPERCOMM_CORE_VALUE_HPP__

#include "../messaging/packing.hpp"

namespace hypercomm {

class hyper_value {
 public:
  using message_type = CkMessage*;
  virtual ~hyper_value() = default;
  virtual bool recastable(void) const = 0;
  virtual message_type release(void) = 0;
};

class plain_value : public hyper_value {
 public:
  message_type msg;

  plain_value(void) : msg(nullptr) {}

  plain_value(message_type _1) : msg(_1) {}

  ~plain_value() {
    if (msg != nullptr) {
      CkFreeMsg(msg);
    }
  }

  virtual bool recastable(void) const override { return msg != nullptr; }

  virtual message_type release(void) override {
    auto value = this->msg;
    this->msg = nullptr;
    return value;
  }
};

template <typename T>
class typed_value : public hyper_value {
 public:
  temporary<T> tmp;

  typed_value(const T& _1) : tmp(_1) {}

  typed_value(message_type msg) { unpack(msg, tmp.value()); }

  virtual bool recastable(void) const override { return false; }

  inline const T& value(void) const { return tmp.value(); }

  inline T& value(void) { return tmp.value(); }

  virtual message_type release(void) override {
    return pack_to_port({}, this->value());
  }
};

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

inline std::shared_ptr<plain_value> msg2value(
    typename hyper_value::message_type msg) {
  return std::make_shared<plain_value>(msg);
}

inline std::shared_ptr<plain_value> msg2value(
    std::shared_ptr<CkMessage>&& msg) {
  return std::make_shared<plain_value>(
      utilities::unwrap_message(std::move(msg)));
}

template <typename T, typename U>
class joining_value;

template <typename... Ts, typename... Us>
class joining_value<std::tuple<Ts...>, std::tuple<Us...>>
    : public typed_value<std::tuple<Ts..., Us...>> {
  inline static std::tuple<Ts..., Us...> join(
      std::shared_ptr<hyper_value>&& left,
      std::shared_ptr<hyper_value>&& right) {
    return std::tuple_cat(
        std::move(value2typed<std::tuple<Ts...>>(std::move(left))->value()),
        std::move(value2typed<std::tuple<Us...>>(std::move(right))->value()));
  }

 public:
  joining_value(std::shared_ptr<hyper_value>&& _1,
                std::shared_ptr<hyper_value>&& _2)
      : typed_value<std::tuple<Ts..., Us...>>(
            join(std::move(_1), std::move(_2))) {}
};
}

#endif
