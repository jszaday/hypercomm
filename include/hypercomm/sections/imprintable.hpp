#ifndef __HYPERCOMM_SECTIONS_IMPRINTABLE_HPP__
#define __HYPERCOMM_SECTIONS_IMPRINTABLE_HPP__

#include "../core/common.hpp"

namespace hypercomm {

class imprintable_base_ : public virtual comparable {
 public:
  virtual bool is_member(const CkArrayIndex&) const = 0;
};

template <typename Index>
class imprintable : public imprintable_base_ {
 public:
  using identity_ptr = std::shared_ptr<identity<Index>>;
  using locality_ptr = indexed_locality_<Index>*;

  virtual bool is_member(const Index&) const = 0;

  virtual bool is_member(const CkArrayIndex& idx) const {
    return this->is_member(reinterpret_index<Index>(idx));
  }

  // pick the root for the spanning tree, with a favored candidate
  virtual const Index& pick_root(const proxy_ptr&,
                                 const Index* = nullptr) const = 0;

  // apply this imprintable to a locality (generating an identity)
  virtual const identity_ptr& imprint(const locality_ptr&) const = 0;
};
}

#endif
