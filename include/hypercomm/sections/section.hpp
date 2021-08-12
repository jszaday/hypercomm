#ifndef __HYPERCOMM_SECTIONS_BASE_HPP__
#define __HYPERCOMM_SECTIONS_BASE_HPP__

#include "section_identity.hpp"

namespace hypercomm {

template <typename Ordinal, typename Index>
struct section : public imprintable<Index> {
  using ordinal_type = Ordinal;
  using this_type = section<Ordinal, Index>;
  using section_ptr = std::shared_ptr<this_type>;

  virtual section_ptr clone(void) const = 0;

  virtual const std::vector<Index>& members(void) const = 0;

  virtual std::size_t num_members() const { return (this->members()).size(); }

  virtual const Index& index_at(const Ordinal& ord) const {
    return (this->members())[ord];
  }

  // a member's ordinal is its position in the list of members
  // assumes all instances of the list are identically ordered
  // ( returns -1 when the member is not found )
  virtual Ordinal ordinal_for(const Index& idx) const {
    const auto& indices = this->members();
    const auto search = std::find(indices.begin(), indices.end(), idx);
    return Ordinal((search == indices.end()) ? -1 : (search - indices.begin()));
  }

  virtual bool is_valid_ordinal(const Ordinal& ord) const {
    // a valid ordinal is positive and within the list of members
    return (ord >= 0) && (ord < this->num_members());
  }

  virtual bool is_member(const Index& idx) const {
    // a member of this section has a valid ordinal
    return this->is_valid_ordinal(this->ordinal_for(idx));
  }

  using identity_ptr = typename imprintable<Index>::identity_ptr;
  using locality_ptr = typename imprintable<Index>::locality_ptr;

  virtual const CkArrayIndex* pick_root(const CkArrayID&) const {
    CkpvStaticDeclare(CkArrayIndex, idx);
    if (!CkpvInitialized(idx)) CkpvInitialize(CkArrayIndex, idx);
    CkpvAccess(idx) = conv2idx<CkArrayIndex>(this->index_at(0));
    return &CkpvAccess(idx);
  }

 protected:
  virtual identity_ptr imprint(const locality_ptr& loc,
                               const reduction_id_t& seed) const {
    return std::make_shared<section_identity<Index>>(seed, *this,
                                                     loc->__index__());
  }
};
}  // namespace hypercomm

#endif
