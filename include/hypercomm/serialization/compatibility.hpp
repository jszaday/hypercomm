#ifndef __HYPERCOMM_COMPATIBILITY_HPP__
#define __HYPERCOMM_COMPATIBILITY_HPP__

#include <charm++.h>
#include "pup.hpp"

namespace hypercomm {
template <class T, typename Enable = void>
struct built_in {
  enum { value = false };
};

template <>
struct built_in<PUP::able*> {
  enum { value = true };
};

template <>
struct built_in<CkArrayIndex> {
  enum { value = true };
};

template <>
struct built_in<CkCallback> {
  enum { value = true };
};

template <>
struct built_in<CkArrayID> {
  enum { value = true };
};

template <>
struct built_in<std::string> {
  enum { value = true };
};

template <typename T>
struct built_in<
    T, typename std::enable_if<std::is_base_of<CProxy, T>::value>::type> {
  enum { value = true };
};

template <class T>
struct is_message<
    T, typename std::enable_if<std::is_base_of<CkMessage, T>::value>::type> {
  enum { value = true };
};

using serdes_state = serdes::state_t;

template <serdes_state>
struct puper_for;

template <>
struct puper_for<serdes_state::SIZING> {
  using type = PUP::sizer;
};

template <>
struct puper_for<serdes_state::PACKING> {
  using type = PUP::toMem;
};

template <>
struct puper_for<serdes_state::UNPACKING> {
  using type = PUP::fromMem;
};

template <>
struct puper<chare_t> {
  using impl_type = std::underlying_type<chare_t>::type;

  inline static void impl(serdes& s, chare_t& t) {
    auto& impl = reinterpret_cast<impl_type&>(t);
    auto smol = (std::uint8_t)impl;  // shrink the enum

    s | smol;

    if (s.unpacking()) {
      impl = (impl_type)smol;
    }
  }
};

template <class T>
struct is_polymorphic<
    T, typename std::enable_if<std::is_base_of<PUP::able, T>::value>::type> {
  enum { value = true };
};

template <typename T>
void interpup(PUP::er& p, T& t) {
  if (typeid(p) == typeid(PUP::fromMem)) {
    auto& mem = *static_cast<PUP::fromMem*>(&p);
    unpacker s(nullptr, mem.get_current_pointer());
    pup(s, t);
    mem.advance(s.size());
  } else if (typeid(p) == typeid(PUP::toMem)) {
    auto& mem = *static_cast<PUP::toMem*>(&p);
    packer s(mem.get_current_pointer());
    pup(s, t);
    mem.advance(s.size());
  } else if (typeid(p) == typeid(PUP::sizer)) {
    p(static_cast<char*>(nullptr), size(t));
  } else {
    CkAbort("unsure how to convert an %s into a serdes", typeid(p).name());
  }
}

using pup_ptr = std::unique_ptr<PUP::er>;
inline std::unique_ptr<PUP::er> make_puper(const serdes& s) {
  switch (s.state) {
    case serdes_state::UNPACKING: {
      using pup_type = typename puper_for<serdes_state::UNPACKING>::type;
      return pup_ptr(new pup_type(s.current));
    }
    case serdes_state::PACKING: {
      using pup_type = typename puper_for<serdes_state::PACKING>::type;
      return pup_ptr(new pup_type(s.current));
    }
    case serdes_state::SIZING: {
      using pup_type = typename puper_for<serdes_state::SIZING>::type;
      return pup_ptr(new pup_type);
    }
    default: {
      return pup_ptr();
    }
  }
}

template <typename T>
struct puper<T, typename std::enable_if<hypercomm::built_in<T>::value>::type> {
  static constexpr auto is_proxy = std::is_base_of<CProxy, T>::value;

  inline static void undelegate(serdes& s, T& t, std::true_type) {
    if (!s.unpacking()) {
      t.ckUndelegate();
    }
  }

  inline static void undelegate(serdes& s, T& t, std::false_type) {}

  inline static void impl(serdes& s, T& t) {
    undelegate(s, t, typename std::integral_constant<bool, is_proxy>::type());
    auto p = make_puper(s);
    if (s.unpacking()) {
      reconstruct(&t);
    }
    (*p) | t;
    s.advanceBytes(p->size());
  }
};

template <typename T>
struct puper<
    std::shared_ptr<T>,
    typename std::enable_if<std::is_base_of<PUP::able, T>::value>::type> {
  inline static void impl(serdes& s, std::shared_ptr<T>& t) {
    if (s.unpacking()) {
      PUP::able* p = nullptr;
      pup<PUP::able*>(s, p);
      ::new (&t) std::shared_ptr<T>(
          std::dynamic_pointer_cast<T>(std::shared_ptr<PUP::able>(p)));
    } else {
      auto p = dynamic_cast<PUP::able*>(t.get());
      pup<PUP::able*>(s, p);
    }
  }
};

template <>
struct puper<CkCallback> {
  inline static void impl(serdes& s, CkCallback& t) {
    auto p = make_puper(s);
    *p | t;
    s.advanceBytes(p->size());
  }
};

template <typename T>
struct puper<T*, typename std::enable_if<is_message<T>::value>::type> {
  inline static void impl(serdes& s, T*& t) {
    auto p = make_puper(s);
    CkPupMessage(*p, reinterpret_cast<void**>(&t), 1);
    s.advanceBytes(p->size());
  }
};

template <typename T>
struct puper<std::shared_ptr<T>,
             typename std::enable_if<is_message<T>::value>::type> {
  inline static void impl(serdes& s, std::shared_ptr<T>& t) {
    if (s.unpacking()) {
      T* msg = nullptr;
      pup(s, msg);
      ::new (&t) std::shared_ptr<T>(static_cast<T*>(msg),
                                    [](T* msg) { CkFreeMsg(msg); });
    } else {
      T* msg = t.get();
      pup(s, msg);
    }
  }
};
}  // namespace hypercomm

#endif
