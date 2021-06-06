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
    hypercomm::enroll<persistent_port>();
    hypercomm::enroll<reduction_port<int>>();
    hypercomm::enroll<broadcaster<int>>();
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

    CkCallback cb(CkIndex_main::run(NULL), thisProxy);
    CProxy_locality::ckNew(n, n, cb);
  }

  void run(CkArrayCreatedMsg* msg) {
    CProxy_locality localities(msg->aid);
    double avgTime = 0.0;

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
  entry_port_ptr foo_port, bar_port;
  std::shared_ptr<mailbox<int>> foo_mailbox, bar_mailbox;
  int n;
  section_ptr section;

  locality(int _1)
      : n(_1),
        foo_port(std::make_shared<persistent_port>(0)),
        bar_port(std::make_shared<persistent_port>(1)) {
    this->foo_mailbox = std::dynamic_pointer_cast<mailbox<int>>(
        this->emplace_component<mailbox<int>>());
    this->bar_mailbox = std::dynamic_pointer_cast<mailbox<int>>(
        this->emplace_component<mailbox<int>>());

    this->connect(this->foo_port, this->foo_mailbox, 0);
    this->connect(this->bar_port, this->bar_mailbox, 0);

    this->activate_component(this->foo_mailbox);
    this->activate_component(this->bar_mailbox);
  }

  void run(const int& numIters) {
    const auto& mine = this->__index__();

    auto leftIdx = (mine + this->n - 1) % this->n;
    auto left = make_proxy(thisProxy[conv2idx<CkArrayIndex>(leftIdx)]);

    auto rightIdx = (mine + 1) % this->n;
    auto right = make_proxy(thisProxy[conv2idx<CkArrayIndex>(rightIdx)]);

    CkPrintf("vil%d> splitting %d values between %d and %d.\n", mine, numIters, leftIdx, rightIdx);

    for (auto i = 0; i < numIters; i += 1) {
      // foo foo bar bar ... foo foo bar bar ...
      const auto& port = ((i % 4) >= 2) ? this->bar_port : this->foo_port;
      // if even send to left, else right
      if (i % 2 == 0) {
        send2port<CkArrayIndex>(left, port,
                                std::make_shared<typed_value<int>>(mine));
      } else {
        send2port<CkArrayIndex>(right, port,
                                std::make_shared<typed_value<int>>(mine));
      }
    }

    auto senti = std::make_shared<sentinel>();
    for (auto i = 0; i < numIters; i += 1) {
      const auto& mbox = (i % 2 == 0) ? this->foo_mailbox : this->bar_mailbox;
      auto com = this->emplace_component<test_component>(1);

      mbox->put_request({},
        std::make_shared<connector>(this, std::make_pair(com->id, 0)));

      senti->expect_all(com);

      this->activate_component(com);
    }
    senti->suspend();
  }
};

#define CK_TEMPLATES_ONLY
#include "tester.def.h"
#undef CK_TEMPLATES_ONLY

#include "tester.def.h"
