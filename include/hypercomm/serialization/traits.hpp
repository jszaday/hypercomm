#ifndef __HYPERCOMM_TRAITS_HPP__
#define __HYPERCOMM_TRAITS_HPP__

#include <deque>
#include <list>

#include "../core/proxy.hpp"
#include "polymorph.hpp"

namespace hypercomm {

template <typename T>
constexpr bool is_bytes(void) {
  return PUP::as_bytes<T>::value;
}

template <typename T>
struct is_list_or_deque {
  enum { value = false };
};

template <typename T>
struct is_list_or_deque<std::vector<T>> {
  enum { value = true };
};

template <typename T>
struct is_list_or_deque<std::list<T>> {
  enum { value = true };
};

template <typename T>
struct is_list_or_deque<std::deque<T>> {
  enum { value = true };
};

template <class T, typename Enable = void>
struct built_in {
  enum { value = 0 };
};

template <>
struct built_in<PUP::able*> {
  enum { value = 1 };
};

template <>
struct built_in<std::string> {
  enum { value = 1 };
};

template <>
struct built_in<CkArrayIndex> {
  enum { value = 1 };
};

template <typename T>
struct built_in<
    T, typename std::enable_if<std::is_base_of<CProxy, T>::value>::type> {
  enum { value = 1 };
};

template <class T, typename Enable = void>
struct is_pupable {
  enum { value = 0 };
};

template <class T, typename Enable = void>
struct is_message {
  enum { value = 0 };
};

template <class T>
struct is_message<T, typename std::enable_if<std::is_base_of<CkMessage, T>::value>::type> {
  enum { value = 1 };
};

template <class T>
struct is_pupable<T, typename std::enable_if<is_message<T>::value>::type> {
  enum { value = 1 };
};

template <class T, typename Enable = void>
struct is_polymorphic {
  enum { value = 0 };
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

template <class T>
struct is_polymorphic<
    T, typename std::enable_if<
           std::is_base_of<PUP::able, T>::value ||
           std::is_base_of<hypercomm::polymorph, T>::value ||
           std::is_base_of<hypercomm::polymorph::trait, T>::value ||
           std::is_base_of<hypercomm::proxy, T>::value>::type> {
  enum { value = 1 };
};

template <class T>
struct is_pupable<T, typename std::enable_if<is_polymorphic<T>::value>::type> {
  enum { value = 1 };
};
}

#endif