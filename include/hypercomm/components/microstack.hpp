#ifndef __HYPERCOMM_COMPONENTS_MICROSTACK_HPP__
#define __HYPERCOMM_COMPONENTS_MICROSTACK_HPP__

#include <memory>
#include <tuple>
#include <type_traits>

namespace hypercomm {

template <std::size_t I, typename T, typename Enable = void>
struct microstack_element;

template <std::size_t I, typename T>
using microstack_element_t = typename microstack_element<I, T>::type;

struct microstack_base {};

namespace {
template <typename T>
constexpr std::size_t get_size_(void) {
  return T::size();
}

template <>
constexpr std::size_t get_size_<microstack_base>(void) {
  return 0;
}
}  // namespace

template <typename T, typename Base = microstack_base>
struct microstack;

template <typename Base, typename... Ts>
struct microstack<std::tuple<Ts...>, Base> : public microstack_base {
  using self_type = microstack<std::tuple<Ts...>, Base>;
  using tuple_type = std::tuple<Ts...>;
  using storage_type = typename std::aligned_storage<sizeof(tuple_type),
                                                     alignof(tuple_type)>::type;

  template <std::size_t I, typename T, typename Enable>
  friend struct microstack_element;

 private:
  std::shared_ptr<Base> base_;
  storage_type storage_;

 public:
  template <typename... Args>
  microstack(const std::shared_ptr<Base>& base, Args&&... args) : base_(base) {
    new (&(**this)) tuple_type(std::forward<Args>(args)...);
  }

  ~microstack() { (**this).~tuple_type(); }

  tuple_type& operator*(void) {
    return reinterpret_cast<tuple_type&>(this->storage_);
  }

  const tuple_type& operator*(void) const {
    return reinterpret_cast<const tuple_type&>(this->storage_);
  }

  template <std::size_t I>
  microstack_element_t<I, self_type>& get(void) {
    return microstack_element<I, self_type>()(*this);
  }

  template <std::size_t I>
  const microstack_element_t<I, self_type>& get(void) const {
    return microstack_element<I, self_type>()(*this);
  }

  const std::shared_ptr<Base>& unwind(void) const { return this->base_; }

  constexpr static std::size_t size(void) {
    return get_size_<Base>() + sizeof...(Ts);
  }
};

template <std::size_t I, typename... Ts>
struct microstack_element<I, microstack<std::tuple<Ts...>>> {
  using type = typename std::tuple_element<I, std::tuple<Ts...>>::type;

  type& operator()(microstack<std::tuple<Ts...>>& stack) const {
    return std::get<I>(*stack);
  }

  const type& operator()(const microstack<std::tuple<Ts...>>& stack) const {
    return std::get<I>(*stack);
  }
};

template <std::size_t I, typename Base, typename... Ts>
struct microstack_element<
    I, microstack<std::tuple<Ts...>, Base>,
    typename std::enable_if<!std::is_same<Base, microstack_base>::value and
                            (I < Base::size())>::type> {
  using type = microstack_element_t<I, Base>;

  type& operator()(microstack<std::tuple<Ts...>, Base>& stack) const {
    return (stack.base_)->template get<I>();
  }

  const type& operator()(
      const microstack<std::tuple<Ts...>, Base>& stack) const {
    return (stack.base_)->template get<I>();
  }
};

template <std::size_t I, typename Base, typename... Ts>
struct microstack_element<
    I, microstack<std::tuple<Ts...>, Base>,
    typename std::enable_if<!std::is_same<Base, microstack_base>::value and
                            (I >= Base::size())>::type> {
  using type =
      typename std::tuple_element<(I - Base::size()), std::tuple<Ts...>>::type;

  type& operator()(microstack<std::tuple<Ts...>, Base>& stack) const {
    return std::get<(I - Base::size())>(*stack);
  }

  const type& operator()(
      const microstack<std::tuple<Ts...>, Base>& stack) const {
    return std::get<(I - Base::size())>(*stack);
  }
};
}  // namespace hypercomm

#endif
