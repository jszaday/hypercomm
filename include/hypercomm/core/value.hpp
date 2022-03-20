#ifndef __HYPERCOMM_CORE_VALUE_HPP__
#define __HYPERCOMM_CORE_VALUE_HPP__

#include "../utilities.hpp"
#include "../messaging/endpoint.hpp"
#include "../messaging/messages.hpp"

namespace hypercomm {
class hyper_value : public detail::array_message {
 public:
  const CMK_REFNUM_TYPE flags;
  endpoint source;

  virtual ~hyper_value() = default;

  hyper_value(CMK_REFNUM_TYPE flags_)
      : array_message(handler()), flags(flags_), source(tags::reconstruct()) {}

  // returns true if wrapping was performed
  virtual bool pup_buffer(serdes& s, bool encapsulate) = 0;

  virtual message* as_message(void) const = 0;

  static const int& handler(void);

 private:
  static void handler_(void*);
};

template <typename T, typename... Args>
inline std::unique_ptr<T> make_value(Args... args) {
  return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}
}  // namespace hypercomm

#endif
