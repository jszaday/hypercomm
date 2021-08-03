#ifndef __HYPERCOMM_MESSAGING_INTERCEPT_MSG_HPP__
#define __HYPERCOMM_MESSAGING_INTERCEPT_MSG_HPP__

#include <hypercomm/messaging/messaging.decl.h>

namespace hypercomm {

extern CProxy_interceptor interceptor_;

struct intercept_msg {
  char core[CmiReservedHeaderSize];

  CkArrayID aid;
  CkArrayIndex idx;
  CkMessage* msg;

  intercept_msg(const CkArrayID& _1, const CkArrayIndex& _2, CkMessage* _3)
      : aid(_1), idx(_2), msg(_3) {
    CmiSetHandler(this, handler());
  }

  static const int& handler(void);

  void* operator new(std::size_t count) { return CmiAlloc(count); }

  void operator delete(void* blk) { CmiFree(blk); }

 private:
  static void handler_(intercept_msg* msg);
};
}

#endif
