
#ifndef __HYPERCOMM_TREE_BUILDER_MANAGED_IDENTITY_HPP__
#define __HYPERCOMM_TREE_BUILDER_MANAGED_IDENTITY_HPP__

#include "../sections/identity.hpp"

#include "manageable_base.hpp"

namespace hypercomm {
template <typename Index>
class managed_identity : public identity<Index> {
  const manageable_base_* inst_;

  inline static std::vector<Index> reinterpret_vector(
      const std::vector<CkArrayIndex>& src) {
    std::vector<Index> dst(src.size());
    std::transform(
        std::begin(src), std::end(src), std::begin(dst),
        [](const CkArrayIndex& val) { return reinterpret_index<Index>(val); });
    return dst;
  }

 public:
  managed_identity(const manageable_base_* inst, const reduction_id_t& seed)
      : inst_(inst), identity<Index>(seed) {}

  virtual const Index& mine(void) const override {
    return reinterpret_index<Index>(inst_->ckGetArrayIndex());
  }

  // TODO make the naming here more consistent!
  virtual std::vector<Index> downstream(void) const {
    CkAssert(inst_->association_ && inst_->association_->valid_upstream_);
    return reinterpret_vector(inst_->association_->upstream_);
  }

  virtual std::vector<Index> upstream(void) const {
    CkAssert(inst_->association_);
    return reinterpret_vector(inst_->association_->downstream_);
  }

  virtual std::shared_ptr<imprintable_base_> get_imprintable(void) const;
};
}  // namespace hypercomm

#endif
