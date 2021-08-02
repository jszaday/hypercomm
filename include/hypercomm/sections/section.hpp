#ifndef __HYPERCOMM_SECTIONS_BASE_HPP__
#define __HYPERCOMM_SECTIONS_BASE_HPP__

#include "imprintable.hpp"

namespace hypercomm {

template <typename Ordinal, typename Index>
struct section : public polymorph, public imprintable<Index> {
  using ordinal_type = Ordinal;
  using this_type = section<Ordinal, Index>;
  using section_ptr = std::shared_ptr<this_type>;

  virtual section_ptr clone(void) const = 0;

  virtual const std::vector<Index>& members(void) const = 0;

  virtual std::size_t num_members() const { return (this->members()).size(); }

  virtual const Index& index_at(const Ordinal& ord) const {
    return (this->members())[ord];
  }

  virtual Ordinal ordinal_for(const Index& idx) const {
    const auto& indices = this->members();
    const auto search = std::find(indices.begin(), indices.end(), idx);
    return Ordinal((search == indices.end()) ? -1 : (search - indices.begin()));
  }

  virtual bool is_valid_ordinal(const Ordinal& ord) const {
    return (ord >= 0) && (ord < this->num_members());
  }

  virtual bool is_member(const Index& idx) const {
    return this->is_valid_ordinal(this->ordinal_for(idx));
  }

  using identity_ptr = typename imprintable<Index>::identity_ptr;
  using locality_ptr = typename imprintable<Index>::locality_ptr;

  virtual const Index& pick_root(const proxy_ptr&, const Index* favored) const {
    // pick favored if it is a valid member of this section
    if (favored && this->is_member(*favored)) {
      return *favored;
    } else {
      return this->index_at(0);
    }
  }

  virtual const identity_ptr& imprint(const locality_ptr& loc) const {
    return loc->identity_for(this->clone());
  }
};
}

#endif
