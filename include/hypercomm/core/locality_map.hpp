#ifndef __HYPERCOMM_CORE_LOCALITY_MAP_HPP__
#define __HYPERCOMM_CORE_LOCALITY_MAP_HPP__

#include "../messaging/packing.hpp"
#include "immediate.hpp"

#include <hypercomm/core/locality.decl.h>

namespace hypercomm {
namespace detail {
// maps indices to PEs
using mapper = immediate_action<int(const CkArrayIndex&)>;

template <typename T>
class default_mapper : public mapper {
  bool node_level_;

 public:
  default_mapper(const bool& _1) : node_level_(_1) {}

  virtual int action(const CkArrayIndex& elt) override {
    const auto& idx = hypercomm::reinterpret_index<T>(elt);
    if (node_level_) {
      // return the first pe at the node
      return CkNodeFirst(idx % CkNumNodes());
    } else {
      return idx % CkNumPes();
    }
  }

  virtual void __pup__(serdes& s) override { s | this->node_level_; }
};
}  // namespace detail
}  // namespace hypercomm

class locality_map_ : public CkArrayMap {
  std::shared_ptr<hypercomm::detail::mapper> mapper_;

 public:
  locality_map_(const bool& _1)
      : mapper_(new hypercomm::detail::default_mapper<int>(_1)) {}

  locality_map_(CkMessage* msg) { hypercomm::unpack(msg, this->mapper_); }

  locality_map_(CkMigrateMessage*) {}

  virtual int procNum(int, const CkArrayIndex& elt) override {
    return this->mapper_->action(elt);
  }
};

namespace hypercomm {

namespace {
using map_type_ = std::shared_ptr<CkGroupID>;

CkpvDeclare(map_type_, group_map_);
CkpvDeclare(map_type_, node_group_map_);
}  // namespace

inline const CkGroupID& access_group_map(void) {
  if (!CkpvInitialized(group_map_)) {
    CkpvInitialize(map_type_, group_map_);
  }
  auto& gmap = CkpvAccess(group_map_);
  if (!gmap) {
    gmap.reset(new CkGroupID(CProxy_locality_map_::ckNew(false)));
  }
  return *gmap;
}

template <typename T, typename... Ts>
inline T make_grouplike(Ts... ts) {
  CkArrayOptions opts(CkNumPes());
  opts.setMap(access_group_map());
  return static_cast<T>(T::ckNew(ts..., opts));
}

inline const CkGroupID& access_node_group_map(void) {
  if (!CkpvInitialized(node_group_map_)) {
    CkpvInitialize(map_type_, node_group_map_);
  }
  auto& ngmap = CkpvAccess(node_group_map_);
  if (!ngmap) {
    ngmap.reset(new CkGroupID(CProxy_locality_map_::ckNew(true)));
  }
  return *ngmap;
}

template <typename T, typename... Ts>
inline T make_nodegrouplike(Ts... ts) {
  CkArrayOptions opts(CkNumNodes());
  opts.setMap(access_node_group_map());
  return static_cast<T>(T::ckNew(ts..., opts));
}
}  // namespace hypercomm

#endif
