#ifndef __HYPERCOMM_CORE_VALUE_WRAPPER_HPP__
#define __HYPERCOMM_CORE_VALUE_WRAPPER_HPP__

#include "common.hpp"

namespace hypercomm {
// wraps a value so it's (deceptively) pup'able
struct value_wrapper {
  deliverable value;

  value_wrapper(const value_wrapper&) = delete;
  value_wrapper(value_ptr&& _) : value(std::forward<value_ptr>(_)) {}
  value_wrapper(value_wrapper&& _) : value_wrapper(std::move(_.value)) {}
  value_wrapper(deliverable&& _) : value(std::move(_)) {}

  deliverable& operator*() { return this->value; }
  const deliverable& operator*() const { return this->value; }
};

template <>
struct puper<value_wrapper> {
  inline static void impl(serdes& s, value_wrapper& f) { NOT_IMPLEMENTED; }
};
}  // namespace hypercomm

#endif
