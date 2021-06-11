#include <charm++.h>

#include "tester.hh"

#ifndef DECOMP_FACTOR
#define DECOMP_FACTOR 4
#endif

constexpr auto kMaxReps = 129;
constexpr auto kDecompFactor = DECOMP_FACTOR;
constexpr auto kVerbose = false;

void enroll_polymorphs(void) {
  hypercomm::init_polymorph_registry();

  if (CkMyRank() == 0) {
    hypercomm::enroll<future_port>();
    hypercomm::enroll<port_opener<int>>();
    hypercomm::enroll<forwarding_callback>();
  }
}

// NOTE this mirrors the "secdest" Charm++ benchmark at:
//      https://github.com/Wingpad/charm-benchmarks/tree/main/secdest
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
    CProxy_locality localities(msg->aid);
    double avgTime = 0.0;

    auto root = conv2idx<CkArrayIndexMax>(0);
    for (int i = 0; i < numReps; i += 1) {
      if ((i % 2) == 0) {
        CkPrintf("main> repetition %d of %d\n", i + 1, numReps);
      }

      auto start = CkWallTimer();

      localities.run(numIters);

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
  int n;

  locality(int _1) : n(_1) {}

  void run(const int& numIters) {
    const auto& mine = this->__index__();
    const auto right = (*this->__proxy__())[conv2idx<CkArrayIndex>((mine + 1) % n)];

    // this sets up a callback that wakes up the current thread
    // then sets the designated pointer to the received value
    auto recvd = resuming_callback::value_type{};
    const auto cb = std::make_shared<resuming_callback>(CthSelf(), &recvd);

    // for each iteration:
    for (auto i = 0; i < numIters; i += 1) {
      // make a future
      auto f = this->make_future();
      // get the handle to its remote counterpart (at our right neighbor)
      // NOTE in the future, we can do something fancier here (i.e., iteration-based offset)
      auto g = future { .source = right, .id = f.id };
      // prepare and send a message
      f.set(hypercomm_msg::make_message(0, {}));
      // request the remote value -> our callback
      this->request_future(g, cb);
      // then suspend
      CthSuspend();
    }
  }
};

#define CK_TEMPLATES_ONLY
#include "tester.def.h"
#undef CK_TEMPLATES_ONLY

#include "tester.def.h"
