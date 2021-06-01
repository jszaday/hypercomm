#ifndef __TESTER_HH__
#define __TESTER_HH__

#include <hypercomm/core/locality.hpp>
#include "tester.decl.h"

using namespace hypercomm;

struct resuming_callback : public core::callback {
  using value_type = core::callback::value_type;

  CthThread th;
  value_type *out;

  resuming_callback(PUP::reconstruct) {}

  resuming_callback(const CthThread& _1, value_type* _2): th(_1), out(_2) {}

  virtual void send(value_type&& value) override {
    *out = std::move(value);
    CthAwaken(th);
  }

  virtual void __pup__(serdes& s) override {
    throw std::runtime_error("not yet implemented");
  }
};

#endif
