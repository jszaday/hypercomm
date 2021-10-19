#ifndef __HYPERCOMM_REDUCTIONS_REDUCER_HPP__
#define __HYPERCOMM_REDUCTIONS_REDUCER_HPP__

#include "common.hpp"
#include "../components/component.hpp"

namespace hypercomm {
struct reducer : public component<deliverable, deliverable> {
 private:
  imprintable_ptr imprintable_;
  reduction_id_t count_;
  using parent_t = component<deliverable, deliverable>;
  std::vector<deliverable> devs_;

 public:
  using pair_type = typename stamp_type::value_type;

  hypercomm::combiner_ptr combiner;
  std::size_t n_ustream, n_dstream;

  reducer(const component_id_t &_1, const pair_type &_2,
          const hypercomm::combiner_ptr &_3, const std::size_t &_4,
          const std::size_t &_5)
      : parent_t(_1),
        imprintable_(_2.first),
        count_(_2.second),
        combiner(_3),
        n_ustream(_4),
        n_dstream(_5) {
    CkAssert(this->n_dstream == 1);
    this->devs_.reserve(this->n_ustream);
    this->permissive = true;
    this->persistent = true;
  }

  inline pair_type stamp(void) const {
    return std::make_pair(this->imprintable_, this->count_);
  }

  // a reducer is affected by a stamp when it was issued at or after it
  bool affected_by(const stamp_type &stamp) const {
    auto search = stamp.find(this->imprintable_);
    return (search != std::end(stamp)) && (this->count_ >= search->second);
  }

  // virtual bool permissive(void) const override { return true; }

  // virtual std::size_t n_inputs(void) const override { return this->n_ustream;
  // }

  // virtual std::size_t n_outputs(void) const override { return
  // this->n_dstream; }

  using in_set = parent_t::in_set;
  using out_set = parent_t::out_set;
  virtual out_set action(in_set &accepted) override;
};
}  // namespace hypercomm

#endif
