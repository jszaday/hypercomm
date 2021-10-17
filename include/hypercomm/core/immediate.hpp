#ifndef __HYPERCOMM_CORE_IMMEDIATE_HPP__
#define __HYPERCOMM_CORE_IMMEDIATE_HPP__

#include "callback.hpp"
#include "typed_value.hpp"
#include "../utilities/apply.hpp"

namespace hypercomm {
template <typename Ret, typename... Args>
struct immediate_action;

template <typename... Args>
struct immediate_action<void(Args...)> : public core::typed_callback<Args...> {
  using parent_type = core::typed_callback<Args...>;
  using tuple_type = typename parent_type::tuple_type;

  virtual void action(Args...) = 0;

  virtual void send(typed_value_ptr<tuple_type>&& val) override {
    apply(
        [&](Args... args) -> void {
          this->action(std::forward<Args>(args)...);
        },
        std::move(val->value()));
  }
};

template <typename Ret, typename... Args>
struct immediate_action<Ret(Args...)> : public core::immediate<false> {
  using tuple_type = std::tuple<Args...>;

  virtual Ret action(Args...) = 0;

  virtual deliverable operator()(deliverable&& dev) override {
    auto val = dev2typed<tuple_type>(std::move(dev));
    auto ret = make_typed_value<Ret>(tags::no_init());

    apply(
        [&](Args... args) -> void {
          new (ret->get()) Ret(this->action(std::forward<Args>(args)...));
        },
        std::move(val->value()));

    return deliverable(std::move(ret));
  }
};

}  // namespace hypercomm

#endif
