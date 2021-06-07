#ifndef __HYPERCOMM_CORE_VALUE_HPP__
#define __HYPERCOMM_CORE_VALUE_HPP__

#include "../utilities.hpp"

namespace hypercomm {

class hyper_value;

class value_source {
 public:
  virtual void take_back(std::shared_ptr<hyper_value>&&) = 0;
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

using value_ptr = std::shared_ptr<hyper_value>;

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

inline std::shared_ptr<plain_value> msg2value(
    typename hyper_value::message_type msg) {
  return std::make_shared<plain_value>(msg);
}

inline std::shared_ptr<plain_value> msg2value(
    std::shared_ptr<CkMessage>&& msg) {
  return std::make_shared<plain_value>(
      utilities::unwrap_message(std::move(msg)));
}
}

#endif
