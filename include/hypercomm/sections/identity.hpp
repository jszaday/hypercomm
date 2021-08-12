#ifndef __HYPERCOMM_SECTIONS_IDENTITY_HPP__
#define __HYPERCOMM_SECTIONS_IDENTITY_HPP__

#include "imprintable.hpp"

namespace hypercomm {

class identity_base_ {
 public:
  virtual std::shared_ptr<imprintable_base_> get_imprintable(void) const = 0;
};

/* identities track the upstream/downstream connections of a chare for a
 * given (imprintable), along with the counters for broadcasts/reductions
 */
template <typename Index>
class identity : public identity_base_ {
 protected:
  reduction_id_t redn_count_;
  reduction_id_t bcast_count_;

 public:
  identity(const reduction_id_t& seed) : redn_count_(seed), bcast_count_(0) {}

  // TODO incorporate tags, e.g., SOCK triplets!
  //     (i.e., should be next_reduction(const comparable&))
  inline reduction_id_t next_reduction(void) { return redn_count_++; }
  inline const reduction_id_t& last_reduction(void) const {
    return redn_count_;
  }

  inline reduction_id_t next_broadcast(void) const { return (bcast_count_ + 1); }
  inline bool accept_broadcast(const reduction_id_t& bcast) {
    if (this->bcast_count_ >= bcast) {
      return false;
    } else {
      this->bcast_count_ = bcast;
      return true;
    }
  }

  virtual const Index& mine(void) const = 0;
  virtual std::vector<Index> upstream(void) const = 0;
  virtual std::vector<Index> downstream(void) const = 0;
};
}  // namespace hypercomm

#endif
