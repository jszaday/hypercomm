#ifndef __HYPERCOMM_SECTIONS_IDENTITY_HPP__
#define __HYPERCOMM_SECTIONS_IDENTITY_HPP__

#include "imprintable.hpp"

namespace hypercomm {

class identity_base_ {
 public:
  virtual std::shared_ptr<imprintable_base_> get_imprintable(void) const = 0;
};

template <typename Index>
class identity: public identity_base_ {
 protected:
  reduction_id_t gen_count_;

 public:
  identity(const reduction_id_t& seed = 0) : gen_count_(seed) {}

  // TODO incorporate tags, e.g., SOCK triplets!
  //     (i.e., should be next_reduction(const comparable&))
  inline reduction_id_t next_reduction(void) { return gen_count_++; }
  inline const reduction_id_t& last_reduction(void) const { return gen_count_; }

  virtual const Index& mine(void) const = 0;
  virtual std::vector<Index> upstream(void) const = 0;
  virtual std::vector<Index> downstream(void) const = 0;
};
}

#endif
