#ifndef __HYPERCOMM_CORE_VALUE_HPP__
#define __HYPERCOMM_CORE_VALUE_HPP__

#include "../utilities.hpp"
#include "../messaging/endpoint.hpp"

namespace hypercomm {
class hyper_value {
 public:
  const CMK_REFNUM_TYPE flags;
  endpoint source;

  virtual ~hyper_value() = default;

  hyper_value(CMK_REFNUM_TYPE flags_)
      : flags(flags_), source(PUP::reconstruct()) {}

  virtual void pup_buffer(serdes& s, const bool& encapsulate) = 0;

  virtual message* as_message(void) const = 0;
};

template <typename T, typename... Args>
inline std::unique_ptr<T> make_value(Args... args) {
  return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}
}  // namespace hypercomm

#endif
