#include "tester.hh"

#include <charm++.h>

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

struct configuration {
  const int numReps;
  const int numIters;
  const int numSkip;
  const int senderIdx;
  const int receiverIdx;

  configuration(const int& _1, const int& _2, const int& _3)
      : numReps(_1), numSkip(_2), numIters(_3), senderIdx(0), receiverIdx(1) {}
};

PUPbytes(configuration);

struct main : public CBase_main {
  int numIters, numReps;

  main(CkArgMsg* m)
      : numIters((m->argc <= 1) ? 100 : atoi(m->argv[1])),
        numReps(numIters / 2 + 1) {
    if (numReps > kMaxReps) numReps = kMaxReps;

    CkEnforceMsg((CkNumPes() == 2) && "expected only two communicators!");
    auto n = 2;

    CkPrintf("main> numReps=%d, numIters=%d\n", numReps, numIters);

    CkCallback cb(CkIndex_main::run(NULL), thisProxy);
    CProxy_locality::ckNew(n, n, cb);
  }

  void run(CkArrayCreatedMsg* msg) {
    auto localities = CProxy_locality(msg->aid);
    auto value = make_typed_value<configuration>(numReps, 10, numIters);

    localities.run(value->release());
  }
};

template <typename T>
struct big_data {
  static constexpr std::size_t kStep = 128;
  static constexpr std::size_t kGoal = kStep * 1024;
  static constexpr std::size_t kSize = kGoal / sizeof(T) + 1;

  static_assert((kSize * sizeof(T)) >= kZeroCopySize,
                "a big datum is not big enough!");

  static constexpr T kMagic = 0x69696969;
  T data[kSize];

  big_data(const bool& init) {
    auto end = data + kSize;
    for (auto it = data; init && it < end; it += kStep) {
      *it = kMagic;
    }
  }

  bool is_valid(void) const {
    auto valid = true;
    auto end = data + kSize;
    for (auto it = data; valid && it < end; it += kStep) {
      valid = valid && (*it == kMagic);
    }
    return valid;
  }
};

using data_type = int;
PUPbytes(big_data<data_type>);

struct locality : public vil<CBase_locality, int> {
  entry_port_ptr port;
  comproxy<mailbox<big_data<data_type>>> mb;
  int n;

  locality(int _1)
      : n(_1),
        port(std::make_shared<persistent_port>(0x420)),
        mb(this->emplace_component<mailbox<big_data<data_type>>>()) {
    this->connect(port, mb, 0);
    this->activate_component(mb);
  }

  void run(CkMessage* msg) {
    auto size = big_data<data_type>::kSize * sizeof(data_type);
    auto cfg = typed_value<configuration>::from_message(msg)->value();
    auto senderIdx = conv2idx<CkArrayIndex>(cfg.senderIdx);
    auto receiverIdx = conv2idx<CkArrayIndex>(cfg.receiverIdx);
    auto& numIters = cfg.numIters;
    auto& numReps = cfg.numReps;
    auto& numSkip = cfg.numSkip;
    auto& mine = this->__index__();
    auto sender = mine == cfg.senderIdx;

    // return the value to the sender
    auto rts = sender ? nullptr
                      : std::make_shared<forwarding_callback<CkArrayIndex>>(
                            make_proxy(thisProxy[senderIdx]), port);

    // implements a primitive form of completion detection
    auto senti = sender ? std::make_shared<sentinel_callback>() : nullptr;
    // creates an appropriately sized buffer for our data
    std::shared_ptr<big_data<data_type>> buffer(
      sender ? new big_data<data_type>(sender) : nullptr
    );

    double startTime = 0;
    for (auto rep = 0; rep < (numReps + numSkip); rep++) {
      if (rep == numSkip) {
        startTime = CkWallTimer();
      }

      // bounce numIters messages back and forth between the communicators
      for (auto it = 0; it < numIters; it++) {
        if (sender) {
          // use the same buffer for send/receive
          this->post_buffer(port, buffer, size);
          auto mem = typed_value<big_data<data_type>>::from_buffer(buffer);
          interceptor::send_async(thisProxy[receiverIdx], port, std::move(mem));
          this->mb->put_request({}, senti);
        } else {
          this->mb->put_request({}, rts);
        }
      }

      // and we wait for the return messages at the sender-side
      if (sender) {
        senti->wait(numIters);
      }
    }

    double tmp;
    double endTime = CkWallTimer();
    double time = endTime - startTime;
    tmp = ((2.0 * size) / (1000 * 1000)) * numIters * numReps;

    CkAssert(this->buffers[port].empty());

    if (sender) {
      CkPrintf("%d> avg bandwidth for %lu was %g MiB/s\n", mine, size,
               tmp / time);

      CkExit();
    }
  }
};

#define CK_TEMPLATES_ONLY
#include "tester.def.h"
#undef CK_TEMPLATES_ONLY

#include "tester.def.h"
