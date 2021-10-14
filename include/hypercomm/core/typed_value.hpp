#ifndef __HYPERCOMM_CORE_TYPED_VALUE_HPP__
#define __HYPERCOMM_CORE_TYPED_VALUE_HPP__

#include "../serialization/construction.hpp"
#include "value.hpp"
#include "config.hpp"

namespace hypercomm {

using unit_type = std::tuple<>;

template <typename T>
class typed_value : public hyper_value {
 protected:
  // static constexpr auto is_contribution = std::is_same<contribution, T>::value;

  const void* storage;
  const storage_scheme scheme;

 public:
  using type = T;

  typed_value(const void* _1, const storage_scheme& _2)
      : hyper_value(true), storage(_1), scheme(_2) {}

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

// template <>
// class typed_value<void> {};

// template <typename T, storage_scheme Scheme = kInline>
// class typed_value_impl_ : public typed_value<T> {
//   static constexpr auto is_contribution = typed_value<T>::is_contribution;

//  public:
//   temporary<T, Scheme> tmp;

//   template <typename... Args>
//   typed_value_impl_(Args... args)
//       : typed_value<T>(&tmp, Scheme), tmp(std::forward<Args>(args)...) {}

//   virtual bool recastable(void) const override { return false; }

//   virtual void pup_buffer(serdes& s, const bool& encapsulate) override {
//     if (encapsulate) {
//       if (Scheme == kInline) {
//         std::shared_ptr<T> ptr(s.observe_source(), this->get());
//         s | ptr;
//       } else {
//         s | this->tmp;
//       }
//     } else {
//       s | this->value();
//     }
//   }

//   virtual hyper_value::message_type release(void) override {
//     auto msg = pack_to_port({}, this->value());
//     if (is_contribution) {
//       msg->set_redn(true);
//     }
//     return msg;
//   }
// };

template <typename T>
using typed_value_ptr = std::unique_ptr<typed_value<T>>;

// NOTE ( this would be implemented via ser/des )
template <typename T>
typed_value_ptr<T> msg2typed(CkMessage* msg) {
  CkFreeMsg(msg);
  return typed_value_ptr<T>(nullptr);
}

template <typename T, typename... Ts>
typed_value_ptr<T> make_typed_value(Ts... ts) {
  return typed_value_ptr<T>(new typed_value<T>(std::forward<Ts>(ts)...));
}

template <typename T>
typed_value_ptr<T> cast_value(value_ptr&& val) {
  return typed_value_ptr<T>(static_cast<typed_value<T>*>(val.release()));
}

template <typename T>
value_ptr cast_value(typed_value_ptr<T>&& val) {
  return value_ptr(static_cast<hyper_value*>(val.release()));
}

}  // namespace hypercomm

#endif
