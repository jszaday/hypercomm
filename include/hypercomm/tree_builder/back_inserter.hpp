#ifndef __HYPERCOMM_BACK_INSERTER_HPP__
#define __HYPERCOMM_BACK_INSERTER_HPP__

#include <charm++.h>

#if CMK_SMP

namespace hypercomm {

struct back_inserter {
  char core[CmiReservedHeaderSize];

  CkGroupID gid;
  CkArrayID aid;

  back_inserter(const CkGroupID& _1, const CkArrayID& _2) : gid(_1), aid(_2) {
    CmiSetHandler(this, handler());
  }

  static const int& handler(void);

  void* operator new(std::size_t count) { return CmiAlloc(count); }

  void operator delete(void* blk) { CmiFree(blk); }

 private:
  static void handler_(back_inserter* msg);
};
}  // namespace hypercomm

#endif

#endif
