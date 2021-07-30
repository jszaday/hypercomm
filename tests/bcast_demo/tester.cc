#include <charm++.h>

#include "tester.hh"

#ifndef DECOMP_FACTOR
#define DECOMP_FACTOR 4
#endif

constexpr auto kMaxReps = 129;
constexpr auto kDecompFactor = DECOMP_FACTOR;
constexpr auto kVerbose = false;

std::vector<int> kIndices;

void enroll_polymorphs(void) {
  hypercomm::init_polymorph_registry();

  if (CkMyRank() == 0) {
    hypercomm::enroll<reduction_port<int>>();
    hypercomm::enroll<broadcaster<CkArrayIndex, int>>();
    hypercomm::enroll<generic_section<int>>();
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

    kIndices.resize(n);
    std::iota(std::begin(kIndices), std::end(kIndices), 0);

    CkCallback cb(CkIndex_main::run(NULL), thisProxy);
    CProxy_locality::ckNew(n, n, cb);
  }

  void run(CkArrayCreatedMsg* msg) {
    CProxy_locality localities(msg->aid);
    double avgTime = 0.0;

    auto proxy = make_proxy(localities);
    auto sect = std::make_shared<vector_section<int>>(kIndices);
    auto epIdx = CkIndex_locality::run(nullptr);

    for (int i = 0; i < numReps; i += 1) {
      if ((i % 2) == 0) {
        CkPrintf("main> repetition %d of %d\n", i + 1, numReps);
      }

      auto start = CkWallTimer();

      auto f = CkCreateFuture();
      auto msg = hypercomm::pack_to_port({}, CkCallback(f));
      broadcast_to<int>(proxy, sect, epIdx, msg);
      CkFreeMsg(CkWaitFuture(f));
      CkReleaseFuture(f);

      auto end = CkWallTimer();

      avgTime += end - start;
    }

    CkPrintf("main> on average, each batch of %d iterations took: %f s\n",
             numIters, avgTime / numReps);

    CkExit();
  }
};

struct locality : public vil<CBase_locality, int> {
  section_ptr section;
  CkCallback cb;
  int n, nRecvd;

  locality(int n_) : n(n_), nRecvd(0), section(std::make_shared<vector_section<int>>(kIndices)) {}

  void run(CkMessage* input) {
    hypercomm::unpack(input, cb);
    auto epIdx = CkIndex_locality::recv_broadcast();
    auto msg = hypercomm_msg::make_message(0x0, {});
    this->broadcast(this->section, epIdx, msg);
  }

  void recv_broadcast(void) {
    auto nExptd = (int)kIndices.size();

    this->nRecvd += 1;

    if (this->nRecvd == nExptd) {
      this->nRecvd = 0;

      auto val = hypercomm::make_unit_value();
      auto fn = std::make_shared<null_combiner>();
      auto cb = hypercomm::intercall(this->cb);

      this->local_contribution(this->section, std::move(val), fn, cb);
    }
  }
};

#define CK_TEMPLATES_ONLY
#include "tester.def.h"
#undef CK_TEMPLATES_ONLY

#include "tester.def.h"
