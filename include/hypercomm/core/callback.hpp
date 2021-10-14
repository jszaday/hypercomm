#ifndef __HYPERCOMM_CORE_CALLBACK_HPP__
#define __HYPERCOMM_CORE_CALLBACK_HPP__

#include "typed_value.hpp"
#include "../messaging/deliverable.hpp"

namespace hypercomm {
namespace core {

template<bool Pack>
struct action_base_;

template<bool Pack>
struct action_base_: virtual public polymorph {
  using argument_type =
    typename std::conditional<Pack, std::vector<deliverable>, deliverable>::type;

  virtual void send(deliverable&&) = 0;
};

template <bool Pack, typename Arg, typename Ret>
struct action;

template <bool Pack, typename Arg, typename Ret>
struct action: virtual public polymorph {
  using value_type = typed_value_ptr<Arg>;
  using return_type = typed_value_ptr<Ret>;
  using argument_type =
      typename std::conditional<Pack, std::vector<value_type>, value_type>::type;

  virtual return_type send(argument_type&&) = 0;
};

template <bool Pack, typename Arg>
struct action<Pack, Arg, void>: public action_base_<Pack> {
  using value_type = typed_value_ptr<Arg>;
  using return_type = void;
  using argument_type =
      typename std::conditional<Pack, std::vector<value_type>, value_type>::type;

  virtual void send(typename action_base_<Pack>::argument_type&&) override {
    NOT_IMPLEMENTED;
  }

  virtual return_type send(argument_type&&) = 0;
};

}  // namespace core

template<typename T>
using callback = core::action<false, T, void>;

template<typename T>
using combiner = core::action<true, T, T>;

template<typename T>
using callback_ptr = std::shared_ptr<callback<T>>;

template<typename T>
using combiner_ptr = std::shared_ptr<combiner<T>>;

using generic_callback = core::action_base_<false>;
using generic_callback_ptr = std::shared_ptr<generic_callback>;

}  // namespace hypercomm

#endif
