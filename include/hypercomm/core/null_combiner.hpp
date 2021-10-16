#ifndef __HYPERCOMM_CORE_NULLCOMB_HPP__
#define __HYPERCOMM_CORE_NULLCOMB_HPP__

#include "callback.hpp"
#include "../messaging/deliverable.hpp"

namespace hypercomm {
struct null_combiner : public combiner {
  null_combiner(void) = default;
  null_combiner(const tags::reconstruct&) {}

  virtual deliverable operator()(std::vector<deliverable>&& args) override {
    if (args.empty()) {
      return deliverable(value_ptr());
    } else {
      return std::move(args[0]);
    }
  }

  virtual void __pup__(hypercomm::serdes&) override {}
};
}  // namespace hypercomm

#endif
