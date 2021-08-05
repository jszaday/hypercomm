#ifndef __HYPERCOMM_REDUCTIONS_REDUCER_HPP__
#define __HYPERCOMM_REDUCTIONS_REDUCER_HPP__

#include "../sections/imprintable.hpp"

namespace hypercomm {

struct reducer : public hypercomm::component {
  using imprintable_ptr = std::shared_ptr<imprintable_base_>;
  using stamp_type = std::tuple<imprintable_ptr, reduction_id_t>;
  stamp_type stamp;

  hypercomm::combiner_ptr combiner;
  std::size_t n_ustream, n_dstream;

  reducer(const component::id_t &_1, const stamp_type &_2,
          const hypercomm::combiner_ptr &_3, const std::size_t &_4,
          const std::size_t &_5)
      : component(_1), stamp(_2), combiner(_3), n_ustream(_4), n_dstream(_5) {}

  bool affected_by(const stamp_type &stamp) const {
    return (std::get<1>(this->stamp) <= std::get<1>(this->stamp)) &&
           comparable_comparator<imprintable_ptr>()(std::get<0>(this->stamp),
                                                    std::get<0>(stamp));
  }

  virtual bool permissive(void) const override { return true; }

  virtual std::size_t n_inputs(void) const override { return this->n_ustream; }

  virtual std::size_t n_outputs(void) const override { return this->n_dstream; }

  virtual value_set action(value_set &&accepted) {
    // NOTE this can be corrected by duplicating the result of
    //      the combiner, but is it valid to do so?
    CkAssertMsg(this->n_dstream == 1, "multi output unsupported");
    // TODO is there a more efficient way to do this?
    typename combiner::argument_type args;
    for (auto &pair : accepted) {
      auto &value = pair.second;
      if (value) {
        args.emplace_back(std::move(value));
      }
    }
    return {std::make_pair(0, this->combiner->send(std::move(args)))};
  }
};
}  // namespace hypercomm

#endif
