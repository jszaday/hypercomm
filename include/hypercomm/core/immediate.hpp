#ifndef __HYPERCOMM_CORE_IMMEDIATE_HPP__
#define __HYPERCOMM_CORE_IMMEDIATE_HPP__

#include "callback.hpp"

namespace hypercomm {
template <typename Ret, typename... Args>
struct immediate_action;

template <typename Ret, typename... Args>
struct immediate_action<Ret(Args...)>
    : virtual public core::action<false, std::tuple<Args...>, Ret> {
  using tuple_type = std::tuple<Args...>;

  using parent_type = core::action<false, tuple_type, Ret>;

  using return_type = typename parent_type::return_type;

  virtual Ret action(Args...) = 0;

  virtual return_type send(typed_value_ptr<tuple_type>&& args) override {
    NOT_IMPLEMENTED;
  }
};
}  // namespace hypercomm

#endif
