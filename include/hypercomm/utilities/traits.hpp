#ifndef __HYPERCOMM_UTILITIES_INTERNAL_HPP__
#define __HYPERCOMM_UTILITIES_INTERNAL_HPP__

#include <array>
#include <tuple>
#include <type_traits>
#include <typeindex>

namespace hypercomm {
template <typename Tuple, std::size_t N, std::size_t I>
inline typename std::enable_if<I == 0>::type make_type_list_(
    std::array<std::type_index, N>& arr) {
  using type = typename std::tuple_element<I, Tuple>::type;
  new (&arr[I]) std::type_index(typeid(type));
}

template <typename Tuple, std::size_t N, std::size_t I>
inline typename std::enable_if<(I >= 1)>::type make_type_list_(
    std::array<std::type_index, N>& arr) {
  using type = typename std::tuple_element<I, Tuple>::type;
  new (&arr[I]) std::type_index(typeid(type));
  make_type_list_<Tuple, N, I - 1>(arr);
}

template <typename Tuple, std::size_t N>
std::array<std::type_index, N> make_type_list_(void) {
  using type = std::array<std::type_index, N>;
  std::aligned_storage<sizeof(type), alignof(type)> storage;
  auto* arr = reinterpret_cast<type*>(&storage);
  make_type_list_<Tuple, N, N - 1>(*arr);
  return *arr;
}

template <typename T, bool Empty = false>
struct tuplify_ {
  using type = std::tuple<T>;
};

template <bool Empty>
struct tuplify_<std::tuple<>, Empty> {
  using type = typename std::conditional<Empty, std::tuple<>,
                                         std::tuple<std::tuple<>>>::type;
};

template <bool Empty, typename... Ts>
struct tuplify_<std::tuple<Ts...>, Empty> {
  using type = std::tuple<Ts...>;
};

template <typename T, template <typename> typename Wrapper,
          typename Enable = void>
struct wrap_;

template <template <typename> typename Wrapper, typename... Ts>
struct wrap_<std::tuple<Ts...>, Wrapper,
             typename std::enable_if<(sizeof...(Ts) == 0)>::type> {
  using type = std::tuple<>;
};

template <template <typename> typename Wrapper, typename... Ts>
struct wrap_<std::tuple<Ts...>, Wrapper,
             typename std::enable_if<(sizeof...(Ts) == 1)>::type> {
  using type = std::tuple<Wrapper<Ts...>>;
};

template <template <typename> typename Wrapper, typename T, typename... Ts>
struct wrap_<std::tuple<T, Ts...>, Wrapper,
             typename std::enable_if<(sizeof...(Ts) >= 1)>::type> {
 private:
  using left_t = typename wrap_<std::tuple<T>, Wrapper>::type;
  using right_t = typename wrap_<std::tuple<Ts...>, Wrapper>::type;

 public:
  using type =
      decltype(std::tuple_cat(std::declval<left_t>(), std::declval<right_t>()));
};
}  // namespace hypercomm

#endif
