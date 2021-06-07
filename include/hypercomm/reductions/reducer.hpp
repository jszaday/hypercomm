#ifndef __HYPERCOMM_REDUCTIONS_REDUCER_HPP__
#define __HYPERCOMM_REDUCTIONS_REDUCER_HPP__

#include "../core/callback.hpp"
#include "../components.hpp"
#include "../serialization/pup.hpp"

namespace hypercomm {

struct reducer : public hypercomm::component {
  hypercomm::combiner_ptr combiner;
  const std::size_t n_ustream, n_dstream;

  reducer(const reduction_id_t &_1, const hypercomm::combiner_ptr &_2,
          const std::size_t &_3, const std::size_t &_4)
      : component(_1), combiner(_2), n_ustream(_3), n_dstream(_4) {}

  virtual std::size_t n_inputs(void) const override { return this->n_ustream; }

  virtual std::size_t n_outputs(void) const override { return this->n_dstream; }

  virtual value_set action(value_set &&accepted) {
    // NOTE this can be corrected by duplicating the result of
    //      the combiner, but is it valid to do so?
    CkAssert(this->n_dstream == 1 && "multi output unsupported");
    // TODO is there a more efficient way to do this?
    typename combiner::argument_type args(accepted.size());
    std::transform(std::begin(accepted), std::end(accepted), std::begin(args),
                   [](typename value_set::value_type &value) {
                     return std::move(value.second);
                   });
    return { std::make_pair(0, this->combiner->send(std::move(args)))};
  }
};
}

#endif
