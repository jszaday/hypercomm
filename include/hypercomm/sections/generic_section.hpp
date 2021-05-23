#ifndef __HYPERCOMM_SECTIONS_GENERIC_HPP__
#define __HYPERCOMM_SECTIONS_GENERIC_HPP__

#include "section.hpp"

#include <cstdint>

namespace hypercomm {

template <typename Index>
struct generic_section : public section<std::int64_t, Index>, public polymorph {
  using indices_type = std::vector<Index>;
  using ordinal_type = std::int64_t;

  /* TODO:
   * make this takes a const ref&
   * then use unaligned storage for copying/unpacking
   */
  indices_type indices;

  generic_section(PUP::reconstruct) {}
  generic_section(indices_type&& _1) : indices(_1) {}

  virtual const indices_type& members(void) const override {
    return this->indices;
  }

  virtual bool equals(const std::shared_ptr<comparable>& other) const override {
    auto other_typed = std::dynamic_pointer_cast<generic_section<Index>>(other);
    return other_typed && (this->indices == other_typed->indices);
  }

  virtual void __pup__(serdes& s) override {
    s | indices;
  }
};
}

#endif
