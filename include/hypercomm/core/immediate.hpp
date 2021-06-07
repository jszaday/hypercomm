#ifndef __HYPERCOMM_CORE_IMMEDIATE_HPP__
#define __HYPERCOMM_CORE_IMMEDIATE_HPP__

#include "callback.hpp"

namespace hypercomm {
template <typename Ret, typename... Args>
struct immediate_action;

template <typename Ret, typename... Args>
struct immediate_action<Ret(Args...)> : virtual public core::action<false, true> {
  static_assert(sizeof...(Args) == 1,
                "multi-args unsupported outside of C++17");

  using tuple_type = std::tuple<Args...>;

  template <std::size_t N>
  using argument_type = typename std::tuple_element<N, tuple_type>::type;

  virtual Ret action(Args...) = 0;

  virtual value_type send(value_type&& msg) override {
    throw std::runtime_error("not yet implemented!");
    // TODO fix this!
    // auto buffer = utilities::get_message_buffer(msg);
    // auto unpack = serdes::make_unpacker(msg, buffer);
    // temporary<argument_type<0>> tmp;
    // pup(unpack, tmp);
    // auto rval = this->action(tmp.value());
    // auto size = hypercomm::size(rval);
    // auto rmsg = message::make_message(size, {});
    // auto packr = serdes::make_packer(rmsg->payload);
    // return value_type(rmsg);
  }
};
}

#endif