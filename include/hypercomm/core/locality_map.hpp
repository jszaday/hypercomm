#ifndef __HYPERCOMM_CORE_LOCALITY_MAP_HPP__
#define __HYPERCOMM_CORE_LOCALITY_MAP_HPP__

#include "../utilities.hpp"

#include "locality.decl.h"

class locality_map_ : public CkArrayMap {
  bool node_level_;

 public:
  locality_map_(const bool& _1) : node_level_(_1) {}
  locality_map_(CkMigrateMessage*) {}

  virtual int procNum(int, const CkArrayIndex& element) override {
    const auto& idx = hypercomm::reinterpret_index<int>(element);

    if (node_level_) {
      return CkNodeFirst(idx % CkNumNodes());
    } else {
      return idx % CkNumPes();
    }
  }
};

namespace hypercomm {

namespace {
using map_type_ = std::shared_ptr<CkGroupID>;

CkpvDeclare(map_type_, group_map_);
CkpvDeclare(map_type_, node_group_map_);
}

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
}

#endif
