#ifndef HYPERCOMM_COMPONENTS_LINK_HPP
#define HYPERCOMM_COMPONENTS_LINK_HPP

#include <hypercomm/components/microstack.hpp>

namespace hypercomm {

namespace detail {
template <typename... Ts>
struct linker {
  using type = microstack<std::tuple<Ts...>>;
};
;

template <typename Base, typename... Ts, typename... Us>
struct linker<std::shared_ptr<microstack<std::tuple<Ts...>, Base>>, Us...> {
  using base_type = microstack<std::tuple<Ts...>, Base>;
  using type = microstack<std::tuple<Us...>, base_type>;
};

template <typename T>
using decay_t = typename std::decay<T>::type;
}  // namespace detail

template <typename... Ts>
using linker_t = typename detail::linker<detail::decay_t<Ts>...>::type;

template <typename... Ts>
std::shared_ptr<linker_t<Ts...>> link(Ts&&... ts) {
  return std::make_shared<linker_t<Ts...>>(std::forward<Ts>(ts)...);
}
}  // namespace hypercomm

#endif
