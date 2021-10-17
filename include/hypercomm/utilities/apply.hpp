#ifndef __HYPERCOMM_UTILITIES_APPLY_HPP__
#define __HYPERCOMM_UTILITIES_APPLY_HPP__

#if __cplusplus >= 201703L
#include <tuple>

namespace hypercomm {
template <class F, class Tuple>
constexpr decltype(auto) apply(F&& f, Tuple&& t) {
  return std::apply<F, Tuple>(std::move(f), std::move(t));
}
}  // namespace hypercomm
#else
#include <tuple>

namespace {
template <std::size_t... N>
struct seq_ {};

template <std::size_t N, std::size_t... S>
struct seq_gen_ : seq_gen_<N - 1, N - 1, S...> {};

template <std::size_t... S>
struct seq_gen_<0, S...> {
  using type = seq_<S...>;
};

template <class F, class Tuple>
struct applicator_ {
  static constexpr auto S = std::tuple_size<Tuple>::value;
  using seq_gen_t = typename seq_gen_<S>::type;

  template <std::size_t... I>
  static auto call_(F&& f, Tuple&& t, seq_<I...>)
      -> decltype(f(std::get<I>(t)...)) {
    return f(std::get<I>(t)...);
  }

  static auto call_(F&& f, Tuple&& t)
      -> decltype(call_(std::move(f), std::move(t), seq_gen_t())) {
    return call_(std::move(f), std::move(t), seq_gen_t());
  }
};
}  // namespace

namespace hypercomm {
template <class F, class Tuple>
auto apply(F&& f, Tuple&& t)
    -> decltype(applicator_<F, Tuple>::call_(std::move(f), std::move(t))) {
  return applicator_<F, Tuple>::call_(std::move(f), std::move(t));
}
}  // namespace hypercomm
#endif

#endif
