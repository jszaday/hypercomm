#ifndef __HYPERCOMM_CORE_PORT_OPENER_HPP__
#define __HYPERCOMM_CORE_PORT_OPENER_HPP__

#include "common.hpp"

namespace hypercomm {

struct port_opener : public immediate_action<void(generic_locality_*)> {
  entry_port_ptr port;
  callback_ptr cb;

  port_opener(PUP::reconstruct) {}

  port_opener(const entry_port_ptr& _1, const callback_ptr& _2)
      : port(_1), cb(_2) {}

  virtual void action(generic_locality_* loc) override { loc->open(port, cb); }

  virtual void __pup__(serdes& s) override {
    s | port;
    s | cb;
  }
};
}  // namespace hypercomm

#endif
