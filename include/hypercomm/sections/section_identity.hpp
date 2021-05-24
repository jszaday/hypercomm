#ifndef __HYPERCOMM_SECTIONS_SECTION_IDENTITY_HPP__
#define __HYPERCOMM_SECTIONS_SECTION_IDENTITY_HPP__

#include "section.hpp"
#include "identity.hpp"

namespace hypercomm {

template <typename Index>
struct section_identity : public identity<Index> {
  // TODO this should be a more generic thing
  //      (e.g., incorporate proxies for cross-array sections)
  using ordinal_type = typename generic_section<Index>::ordinal_type;
  using index_type = Index;

  using section_type = section<ordinal_type, index_type>;
  using section_ptr = std::shared_ptr<section_type>;

  // TODO add const auto& for local reference too.

  section_ptr sect;
  Index mine;

  // TODO when can we avoid this clone?
  section_identity(const section_type& _1, const Index& _2)
  : section_identity(_1.clone(), _2) {}

  section_identity(const section_ptr& _1, const Index& _2)
      : sect(_1), mine(_2) {};

  virtual std::vector<Index> downstream(void) const override {
    const auto ord = sect->ordinal_for(mine);
    CkAssert((sect->is_valid_ordinal(ord)) &&
             "identity> index must be member of section");
    const auto parent = binary_tree::parent(ord);
    auto rval = std::vector<Index>{};
    if (sect->is_valid_ordinal(parent)) rval.push_back(sect->index_at(parent));
    return rval;
  }

  virtual std::vector<Index> upstream(void) const override {
    const auto ord = sect->ordinal_for(mine);
    CkAssert((sect->is_valid_ordinal(ord)) &&
             "identity> index must be member of section");
    const auto left = binary_tree::left_child(ord);
    const auto right = binary_tree::right_child(ord);
    auto rval = std::vector<Index>{};
    if (sect->is_valid_ordinal(left)) rval.push_back(sect->index_at(left));
    if (sect->is_valid_ordinal(right)) rval.push_back(sect->index_at(right));
#if CMK_DEBUG
    CkPrintf("%d> has children %lu and %lu.\n", mine, left, right);
#endif
    return rval;
  }
};

}

#endif
