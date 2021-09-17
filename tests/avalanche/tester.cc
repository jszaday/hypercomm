#include <charm++.h>

#include "tester.hh"

#include <ctime>
#include <cstdlib>

#ifndef DECOMP_FACTOR
#define DECOMP_FACTOR 4
#endif

constexpr auto kDecompFactor = DECOMP_FACTOR;

using value_type = float;

void enroll_polymorphs(void) {
  hypercomm::init_polymorph_registry();

  if (CkMyRank() == 0) {
    hypercomm::enroll<persistent_port>();
  }
}

static constexpr auto kMaxReps = 65;

/* readonly */ int numElements;

// NOTE this mirrors the "avalanche" Charm++ benchmark at:
//      https://github.com/jszaday/charm-benchmarks/tree/main/avalanche
struct main : public CBase_main {
  int numIters, numReps;
  CProxy_receiver receivers;
  CProxy_sender senders;

  main(CkArgMsg* m) : numIters(atoi(m->argv[1])), numReps(numIters / 2 + 1) {
    if (numReps > kMaxReps) numReps = kMaxReps;
    numElements = kDecompFactor * CkNumPes();

    CkPrintf("main> kDecompFactor=%d, kNumPes=%d\n", kDecompFactor, CkNumPes());

    receivers = CProxy_receiver::ckNew(numIters, numElements);
    senders = CProxy_sender::ckNew(numIters, receivers, numElements);

    thisProxy.run();
  }

  void run(void) {
    double avgTime = 0.0;

    for (int i = 0; i < numReps; i += 1) {
      auto start = CkWallTimer();

      senders.run();
      receivers.run();

      CkWaitQD();

      auto end = CkWallTimer();

      avgTime += end - start;
    }

    CkPrintf("on avg, exchange took: %f s\n", avgTime / numReps);

    CkExit();
  }
};

struct receiver : public vil<CBase_receiver, int> {
  comproxy<mailbox<int>> mbox;
  entry_port_ptr recv_port;

  int numIters;

  receiver(const int& _1)
      : numIters(_1), recv_port(std::make_shared<persistent_port>(0)),
        mbox(this->emplace_component<mailbox<int>>()) {
    this->connect(recv_port, this->mbox, 0);

    this->activate_component(this->mbox);
  }

  void run(void) {
    this->update_context();

    for (int i = 0; i < numIters; i++) {
      for (int j = 0; j < numElements; j++) {
        auto mtchr = std::make_shared<matcher<int>>(i);
        auto cb = std::make_shared<nil_callback>();
        this->mbox->put_request(mtchr, cb);
      }
    }
  }
};

struct sender : public vil<CBase_sender, int> {
  std::vector<std::shared_ptr<array_element_proxy>> receivers;
  entry_port_ptr recv_port;
  int numIters;

  sender(const int& _1, const CProxy_receiver& _2)
      : numIters(_1),
        recv_port(std::make_shared<persistent_port>(0)),
        receivers(numElements) {
    for (auto i = 0; i < numElements; i++) {
      receivers[i] = make_proxy(_2[conv2idx<CkArrayIndex>(i)]);
    }
  }

  void run(void) {
    for (int i = 0; i < numIters; i++) {
      for (int j = 0; j < numElements; j++) {
        send2port<CkArrayIndex>(receivers[j], recv_port,
                                make_typed_value<int>(i));
      }
    }
  }
};

#define CK_TEMPLATES_ONLY
#include "tester.def.h"
#undef CK_TEMPLATES_ONLY

#include "tester.def.h"
