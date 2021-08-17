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
  }
}

// NOTE this mirrors the "secdest" Charm++ benchmark at:
//      https://github.com/Wingpad/charm-benchmarks/tree/main/secdest
struct main : public CBase_main {
  int numIters, numReps;

  main(CkArgMsg* m)
      : numIters((m->argc <= 1) ? 4 : atoi(m->argv[1])),
        numReps(numIters / 2 + 1) {
    numIters = (numIters % 2) ? numIters - 1 : numIters;
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
  entry_port_ptr foo_port, bar_port, baz_port;
  comproxy<mailbox<int>> foo_mailbox, bar_mailbox, baz_mailbox;
  int n, repNo;
  section_ptr section;

  locality(int _1)
      : n(_1),
        repNo(0),
        foo_port(std::make_shared<persistent_port>(0)),
        bar_port(std::make_shared<persistent_port>(1)),
        baz_port(std::make_shared<persistent_port>(2)),
        foo_mailbox(this->emplace_component<mailbox<int>>()),
        bar_mailbox(this->emplace_component<mailbox<int>>()),
        baz_mailbox(this->emplace_component<mailbox<int>>()) {
    this->connect(this->foo_port, this->foo_mailbox, 0);
    this->connect(this->bar_port, this->bar_mailbox, 0);
    this->connect(this->baz_port, this->baz_mailbox, 0);

    this->activate_component(this->foo_mailbox);
    this->activate_component(this->bar_mailbox);
    this->activate_component(this->baz_mailbox);
  }

  void run(const int& numIters) {
    const auto& mine = this->__index__();

    this->update_context();

    auto leftIdx = (mine + this->n - 1) % this->n;
    auto left = make_proxy(thisProxy[conv2idx<CkArrayIndex>(leftIdx)]);

    auto rightIdx = (mine + 1) % this->n;
    auto right = make_proxy(thisProxy[conv2idx<CkArrayIndex>(rightIdx)]);

#if CMK_VERBOSE
    CkPrintf("vil%d> splitting %d values between %d and %d.\n", mine,
             numIters * 2, leftIdx, rightIdx);
#endif

    for (auto i = 0; i < numIters; i += 1) {
      if (i % 2 == 0) {
        send2port<CkArrayIndex>(left, this->foo_port,
                                make_value<typed_value<int>>(mine));

        send2port<CkArrayIndex>(left, this->bar_port,
                                make_value<typed_value<int>>(mine));
      } else {
        send2port<CkArrayIndex>(left, this->baz_port,
                                make_value<typed_value<int>>(mine));
      }
    }

    /* This implements SDAG-like logic along the following lines:
     *
     *  forall [i] (0:(numIters - 1),1) {
     *    case {
     *      when baz(...) { ... }            // com0
     *      when foo(...), bar(...) { ... }  // com1
     *    }
     *  }
     * 
     */

    this->repNo += 1;
    auto senti = std::make_shared<sentinel>((component::id_t)this->repNo);

    for (auto i = 0; i < numIters; i += 1) {
      // the constructor argument represents the nbr of inputs
      auto com0 = this->emplace_component<test_component>(1);
      auto com1 = this->emplace_component<test_component>(2);

      // no pattern-matching is necessary, so the predicate is
      // null. additionally, each of these connect to a port
      // of a component (usually formatted as, e.g., com1:0)
      this->foo_mailbox->put_request_to({}, com1, 0);
      this->bar_mailbox->put_request_to({}, com1, 1);
      this->baz_mailbox->put_request_to({}, com0, 0);

      // the sentinel requires only one of com0 or com1 to pass
      senti->expect_any(com0, com1);

      this->activate_component(com1);
      this->activate_component(com0);
    }

    // this forms the implicit barrier at the end of the forall
    senti->suspend();
  }
};

#define CK_TEMPLATES_ONLY
#include "tester.def.h"
#undef CK_TEMPLATES_ONLY

#include "tester.def.h"
