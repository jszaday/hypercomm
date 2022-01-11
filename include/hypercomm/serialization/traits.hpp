#ifndef __HYPERCOMM_TRAITS_HPP__
#define __HYPERCOMM_TRAITS_HPP__

#include <list>
#include <deque>

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
struct is_list_like {
  enum { value = false };
};

template <typename T>
struct is_list_like<std::vector<T>> {
  enum { value = true };
};

template <typename T>
struct is_list_like<std::list<T>> {
  enum { value = true };
};

template <typename T>
struct is_list_like<std::deque<T>> {
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

// TODO ( unecessary with C++17 )
template <class... Args>
using void_t = void;

template <typename T, typename U, typename = void>
struct is_safely_castable : std::false_type {};

template <typename T, typename U>
struct is_safely_castable<T, U,
                          void_t<decltype(static_cast<U*>(std::declval<T*>()))>>
    : std::true_type {};

template <class T, std::size_t = sizeof(T)>
std::true_type is_complete_impl(T*);

std::false_type is_complete_impl(...);

template <class T>
using is_complete = decltype(is_complete_impl(std::declval<T*>()));

template <template <typename...> class Template, typename T>
struct is_specialization_of : std::false_type {};

template <template <typename...> class Template, typename... Args>
struct is_specialization_of<Template, Template<Args...>> : std::true_type {};

}  // namespace hypercomm

#endif
