#include <charm++.h>

#include <hypercomm/core/threading.hpp>

#include "tester.hh"

#ifndef DECOMP_FACTOR
#define DECOMP_FACTOR 4
#endif

constexpr auto kMaxReps = 129;
constexpr auto kDecompFactor = DECOMP_FACTOR;
constexpr auto kVerbose = false;

void enroll_polymorphs(void) {
  hypercomm::init_polymorph_registry();

  hypercomm::thread::setup_isomalloc();

  if (CkMyRank() == 0) {
    hypercomm::enroll<future_port>();
    hypercomm::enroll<port_opener>();
    hypercomm::enroll<forwarding_callback<CkArrayIndex>>();
  }
}

struct main : public CBase_main {
  int numIters, numReps;

  main(CkArgMsg* m)
      : numIters((m->argc <= 1) ? 4 : atoi(m->argv[1])),
        numReps(numIters / 2 + 1) {
    if (numReps > kMaxReps) numReps = kMaxReps;
    auto n = kDecompFactor * CkNumPes();

    CkPrintf("main> kDecompFactor=%d, kNumPes=%d\n", kDecompFactor, CkNumPes());

    CkCallback cb(CkIndex_main::run(NULL), thisProxy);
    CProxy_locality::ckNew(n, n, cb);
  }

  void run(CkArrayCreatedMsg* msg) {
    auto localities = CProxy_locality(msg->aid);
    auto value = typed_value<int>(numIters);
    double avgTime = 0.0;

    for (int i = 0; i < numReps; i += 1) {
      if ((i % 2) == 0) {
        CkPrintf("main> repetition %d of %d\n", i + 1, numReps);
      }

      auto start = CkWallTimer();

      localities.run(value.release());

      CkWaitQD();

      auto end = CkWallTimer();

      avgTime += end - start;
    }

    CkPrintf("main> on average, each batch of %d iterations took: %f s\n",
             numIters, avgTime / numReps);

    CkExit();
  }
};

struct locality : public vil<CBase_locality, int> {
  thread::manager thman;
  int n;

  locality(int _1) : n(_1), thman(this) {}

  void pup(PUP::er& p) {
    if (p.isUnpacking()) thman.set_owner(this);
    p | thman;  // pup'ing the manager captures our threads

    p | n;
  }

  void run(CkMessage* msg) {
    // create a thread within the manager
    auto thp = thman.emplace(&locality::run_, msg);
    auto& th = thp.first;
    // add our listeners to it
    ((Chare*)this)->CkAddThreadListeners(th, msg);
    // then launch it
    CthResume(th);
  }

  static void run_(locality* &self, CkMessage* msg) {
    const auto numIters = typed_value<int>::from_message(msg)->value();
    const auto& mine = self->__index__();
    const auto right =
        (*self->__proxy__())[conv2idx<CkArrayIndex>((mine + 1) % self->n)];

    // update the context at the outset
    self->update_context();

    // for each iteration:
    for (auto i = 0; i < numIters; i += 1) {
      // make a future
      auto f = self->make_future();
      // get the handle to its remote counterpart (at our right neighbor)
      // NOTE in the future, we can do something fancier here (i.e., iteration-based offset)
      auto g = future{.source = right, .id = f.id};
      // prepare and send a message
      f.set(message::make_message(0, {}));
      // request the remote value -> our callback
      auto cb = std::make_shared<resuming_callback<unit_type>>(CthSelf());
      self->request_future(g, cb);
      // suspend if necessary
      if (!cb->ready()) {
        CthSuspend();
      }
      // update the context after resume
      self->update_context();
    }
  }
};

#define CK_TEMPLATES_ONLY
#include "tester.def.h"
#undef CK_TEMPLATES_ONLY

#include "tester.def.h"
