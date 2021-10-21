#ifndef __HYPERCOMM_CORE_CALLBACK_HPP__
#define __HYPERCOMM_CORE_CALLBACK_HPP__

#include "value.hpp"
#include "../serialization/construction.hpp"

namespace hypercomm {
namespace core {
class callback : virtual public polymorph {
 public:
  using value_type = deliverable;
  virtual void send(deliverable&&) = 0;
};

template <typename... Args>
class typed_callback : public callback {
 public:
  using tuple_type = std::tuple<Args...>;

  virtual void send(typed_value_ptr<tuple_type>&& args) = 0;

  virtual void send(deliverable&& dev) override;
};

template <bool Pack>
class immediate : virtual public polymorph {
 public:
  using return_type = deliverable;
  using argument_type =
      typename std::conditional<Pack, std::vector<deliverable>,
                                deliverable>::type;

  virtual return_type operator()(argument_type&&) = 0;
};
}  // namespace core

using callback = core::callback;
using callback_ptr = std::shared_ptr<callback>;

using combiner = core::immediate<true>;
using combiner_ptr = std::shared_ptr<combiner>;
}  // namespace hypercomm

#endif
