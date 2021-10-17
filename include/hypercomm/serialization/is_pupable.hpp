#ifndef __HYPERCOMM_IS_PUPABLE_HPP__
#define __HYPERCOMM_IS_PUPABLE_HPP__

#include "../utilities.hpp"
#include "pup.hpp"

namespace hypercomm {

template <typename T, typename Enable = void>
struct is_pupable {
  static constexpr auto value = is_complete<puper<T>>::value;
};

template <typename T>
struct is_pupable<std::tuple<T>> {
  static constexpr auto value = is_pupable<T>::value;
};

template <typename T, typename... Ts>
struct is_pupable<std::tuple<T, Ts...>, std::enable_if<sizeof...(Ts) >= 1>> {
  static constexpr auto value =
      is_pupable<T>::value && is_pupable<std::tuple<Ts...>>::value;
};

}  // namespace hypercomm

#endif
