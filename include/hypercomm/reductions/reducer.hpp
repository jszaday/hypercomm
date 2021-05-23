#ifndef __HYPERCOMM_REDUCTIONS_REDUCER_HPP__
#define __HYPERCOMM_REDUCTIONS_REDUCER_HPP__

#include "../core.hpp"
#include "../components.hpp"

namespace hypercomm {

using reduction_id_t = component_id_t;

struct reducer : public hypercomm::component {
  hypercomm::combiner_ptr combiner;

  reducer(const reduction_id_t& _1, const hypercomm::combiner_ptr& _2,
          value_t&& _3)
      : component(_1), combiner(_2) {
    // TODO offer an emplace function to replace this
    QdCreate(1);
    this->accepted.emplace_back(std::move(_3));
  }

  virtual value_t action(void) {
    // TODO offer a take function to replace this
    //      (that also calls QdProcess)
    return this->combiner->send(std::move(this->accepted));
  }
};

}

#endif
