#include "tester.decl.h"

#include <hypercomm/core/locality.hpp>
#include <hypercomm/core/threading.hpp>

using namespace hypercomm;

void setup_environment(void) { hypercomm::thread::setup_isomalloc(); }

struct main : public CBase_main {
  int multiplier;

  main(CkArgMsg* m) : multiplier((m->argc <= 1) ? 4 : atoi(m->argv[1])) {
    auto np = CkNumPes();
    if (np < 2) CkAbort("this demo expects at least two PEs");

    int n = np * multiplier;

    CkPrintf("main> multiplier=%d, numPes=%d\n", multiplier, np);

    CProxy_locality localities = CProxy_locality::ckNew();
    // loads all the work onto our PE
    for (auto i = 0; i < n; i += 1) {
      localities[i].insert(CkMyPe());
    }

    localities.doneInserting();

    CkExitAfterQuiescence();
  }
};

struct locality : public CBase_locality {
  thread::manager thman;
  int tid;

  locality(void) : thman(this) {
    // enable sync load balancing
    this->usesAtSync = true;
    // create an empty message
    auto msg = hypercomm_msg::make_message(0, {});
    // create a thread within the manager
    auto thp = thman.emplace(&locality::run_, msg);
    // register it
    this->tid = thp.second;
    // add our listeners to it
    ((Chare*)this)->CkAddThreadListeners(thp.first, msg);
    // then launch it
    CthResume(thp.first);
  }

  locality(CkMigrateMessage* m) : thman(this) {}

  void pup(PUP::er& p) {
    p | thman;  // pup'ing the manager captures our threads
    p | tid;
  }

  static void run_(locality*& self, CkMessage* msg) {
    // free the message (before we migrate)
    CkFreeMsg(msg);
    // block quiescence detection
    QdCreate(1);
    // indicate we're ready to migrate
    self->AtSync();
    // print a message (as one does)
    auto idx = self->thisIndex;
    CkPrintf("ch%d@pe%d> I'm going to sleep...\n", idx, CkMyPe());
    // then suspend
    CthSuspend();
#if CMK_VERBOSE
    CkPrintf("th%p> was resumed with owner(%p) via %p.\n", CthSelf(), self,
             &self);
#endif
    // validate that (self) is still accessible
    if (idx != self->thisIndex) {
      ::CkAbort("ch%d> unsuccessful resume-from-sync! incorrect idx(%d)!\n",
                idx, self->thisIndex);
    } else {
      CkPrintf("ch%d@pe%d> I'm alive again~!\n", idx, CkMyPe());
    }
    // unblock quiescence detection
    QdProcess(1);
  }

  void ResumeFromSync(void) { this->thman.resume(this->tid); }
};

#define CK_TEMPLATES_ONLY
#include "tester.def.h"
#undef CK_TEMPLATES_ONLY

#include "tester.def.h"
