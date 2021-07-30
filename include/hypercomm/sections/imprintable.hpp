#ifndef __HYPERCOMM_SECTIONS_IMPRINTABLE_HPP__
#define __HYPERCOMM_SECTIONS_IMPRINTABLE_HPP__

#include "identity.hpp"
#include "../core/common.hpp"

namespace hypercomm {

template <typename Index>
class imprintable : public virtual polymorph::trait {
 public:
  using identity_ptr = std::shared_ptr<identity<Index>>;
  using locality_ptr = indexed_locality_<Index>*;

  virtual const Index& root(void) const = 0;
  virtual const identity_ptr& imprint(const locality_ptr&) const = 0;
};
}

#endif
