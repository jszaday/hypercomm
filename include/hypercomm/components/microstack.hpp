#ifndef __HYPERCOMM_COMPONENTS_MICROSTACK_HPP__
#define __HYPERCOMM_COMPONENTS_MICROSTACK_HPP__

#include <memory>
#include <tuple>
#include <type_traits>

#include <hypercomm/serialization/tags.hpp>

namespace hypercomm {

template <std::size_t I, typename T, typename Enable = void>
struct microstack_element;

template <std::size_t I, typename T>
using microstack_element_t = typename microstack_element<I, T>::type;

struct microstack_base {};

template <typename T, typename Base = microstack_base>
struct microstack;

template <typename T>
struct microstack_features;

template <typename Base, typename... Ts>
struct microstack_features<microstack<std::tuple<Ts...>, Base>>
    : public microstack_base {
  using self_type = microstack<std::tuple<Ts...>, Base>;
  using tuple_type = std::tuple<Ts...>;
  using storage_type = typename std::aligned_storage<sizeof(tuple_type),
                                                     alignof(tuple_type)>::type;

  template <std::size_t, typename, typename>
  friend struct microstack_element;

 protected:
  storage_type storage_;

  template <typename... Args>
  microstack_features(Args&&... args) {
    new (&(**this)) tuple_type(std::forward<Args>(args)...);
  }

  microstack_features(tags::no_init) {}

 public:
  ~microstack_features() { (**this).~tuple_type(); }

  template <std::size_t I>
  microstack_element_t<I, self_type>& get(void) {
    return microstack_element<I, self_type>()(static_cast<self_type&>(*this));
  }

  template <std::size_t I>
  const microstack_element_t<I, self_type>& get(void) const {
    return microstack_element<I, self_type>()(
        static_cast<const self_type&>(*this));
  }

  tuple_type& operator*(void) {
    return reinterpret_cast<tuple_type&>(this->storage_);
  }

  const tuple_type& operator*(void) const {
    return reinterpret_cast<const tuple_type&>(this->storage_);
  }
};

template <typename... Ts>
struct microstack<std::tuple<Ts...>, microstack_base>
    : public microstack_features<
          microstack<std::tuple<Ts...>, microstack_base>> {
  using self_type = microstack<std::tuple<Ts...>, microstack_base>;
  using tuple_type = std::tuple<Ts...>;

  template <typename... Args>
  microstack(Args&&... args)
      : microstack_features<self_type>(std::forward<Args>(args)...) {}

  std::shared_ptr<self_type> clone(void) const {
    return std::make_shared<self_type>(**this);
  }

  constexpr static std::size_t size(void) { return sizeof...(Ts); }

  std::shared_ptr<microstack_base> unwind(void) const { return {}; }
};

template <typename Base, typename... Ts>
struct microstack<std::tuple<Ts...>, Base>
    : public microstack_features<microstack<std::tuple<Ts...>, Base>> {
  using self_type = microstack<std::tuple<Ts...>, Base>;
  using tuple_type = std::tuple<Ts...>;

  template <std::size_t, typename, typename>
  friend struct microstack_element;

 private:
  std::shared_ptr<Base> base_;

 public:
  template <typename... Args>
  microstack(const std::shared_ptr<Base>& base, Args&&... args)
      : microstack_features<self_type>(std::forward<Args>(args)...),
        base_(base) {}

  std::shared_ptr<self_type> clone(void) const {
    return std::make_shared<self_type>(this->base_, **this);
  }

  constexpr static std::size_t size(void) {
    return Base::size() + sizeof...(Ts);
  }

  const std::shared_ptr<Base>& unwind(void) const { return this->base_; }
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
