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

  // pick the root for the spanning tree, with a favored candidate
  virtual const Index& pick_root(const proxy_ptr&, Index* = nullptr) const = 0;

  // apply this imprintable to a locality (generating an identity)
  virtual const identity_ptr& imprint(const locality_ptr&) const = 0;
};
}

#endif
