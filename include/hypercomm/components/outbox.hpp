#ifndef __HYPERCOMM_COMPONENTS_OUTBOX_HPP__
#define __HYPERCOMM_COMPONENTS_OUTBOX_HPP__

#include <deque>

#include "../utilities/traits.hpp"
#include "../core/typed_value.hpp"
#include "connector.hpp"

namespace hypercomm {
namespace components {
template <typename T>
struct outbox_;

template <>
struct outbox_<std::tuple<>> {
  inline void unspool(std::tuple<>&) {}

  inline bool empty(void) const { return false; }
};

template <typename... Ts>
struct outbox_<std::tuple<Ts...>> {
 private:
  static constexpr auto n_outputs = sizeof...(Ts);

  using tuple_type = std::tuple<Ts...>;

  template <size_t I>
  using tuple_elt_t = typename std::tuple_element<I, tuple_type>::type;

  template <typename T>
  using deque_type = std::deque<T>;

  using buffer_type = typename wrap_<tuple_type, deque_type>::type;

  using connector_type = typename wrap_<tuple_type, connector_>::type;

  template <std::size_t I>
  using buffer_elt_t = typename std::tuple_element<I, buffer_type>::type;

  buffer_type buffer_;
  connector_type connectors_;

  template <std::size_t I>
  inline void deliver_(tuple_type& tuple) {
    auto& con = std::get<I>(this->connectors_);
    if (con.ready()) {
      con.relay(std::move(std::get<I>(tuple)));
    } else {
      (std::get<I>(this->buffer_)).emplace_back(std::move(std::get<I>(tuple)));
    }
  }

  template <std::size_t I>
  inline typename std::enable_if<(I == 0)>::type unspool_(tuple_type& tuple) {
    this->deliver_<I>(tuple);
  }

  template <std::size_t I>
  inline typename std::enable_if<(I >= 1)>::type unspool_(tuple_type& tuple) {
    unspool_<(I - 1)>(tuple);
    this->deliver_<I>(tuple);
  }

  template <std::size_t I>
  inline typename std::enable_if<(I == 0), bool>::type empty_(void) const {
    return std::get<I>(this->buffer_).empty();
  }

  template <std::size_t I>
  inline typename std::enable_if<(I >= 1), bool>::type empty_(void) const {
    return std::get<I>(this->buffer_).empty() && this->empty_<(I - 1)>();
  }

 public:
  outbox_(void) : buffer_(), connectors_() {}

  template <std::size_t I>
  inline void try_flush(void) {
    auto& buf = std::get<I>(this->buffer_);
    auto& con = std::get<I>(this->connectors_);
    while (!buf.empty()) {
      con.relay(std::move(buf.front()));
      buf.pop_front();
    }
  }

  template <std::size_t I, typename... Args>
  inline bool connect_to(Args&&... args) {
    auto& con = std::get<I>(this->connectors_);
    bool prev = con.ready();
    if (prev) con.implode();
    new (&con) connector_<tuple_elt_t<I>>(std::forward<Args>(args)...);
    CkAssertMsg(con.ready(),
                "con not invalidated before set, or failure to initialize");
    return prev;
  }

  template <std::size_t I>
  inline void invalidate(void) {
    auto& con = std::get<I>(this->connectors_);
    if (con.ready()) con.implode();
  }

  inline void unspool(tuple_type& tuple) {
    this->unspool_<(n_outputs - 1)>(tuple);
  }

  inline bool empty(void) const { return this->empty_<(n_outputs - 1)>(); }
};
}  // namespace components
}  // namespace hypercomm

#endif
