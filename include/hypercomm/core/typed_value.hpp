#ifndef __HYPERCOMM_CORE_TYPED_VALUE_HPP__
#define __HYPERCOMM_CORE_TYPED_VALUE_HPP__

#include "../messaging/packing.hpp"
#include "../reductions/contribution.hpp"
#include "../serialization/is_pupable.hpp"

#include "deliverable_value.hpp"
#include "config.hpp"

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

template <typename T, storage_scheme Scheme = kInline>
class typed_value_impl_ : public typed_value<T> {
  static constexpr auto is_contribution = typed_value<T>::is_contribution;

 public:
  temporary<T, Scheme> tmp;

  template <typename... Args>
  typed_value_impl_(Args... args)
      : typed_value<T>(&tmp, Scheme), tmp(std::forward<Args>(args)...) {}

  virtual bool recastable(void) const override { return false; }

  virtual void pup_buffer(serdes& s, const bool& encapsulate) override {
    if (encapsulate) {
      if (Scheme == kInline) {
        std::shared_ptr<T> ptr(s.observe_source(), this->get());
        s | ptr;
      } else {
        s | this->tmp;
      }
    } else {
      s | this->value();
    }
  }

  virtual CMK_REFNUM_TYPE flags(void) override {
    return (is_contribution << messaging::__attribs__::kRedn);
  }

  virtual hyper_value::message_type release(void) override {
    auto msg = pack_to_port({}, this->value());
    CkSetRefNum(msg, this->flags());
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
std::unique_ptr<typed_value<T>> zero_copy_value::to_typed(
    zero_copy_value* value) {
  auto src = std::shared_ptr<message>(value->msg);
  auto result = make_value<typed_value_impl_<T, kBuffer>>(tags::no_init{});

  unpacker s(src, value->offset, true);
  result->pup_buffer(s, true);
  auto n_deferred = s.n_deferred();

  CkAssertMsg(n_deferred == value->values.size(),
              "deferred counts did not match!");
  for (auto i = 0; i < n_deferred; i++) {
    s.reset_deferred(i, std::move(value->values[i]));
  }

  return std::move(result);
}

template <typename T>
std::unique_ptr<typed_value<T>> dev2typed(
    deliverable&& dev, std::shared_ptr<value_source>&& src = {});

template <typename T>
std::unique_ptr<typed_value<T>> value2typed(value_ptr&& ptr) {
  auto* value = ptr.get();
  if (typed_value<T>* p1 = dynamic_cast<typed_value<T>*>(value)) {
    return std::unique_ptr<typed_value<T>>((typed_value<T>*)ptr.release());
  } else if (deliverable_value* p2 = dynamic_cast<deliverable_value*>(value)) {
    return dev2typed<T>(std::move(p2->dev), std::move(p2->source));
  } else {
    CkAbort("fatal> cannot convert %s to %s.\n",
            ptr ? typeid(ptr.get()).name() : "(nil)",
            typeid(typed_value<T>).name());
  }
}

template <typename T>
std::unique_ptr<typed_value<T>> dev2typed(deliverable&& dev,
                                          std::shared_ptr<value_source>&& src) {
  switch (dev.kind) {
    case deliverable::kDeferred: {
      auto* zc = dev.release<zero_copy_value>();
      CkAssert(zc->ready());
      // TODO ( update source )
      return zero_copy_value::to_typed<T>(zc);
    }
    case deliverable::kMessage: {
      auto& ep = dev.endpoint();
      auto* msg = dev.release<CkMessage>();
      auto typed = typed_value<T>::from_message(msg);
      if (typed) {
        typed->source = src ? std::move(src)
                            : std::make_shared<endpoint_source>(std::move(ep));
      }
      return std::move(typed);
    }
    case deliverable::kValue: {
      auto& ep = dev.endpoint();
      auto* val = dev.release<hyper_value>();
      val->source = src ? std::move(src)
                        : std::make_shared<endpoint_source>(std::move(ep));
      return value2typed<T>(value_ptr(val));
    }
    default: {
      NOT_IMPLEMENTED;
      return {};
    }
  }
}

namespace core {

template <typename Tuple, typename Enable = void>
struct pup_guard;

template <typename... Args>
struct pup_guard<
    std::tuple<Args...>,
    typename std::enable_if<is_pupable<std::tuple<Args...>>::value>::type> {
  using tuple_type = std::tuple<Args...>;

  static typed_value_ptr<tuple_type> unpack(deliverable&& dev) {
    return dev2typed<tuple_type>(std::move(dev));
  }
};

template <typename... Args>
struct pup_guard<
    std::tuple<Args...>,
    typename std::enable_if<!is_pupable<std::tuple<Args...>>::value>::type> {
  using tuple_type = std::tuple<Args...>;

  static typed_value_ptr<tuple_type> unpack(deliverable&& dev) {
    NOT_IMPLEMENTED;
  }
};

template <typename... Args>
void typed_callback<Args...>::send(deliverable&& dev) {
  this->send(pup_guard<tuple_type>::unpack(std::move(dev)));
}
}  // namespace core
}  // namespace hypercomm

#endif
