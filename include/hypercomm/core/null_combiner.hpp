#ifndef __HYPERCOMM_CORE_NULLCOMB_HPP__
#define __HYPERCOMM_CORE_NULLCOMB_HPP__

#include "callback.hpp"

namespace hypercomm {
struct null_combiner : public core::combiner {
  virtual combiner::return_type send(combiner::argument_type&& args) override {
    return args.empty() ? combiner::return_type{} : args[0];
  }

  virtual void __pup__(hypercomm::serdes&) override {}
};
}

#endif
