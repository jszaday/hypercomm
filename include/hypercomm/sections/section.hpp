#ifndef __HYPERCOMM_SECTIONS_BASE_HPP__
#define __HYPERCOMM_SECTIONS_BASE_HPP__

#include <vector>
#include <algorithm>

namespace hypercomm {

template <typename Ordinal, typename Index>
struct section : public virtual comparable {
  virtual const std::vector<Index>& members(void) const = 0;

  virtual bool is_valid_ordinal(const Ordinal& ord) const {
    return (ord > -1) && (ord < this->num_members());
  }

  virtual std::size_t num_members() const { return (this->members()).size(); }

  virtual Ordinal ordinal_for(const Index& idx) const {
    const auto& indices = this->members();
    const auto search = std::find(indices.begin(), indices.end(), idx);
    return Ordinal((search == indices.end()) ? -1 : (search - indices.begin()));
  }

  virtual Index index_at(const Ordinal& ord) const {
    return (this->members())[ord];
  }

  virtual hash_code hash(void) const override { return 0x0; }
};

}

#endif
