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
      return CkNodeFirst(idx);
    } else {
      return idx;
    }
  }
};

#endif
