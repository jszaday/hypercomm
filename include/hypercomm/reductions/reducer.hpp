#ifndef __HYPERCOMM_REDUCTIONS_REDUCER_HPP__
#define __HYPERCOMM_REDUCTIONS_REDUCER_HPP__

#include "../components/component.hpp"
#include "../reductions/contribution.hpp"

namespace hypercomm {

struct reducer_base_ {
  using imprintable_ptr = std::shared_ptr<imprintable_base_>;
  using stamp_type = comparable_map<imprintable_ptr, reduction_id_t>;
  using pair_type = typename stamp_type::value_type;

 protected:
  imprintable_ptr imprintable_;
  reduction_id_t count_;

  reducer_base_(const imprintable_ptr& imprintable,
                std::size_t count)
  : imprintable_(imprintable), count_(count) {}

 public:
  inline pair_type stamp(void) const {
    return std::make_pair(this->imprintable_, this->count_);
  }

  // a reducer is affected by a stamp when it was issued at or after it
  bool affected_by(const stamp_type &stamp) const {
    auto search = stamp.find(this->imprintable_);
    return (search != std::end(stamp)) && (this->count_ >= search->second);
  }
};

template<typename T>
class reducer : public hypercomm::component<contribution<T>, contribution<T>>, public reducer_base_ {
  using parent_type_ = hypercomm::component<contribution<T>, contribution<T>>;

 public:
  combiner_ptr<T> combiner;
  std::size_t n_ustream, n_dstream;

  reducer(const component_id_t &_1, const pair_type &_2,
          const combiner_ptr<T> &_3, const std::size_t &_4,
          const std::size_t &_5)
      : parent_type_(_1),
        reducer_base_(_2.first, _2.second),
        combiner(_3),
        n_ustream(_4),
        n_dstream(_5) {
          this->persistent = true;
        }
  // virtual value_set action(value_set &&accepted) override;
};
}  // namespace hypercomm

#endif
