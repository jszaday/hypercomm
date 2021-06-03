#ifndef __HYPERCOMM_REDUCTIONS_REDUCER_HPP__
#define __HYPERCOMM_REDUCTIONS_REDUCER_HPP__

#include "../core.hpp"
#include "../components.hpp"

namespace hypercomm {

using reduction_id_t = component_id_t;

struct reducer : public hypercomm::component {
  hypercomm::combiner_ptr combiner;
  const std::size_t n_expected;

  reducer(const reduction_id_t& _1,
          const hypercomm::combiner_ptr& _2,
          const std::size_t& _3,
          value_type&& _4)
      : component(_1), combiner(_2), n_expected(_3) {
    // TODO offer an emplace function to replace this
    // QdCreate(1);
    // this->accepted.emplace_back(std::move(_3));
  }

  virtual std::size_t n_inputs(void) const override {
    return this->n_expected;
  }

  virtual std::size_t n_outputs(void) const override {
    return 1;
  }

  virtual value_set action(value_set&& accepted) {
    return {
      // TODO fixme
      std::make_pair(0, this->combiner->send({}))
    };
  }
};

}

#endif
