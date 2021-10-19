#ifndef __HYPERCOMM_MESSAGING_DELIVERY_HPP__
#define __HYPERCOMM_MESSAGING_DELIVERY_HPP__

#include "destination.hpp"
#include "deliverable.hpp"

namespace hypercomm {

struct delivery {
  char core[CmiReservedHeaderSize];

  CkArrayID aid;
  CkArrayIndex idx;
  deliverable payload;

  delivery(const CkArrayID& _1, const CkArrayIndex& _2, deliverable&& _3)
      : aid(_1), idx(_2), payload(std::move(_3)) {
    CkAssert((bool)this->payload);
    std::fill(this->core, this->core + CmiReservedHeaderSize, '\0');
    CmiSetHandler(this, handler());
  }

  static const int& handler(void);

  void* operator new(std::size_t count) { return CmiAlloc(count); }

  void operator delete(void* blk) { CmiFree(blk); }

  static void process(ArrayElement*, deliverable&&, bool);

 private:
  static void handler_(delivery* msg);
};
}  // namespace hypercomm

#endif
