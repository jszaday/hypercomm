#ifndef __HYPERCOMM_CORE_VALUE_HPP__
#define __HYPERCOMM_CORE_VALUE_HPP__

#include "../utilities.hpp"

namespace hypercomm {

class hyper_value;
using value_ptr = std::unique_ptr<hyper_value>;

template <typename T>
class typed_value;

template <typename T>
using typed_value_ptr = std::unique_ptr<typed_value<T>>;

class deliverable;

class value_source {
 public:
  virtual void take_back(value_ptr&&) = 0;
};

class hyper_value {
 public:
  using message_type = CkMessage*;
  using source_type = std::shared_ptr<value_source>;

  source_type source;
  const bool pupable;

  hyper_value(const bool& _ = false) : pupable(_) {}
  virtual ~hyper_value() = default;
  virtual void pup_buffer(serdes& s, const bool& encapsulate) {
    NOT_IMPLEMENTED;
  }
  virtual message_type release(void) = 0;
  virtual bool recastable(void) const = 0;
};

inline void try_return(value_ptr&& value) {
  if (value) {
    auto& src = value->source;
    if (src) {
      src->take_back(std::move(value));
      return;
    }
  }
#if CMK_VERBOSE
  CkError("warning> unable to return value %p.\n", value.get());
#endif
}

template <typename T, typename... Args>
inline std::unique_ptr<T> make_value(Args... args) {
  return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}
}  // namespace hypercomm

#endif
