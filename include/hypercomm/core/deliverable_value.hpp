#ifndef __HYPERCOMM_CORE_DEV_VALUE_HPP__
#define __HYPERCOMM_CORE_DEV_VALUE_HPP__

#include "../messaging/deliverable.hpp"
#include "value.hpp"

namespace hypercomm {
// TODO ( semi-urgently need to deprecate this )
class deliverable_value : public hyper_value {
 public:
  deliverable dev;

  template <typename... Args>
  deliverable_value(Args... args) : dev(std::forward<Args>(args)...) {}

  virtual bool recastable(void) const override { return (bool)this->dev; }

  virtual message_type release(void) override {
    switch (this->dev.kind) {
      case deliverable::kMessage:
        return this->dev.release<CkMessage>();
      case deliverable::kValue: {
        auto *val = this->dev.release<hyper_value>();
        auto *msg = val->release();
        delete val;
        return msg;
      }
      default:
        NOT_IMPLEMENTED;
        return nullptr;
    }
  }
};

inline value_ptr msg2value(message *msg) {
  if (msg->is_null()) {
    delete msg;
    return nullptr;
  } else {
    CkAssertMsg(!msg->is_zero_copy(), "value for msg unavailable!");
    return make_value<deliverable_value>(msg);
  }
}

inline value_ptr msg2value(typename hyper_value::message_type msg) {
  if (UsrToEnv(msg)->getMsgIdx() == message::index()) {
    return msg2value((message *)msg);
  } else {
    return make_value<deliverable_value>(msg);
  }
}

inline value_ptr msg2value(std::shared_ptr<CkMessage> &&msg) {
  return make_value<deliverable_value>(
      utilities::unwrap_message(std::move(msg)));
}
}  // namespace hypercomm

#endif
