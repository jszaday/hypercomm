#ifndef __HYPERCOMM_CORE_TYPED_VALUE_HPP__
#define __HYPERCOMM_CORE_TYPED_VALUE_HPP__

#include "../messaging/packing.hpp"
#include "../reductions/contribution.hpp"
#include "../serialization/is_pupable.hpp"

#include "config.hpp"

namespace hypercomm {

using unit_type = std::tuple<>;

template <typename T>
class typed_value : public hyper_value {
 protected:
  static constexpr auto is_contribution = std::is_same<contribution, T>::value;
  static constexpr CMK_REFNUM_TYPE flags = is_contribution
                                           << messaging::__attribs__::kRedn;

  const void* storage;
  const storage_scheme scheme;

 public:
  using type = T;

  typed_value(const void* _1, const storage_scheme& _2)
      : hyper_value(flags), storage(_1), scheme(_2) {}

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
  static std::unique_ptr<typed_value<T>> from_message(CkMessage* msg);

  template <typename... Args>
  inline static std::unique_ptr<typed_value<T>> from_buffer(Args... args);
};

namespace detail {
template <typename T, typename Enable = void>
struct pup_guard;

template <typename T>
struct pup_guard<T, typename std::enable_if<is_pupable<T>::value>::type> {
  template <storage_scheme Scheme>
  inline static void unpack(CkMessage* msg, temporary<T, Scheme>& t) {
    hypercomm::unpack(msg, t);
  }

  template <typename A>
  inline static void pup(serdes& s, A& a) {
    s | a;
  }

  template <typename A>
  inline static message* pack_to_port(const entry_port_ptr& dst, A& a) {
    return hypercomm::pack_to_port(dst, a);
  }
};

template <typename T>
class pup_guard<T, typename std::enable_if<!is_pupable<T>::value>::type> {
  static void exit_ [[noreturn]] (void) {
    not_implemented("type %s is not pupable", typeid(T).name());
  }

 public:
  template <storage_scheme Scheme>
  inline static void unpack(CkMessage*, temporary<T, Scheme>&) {
    exit_();
  }

  template <typename A>
  inline static void pup(serdes&, A&) {
    exit_();
  }

  template <typename A>
  inline static message* pack_to_port(const entry_port_ptr&, A&) {
    exit_();
  }
};

template <typename T, typename Enable = void>
struct encapsulate_pup_;
}  // namespace detail

template <typename T, storage_scheme Scheme = kInline>
class typed_value_impl_ : public typed_value<T> {
  using guard_t = detail::pup_guard<T>;
  static constexpr auto is_contribution = typed_value<T>::is_contribution;

  template <typename Type, typename Enable>
  friend class detail::encapsulate_pup_;

 public:
  temporary<T, Scheme> tmp;

  template <typename... Args>
  typed_value_impl_(Args... args)
      : typed_value<T>(&tmp, Scheme), tmp(std::forward<Args>(args)...) {}

  virtual bool pup_buffer(serdes& s, bool encapsulate) override;

  // TODO ( deprecate this? )
  virtual message* as_message(void) const override {
    auto msg = guard_t::pack_to_port({}, this->value());
    CkSetRefNum(msg, this->flags);
    return msg;
  }
};

namespace detail {

template <typename T>
using not_shared_ptr_t = typename std::enable_if<
    !is_specialization_of<std::shared_ptr, T>::value>::type;

template <typename T>
struct encapsulate_pup_<typed_value_impl_<T, kInline>, not_shared_ptr_t<T>> {
  using guard_t = typename typed_value_impl_<T, kInline>::guard_t;
  inline static void pup(serdes& s, typed_value_impl_<T, kInline>* self) {
    std::shared_ptr<T> ptr(s.observe_source(), self->get());
    guard_t::pup(s, ptr);
  }
};

template <typename T>
struct encapsulate_pup_<typed_value_impl_<T, kBuffer>, not_shared_ptr_t<T>> {
  using guard_t = typename typed_value_impl_<T, kBuffer>::guard_t;
  inline static void pup(serdes& s, typed_value_impl_<T, kBuffer>* self) {
    guard_t::pup(s, self->tmp);
  }
};

template <typename T, storage_scheme Scheme>
struct encapsulate_pup_<typed_value_impl_<std::shared_ptr<T>, Scheme>> {
  using guard_t =
      typename typed_value_impl_<std::shared_ptr<T>, Scheme>::guard_t;
  inline static void pup(serdes& s,
                         typed_value_impl_<std::shared_ptr<T>, Scheme>* self) {
    guard_t::pup(s, self->value());
  }
};
}  // namespace detail

template <typename T, storage_scheme Scheme>
bool typed_value_impl_<T, Scheme>::pup_buffer(serdes& s, bool encapsulate) {
  if (encapsulate) {
    detail::encapsulate_pup_<typed_value_impl_<T, Scheme>>::pup(s, this);
  } else {
    guard_t::pup(s, this->value());
  }
  return encapsulate && !(is_specialization_of<std::shared_ptr, T>::value);
}

template <typename T>
template <typename... Args>
inline std::unique_ptr<typed_value<T>> typed_value<T>::from_buffer(
    Args... args) {
  return make_value<typed_value_impl_<T, kBuffer>>(
      tags::use_buffer<T>(std::forward<Args>(args)...));
}

template <typename T>
template <storage_scheme Scheme>
std::unique_ptr<typed_value<T>> typed_value<T>::from_message(CkMessage* msg) {
  if (utilities::is_null_message(msg)) {
    return std::unique_ptr<typed_value<T>>();
  } else if (!is_contribution && utilities::is_reduction_message(msg)) {
    CkMessage* imsg;
    unpack(msg, imsg);
    return from_message(imsg);
  } else {
    auto result = make_value<typed_value_impl_<T, Scheme>>(tags::no_init{});
    detail::pup_guard<T>::unpack(msg, result->tmp);
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

  // delete the zc value since it's no longer reqd
  delete value;

  return std::move(result);
}

template <typename T>
inline typed_value_ptr<T> value2typed(hyper_value* ptr) {
#if CMK_ERROR_CHECKING
  auto* val = dynamic_cast<typed_value<T>*>(ptr);
  if (ptr && !val) {
    CkAbort("could not cast %s to %s\n", typeid(*ptr).name(), typeid(T).name());
  }
#else
  auto* val = static_cast<typed_value<T>*>(ptr);
#endif
  return typed_value_ptr<T>(val);
}

template <typename T>
inline std::unique_ptr<typed_value<T>> dev2typed(deliverable&& dev) {
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
        typed->source = std::move(ep);
      }
      return std::move(typed);
    }
    case deliverable::kValue: {
      auto& ep = dev.endpoint();
      auto* val = dev.release<hyper_value>();
      if (val == nullptr) {
        return typed_value_ptr<T>();
      } else {
        val->source = std::move(ep);
        return value2typed<T>(val);
      }
    }
    default: {
      not_implemented("unrecognized deliverable kind!");
    }
  }
}

namespace core {

template <typename... Args>
void typed_callback<Args...>::send(deliverable&& dev) {
  this->send(dev2typed<tuple_type>(std::move(dev)));
}
}  // namespace core
}  // namespace hypercomm

#endif
