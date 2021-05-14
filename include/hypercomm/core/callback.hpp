#ifndef __HYPERCOMM_CORE_CALLBACK_HPP__
#define __HYPERCOMM_CORE_CALLBACK_HPP__

#include "../serialization/polymorph.hpp"

namespace hypercomm {
namespace core {

template<bool MultiMsg, bool Returns>
struct action : virtual public polymorph {
  using value_type = std::shared_ptr<CkMessage>;

  using return_type = typename std::conditional<Returns, value_type, void>::type;
  using argument_type = typename std::conditional<MultiMsg, std::vector<value_type>, value_type>::type;

  virtual return_type send(argument_type&&) = 0;

  template<bool multi_msg_t = MultiMsg,
           typename std::enable_if<multi_msg_t>::type>
  return_type send(value_type&& value) {
    argument_type list = { value };
    return send(std::move(list));
  }
};

using callback = action<false, false>;
using combiner = action<true, true>;

}
}

#endif
