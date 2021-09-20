#ifndef __HYPERCOMM_CORE_TYPED_VALUE_HPP__
#define __HYPERCOMM_CORE_TYPED_VALUE_HPP__

#include "../reductions/contribution.hpp"
#include "../messaging/packing.hpp"
#include "config.hpp"
#include "value.hpp"

namespace hypercomm {

using unit_type = std::tuple<>;

template <typename T>
class typed_value : public hyper_value {
 protected:
  static constexpr auto is_contribution = std::is_same<contribution, T>::value;

  const void* storage;
  const storage_scheme scheme;

 public:
  using type = T;

  typed_value(const void* _1, const storage_scheme& _2)
      : storage(_1), scheme(_2) {}

  // avoids a costly virtual method dispatch
  inline T* get(void) noexcept {
    switch (scheme) {
      case kBuffer:
        return &(((temporary<T, kBuffer>*)storage)->value());
      case kInline:
        return &(((temporary<T, kInline>*)storage)->value());
      default:
        return nullptr;
    }
  }

  inline T& value(void) noexcept { return *(this->get()); }

  inline const T& value(void) const noexcept {
    return *(const_cast<typed_value<T>*>(this)->get());
  }

  inline T* operator->(void) noexcept { return this->get(); }

  template <storage_scheme Scheme = kInline>
  static std::unique_ptr<typed_value<T>> from_message(message_type msg);

  template <typename... Args>
  inline static std::unique_ptr<typed_value<T>> from_buffer(Args... args);
};

template <typename T, storage_scheme Scheme = kInline>
class typed_value_impl_ : public typed_value<T> {
  static constexpr auto is_contribution = typed_value<T>::is_contribution;

 public:
  temporary<T, Scheme> tmp;

  template <typename... Args>
  typed_value_impl_(Args... args)
      : typed_value<T>(&tmp, Scheme), tmp(std::forward<Args>(args)...) {}

  virtual bool recastable(void) const override { return false; }

  std::pair<const void*, std::size_t> try_zero_copy(void) const override {
    if (zero_copyable<T>::value) {
      auto& val = this->value();
      auto size = hypercomm::size(val);
      if (size >= kZeroCopySize) {
        return std::make_pair(&val, size);
      }
    }

    return std::make_pair(nullptr, 0x0);
  }

  virtual hyper_value::message_type release(void) override {
    auto msg = pack_to_port({}, this->value());
    if (is_contribution) {
      msg->set_redn(true);
    }
    return msg;
  }
};

template <typename T>
template <typename... Args>
inline std::unique_ptr<typed_value<T>> typed_value<T>::from_buffer(
    Args... args) {
  return make_value<typed_value_impl_<T, kBuffer>>(
      tags::use_buffer<T>(std::forward<Args>(args)...));
}

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

template <typename T, typename... Args>
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
    buffer_value* try_buff = (zero_copyable<T>::value)
                                 ? dynamic_cast<buffer_value*>(value)
                                 : nullptr;
    if (try_buff) {
      auto* payload = try_buff->payload<T>();
      auto typed =
          typed_value<T>::from_buffer(std::move(try_buff->buffer), payload);
      delete value;
      return std::move(typed);
    } else {
      auto typed = typed_value<T>::from_message(value->release());
      typed->source = value->source;
      delete value;
      return std::move(typed);
    }
  } else {
    CkAbort("invalid cast!");
  }
}
}  // namespace hypercomm

#endif
