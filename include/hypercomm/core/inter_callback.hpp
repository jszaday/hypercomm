#ifndef __HYPERCOMM_CORE_INTERCB_HPP__
#define __HYPERCOMM_CORE_INTERCB_HPP__

#include "callback.hpp"
#include "../serialization/special.hpp"
#include "../messaging/deliverable.hpp"

namespace hypercomm {

struct inter_callback : public core::callback {
  CkCallback cb;

  inter_callback(tags::reconstruct) {}

  inter_callback(const CkCallback& _1) : cb(_1) {}

  virtual void send(core::callback::value_type&& value) override {
    cb.send(deliverable::to_message(std::move(value)));
  }

  virtual void __pup__(serdes& s) override { s | cb; }
};

inline callback_ptr intercall(const CkCallback& cb) {
  return std::make_shared<inter_callback>(cb);
}

}  // namespace hypercomm

#endif
