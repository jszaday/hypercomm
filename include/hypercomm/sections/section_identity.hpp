#ifndef __HYPERCOMM_SECTIONS_SECTION_IDENTITY_HPP__
#define __HYPERCOMM_SECTIONS_SECTION_IDENTITY_HPP__

#include "identity.hpp"

namespace hypercomm {

template <typename Ordinal, typename Index>
struct section;

template <typename Index>
class section_identity : public identity<Index> {
 public:
  // TODO this should be a more generic thing
  //      (e.g., incorporate proxies for cross-array sections)
  using ordinal_type = std::int64_t;
  using index_type = Index;

  using section_type = section<ordinal_type, index_type>;
  using section_ptr = std::shared_ptr<section_type>;

  // TODO add const auto& for local reference too.

 private:
  section_ptr sect_;
  Index mine_;

 public:
  virtual const Index &mine(void) const override { return this->mine_; }

  // TODO when can we avoid this clone?
  section_identity(const section_type &_1, const Index &_2)
      : section_identity(_1.clone(), _2) {}

  section_identity(const section_ptr &_1, const Index &_2)
      : sect_(_1), mine_(_2){};

  virtual std::vector<Index> downstream(void) const override {
    const auto ord = sect_->ordinal_for(mine_);
    CkAssert((sect_->is_valid_ordinal(ord)) &&
             "identity> index must be member of section");
    const auto parent = binary_tree::parent(ord);
    auto rval = std::vector<Index>{};
    if (sect_->is_valid_ordinal(parent))
      rval.push_back(sect_->index_at(parent));
    return rval;
  }

  virtual std::vector<Index> upstream(void) const override {
    const auto ord = sect_->ordinal_for(mine_);
    CkAssert((sect_->is_valid_ordinal(ord)) &&
             "identity> index must be member of section");
    const auto left = binary_tree::left_child(ord);
    const auto right = binary_tree::right_child(ord);
    auto rval = std::vector<Index>{};
    if (sect_->is_valid_ordinal(left)) rval.push_back(sect_->index_at(left));
    if (sect_->is_valid_ordinal(right)) rval.push_back(sect_->index_at(right));
#if CMK_VERBOSE
    CkPrintf("%d> has children %lu and %lu.\n", mine_, left, right);
#endif
    return rval;
  }

  virtual std::shared_ptr<imprintable_base_> get_imprintable(void) const {
    return std::static_pointer_cast<imprintable_base_>(this->sect_);
  }
};
}

#endif
