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

template<typename T>
struct big_chungus: public hyper_value {
  static constexpr std::size_t kStep = 128;
  static constexpr std::size_t kGoal = kStep * 1024;
  static constexpr std::size_t kSize = kGoal / sizeof(T) + 1;
  static constexpr T kMagic = 0x69696969;
  T data[kSize];

  big_chungus(void) {
    auto end = data + kSize;
    for (auto it = data; it < end; it += kStep) {
      *it = kMagic;
    }
  }

  static bool is_valid(T* data) {
    auto end = data + kSize;
    auto valid = true;
    for (auto it = data; valid && it < end; it += kStep) {
      valid = valid && (*it == kMagic);
    }
    return valid;
  }

  virtual bool recastable(void) const { return false; }
  virtual message_type release(void) { return nullptr; }

  virtual std::pair<void*, std::size_t> as_nocopy(void) const {
    return std::make_pair(const_cast<int*>(data), kSize * sizeof(T));
  }
};

template<typename T>
struct validate_chunga : public hypercomm::component {
  validate_chunga(const id_t& _1) : component(_1) {}

  virtual std::size_t n_inputs(void) const override { return 1; }

  virtual std::size_t n_outputs(void) const override { return 0; }

  virtual bool keep_alive(void) const override {
    return true;
  }

  virtual value_set action(value_set&& values) override {
    auto val = std::move(values[0]);
    auto buf = dynamic_cast<buffer_value*>(val.get());
    if (buf != nullptr) {
      CkPrintf("com%lu> big chungus has arrived... via zerocopy!.\n", this->id);
      auto valid = big_chungus<T>::is_valid(buf->payload<T>());
      CkEnforceMsg(valid, "bad chunga!");
    }
    return {};
  }
};

using wunga_type = int;

struct locality : public vil<CBase_locality, int> {
  entry_port_ptr port;
  component::id_t com;
  int n;

  locality(int _1)
  : n(_1), port(std::make_shared<persistent_port>(0x420)),
    com(this->emplace_component<validate_chunga<wunga_type>>()) {
    this->connect(port, com, 0);
    this->activate_component(com);
  }

  void run(CkMessage* msg) {
    const auto numIters = typed_value<int>::from_message(msg)->value();
    for (auto it = 0; it < numIters; it++) {
      auto mem = make_value<big_chungus<wunga_type>>();
      auto idx = conv2idx<CkArrayIndex>((reinterpret_index<int>(thisIndexMax) + it) % n);
      interceptor::send_async(thisProxy[idx], port, std::move(mem));
    }
  }
};

#define CK_TEMPLATES_ONLY
#include "tester.def.h"
#undef CK_TEMPLATES_ONLY

#include "tester.def.h"
