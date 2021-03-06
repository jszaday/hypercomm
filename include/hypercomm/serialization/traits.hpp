#ifndef __HYPERCOMM_TRAITS_HPP__
#define __HYPERCOMM_TRAITS_HPP__

#include <deque>
#include <list>
#include "polymorph.hpp"

namespace hypercomm {

template <typename T, typename Enable = void>
struct is_bytes {
  enum { value = (std::is_trivial<T>::value && !std::is_pointer<T>::value) };
};

template <typename... Ts>
struct is_bytes<std::tuple<Ts...>> {
  enum { value = false };
};

template <typename T, typename Enable = void>
struct is_zero_copyable : public is_bytes<T> {};

template <typename T>
using is_polymorph = std::is_base_of<hypercomm::polymorph, T>;

template <typename T>
using is_trait = std::is_base_of<hypercomm::polymorph::trait, T>;

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
struct is_polymorphic {
  enum { value = false };
};

template <class T, typename Enable = void>
struct is_message {
  enum { value = false };
};

template <class T>
struct is_polymorphic<
    T, typename std::enable_if<
           std::is_base_of<hypercomm::polymorph, T>::value ||
           std::is_base_of<hypercomm::polymorph::trait, T>::value>::type> {
  enum { value = true };
};

template <class T, typename Enable = void>
struct is_idiosyncratic_ptr {
  static constexpr auto value = is_zero_copyable<T>::value ||
                                is_polymorphic<T>::value ||
                                is_message<T>::value;
};

template <template <typename...> class C, typename... Ts>
std::true_type is_base_of_template_impl(const C<Ts...>*);

template <template <typename...> class C>
std::false_type is_base_of_template_impl(...);

template <typename T, template <typename...> class C>
using is_base_of_template =
    decltype(is_base_of_template_impl<C>(std::declval<T*>()));

}  // namespace hypercomm

#endif
