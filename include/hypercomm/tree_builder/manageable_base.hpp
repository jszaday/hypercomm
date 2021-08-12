#ifndef __HYPERCOMM_TREE_BUILDER_MANAGEABLE_BASE_HPP__
#define __HYPERCOMM_TREE_BUILDER_MANAGEABLE_BASE_HPP__

#include "common.hpp"

#include "../reductions/reducer.hpp"

namespace hypercomm {

// NOTE eventually this will be replaced with a map
//      of sections to assocations :3
using association_ptr_ = std::unique_ptr<association_>;

class common_functions_ {
 public:
  using stamp_type = typename reducer::stamp_type;
  virtual stamp_type __stamp__(const CkArrayIndex* = nullptr) const = 0;
};

class manageable_base_ : public ArrayElement, public virtual common_functions_ {
  friend class tree_builder;

  template <typename T>
  friend class manageable;

  template <typename T>
  friend class managed_identity;

  association_ptr_ association_;

 protected:
  manageable_base_(void) : association_(nullptr) {}

 private:
  inline void set_association_(association_ptr_&& value) {
    this->association_.reset(value.release());
  }

  inline void set_endpoint_(void) {
    this->association_->valid_upstream_ = true;
  }

  inline bool is_endpoint_(void) const {
    return this->association_ && this->association_->valid_upstream_ &&
           this->association_->upstream_.empty();
  }

  inline void put_upstream_(const CkArrayIndex& idx) {
    this->association_->valid_upstream_ = true;
    this->association_->upstream_.emplace_back(idx);
  }

  inline void put_downstream_(const CkArrayIndex& idx) {
    this->association_->downstream_.emplace_back(idx);
  }

  inline std::size_t num_downstream_(void) const {
    if (this->association_) {
      return this->association_->downstream_.size();
    } else {
      return std::size_t{};
    }
  }
};
}  // namespace hypercomm

#endif
