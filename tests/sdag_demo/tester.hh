#ifndef __TESTER_HH__
#define __TESTER_HH__

#include <hypercomm/core/locality.hpp>
#include <hypercomm/components/mailbox.hpp>
#include <hypercomm/core/persistent_port.hpp>
#include <hypercomm/components/sentinel.hpp>

#include "tester.decl.h"

using namespace hypercomm;

struct test_component: public component {

  std::size_t n_inputs_;

  test_component(const id_t& _1, const std::size_t& _2)
  : component(_1), n_inputs_(_2) {}

  virtual value_set action(value_set&& values) override {
    CkPrintf("com%lu> i was invoked\n", this->id);
    return {};
  }

  virtual std::size_t n_inputs(void) const override {
    return this->n_inputs_;
  }

  virtual std::size_t n_outputs(void) const override {
    return 0;
  }
};

#endif
