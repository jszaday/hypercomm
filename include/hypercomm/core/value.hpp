#ifndef __HYPERCOMM_CORE_VALUE_HPP__
#define __HYPERCOMM_CORE_VALUE_HPP__

#include "../utilities.hpp"

namespace hypercomm {

class hyper_value;

using value_ptr = std::unique_ptr<hyper_value>;

class value_source {
 public:
  virtual void take_back(value_ptr&&) = 0;
};

class hyper_value {
 public:
  using message_type = CkMessage*;
  using source_type = std::shared_ptr<value_source>;

  source_type source;

  virtual ~hyper_value() = default;
  virtual bool recastable(void) const = 0;
  virtual message_type release(void) = 0;
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

class plain_value : public hyper_value {
 public:
  message_type msg;

  plain_value(void) : msg(nullptr) {}

  plain_value(message_type _1) : msg(_1) {}

  ~plain_value() {
    if (msg != nullptr) {
      CkFreeMsg(msg);
    }
  }

  virtual bool recastable(void) const override { return msg != nullptr; }

  virtual message_type release(void) override {
    auto value = this->msg;
    this->msg = nullptr;
    return value;
  }
};

template <typename T, typename... Args>
inline std::unique_ptr<T> make_value(Args... args) {
  return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

inline std::unique_ptr<plain_value> msg2value(
    typename hyper_value::message_type msg) {
  return make_value<plain_value>(msg);
}

inline std::unique_ptr<plain_value> msg2value(
    std::shared_ptr<CkMessage>&& msg) {
  return make_value<plain_value>(utilities::unwrap_message(std::move(msg)));
}
}  // namespace hypercomm

#endif
