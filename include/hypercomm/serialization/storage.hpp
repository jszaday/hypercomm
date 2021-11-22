
#ifndef __HYPERCOMM_SERDES_STORAGE_HPP__
#define __HYPERCOMM_SERDES_STORAGE_HPP__

#include "pup.hpp"

namespace hypercomm {
// this is a class whose members are stored in-order
// with correct alignment (with the class's footprint)
// (v.s., std::tuple, whose member ordering is undefined!)
template <typename... Ts>
struct tuple_storage;

template <>
struct tuple_storage<> {};

template <typename T, std::size_t I>
struct tuple_storage_impl_ {
  using type = typename std::aligned_storage<sizeof(T), alignof(T)>::type;
  type storage_;

  inline T& operator*(void) { return reinterpret_cast<T&>(this->storage_); }

  tuple_storage_impl_(tags::no_init) {}

  template <typename... Args>
  tuple_storage_impl_(Args&&... args) {
    new (&(**this)) T(std::forward<Args>(args)...);
  }
};

template <typename T, typename... Ts>
struct tuple_storage<T, Ts...> : public tuple_storage_impl_<T, sizeof...(Ts)>,
                                 public tuple_storage<Ts...> {
  template <typename Arg, typename... Args>
  tuple_storage(Arg&& head, Args&&... tail)
      : tuple_storage_impl_<T, sizeof...(Ts)>(std::forward<Arg>(head)),
        tuple_storage<Ts...>(std::forward<Args>(tail)...) {}
};

template <std::size_t I, typename... Ts>
inline typename std::tuple_element<I, std::tuple<Ts...>>::type& element_at(
    tuple_storage<Ts...>& ts) {
  constexpr auto N = sizeof...(Ts);
  using element_t = typename std::tuple_element<I, std::tuple<Ts...>>::type;
  using storage_t = tuple_storage_impl_<element_t, (N - I - 1)>;
  return **(static_cast<storage_t*>(&ts));
}

namespace {
template <size_t N, typename... Args,
          PUP::Requires<(sizeof...(Args) > 0 && N == 0)> = nullptr>
inline void pup_tuple_impl(serdes& s, tuple_storage<Args...>& t) {
  pup(s, element_at<N>(t));
}

template <size_t N, typename... Args,
          PUP::Requires<(sizeof...(Args) > 0 && N > 0)> = nullptr>
inline void pup_tuple_impl(serdes& s, tuple_storage<Args...>& t) {
  pup_tuple_impl<(N - 1)>(s, t);
  pup(s, element_at<N>(t));
}
}  // namespace

template <typename... Ts>
struct puper<tuple_storage<Ts...>,
             typename std::enable_if<(sizeof...(Ts) > 0)>::type> {
  inline static void impl(serdes& s, tuple_storage<Ts...>& t) {
    pup_tuple_impl<(sizeof...(Ts) - 1)>(s, t);
  }
};

template <typename... Ts>
struct puper<tuple_storage<Ts...>,
             typename std::enable_if<(sizeof...(Ts) == 0)>::type> {
  inline static void impl(serdes& s, tuple_storage<Ts...>& t) {}
};
}  // namespace hypercomm

#endif
