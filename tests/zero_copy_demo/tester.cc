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
    auto value = make_typed_value<int>(numIters);
    double avgTime = 0.0;

    for (int i = 0; i < numReps; i += 1) {
      if ((i % 2) == 0) {
        CkPrintf("main> repetition %d of %d\n", i + 1, numReps);
      }

      auto start = CkWallTimer();

      localities.run(value->release());

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
struct big_data {
  static constexpr std::size_t kStep = 128;
  static constexpr std::size_t kGoal = kStep * 1024;
  static constexpr std::size_t kSize = kGoal / sizeof(T) + 1;

  static_assert((kSize * sizeof(T)) >= kZeroCopySize, "a big datum is not big enough!");

  static constexpr T kMagic = 0x69696969;
  T data[kSize];

  big_data(void) {
    auto end = data + kSize;
    for (auto it = data; it < end; it += kStep) {
      *it = kMagic;
    }
  }

  bool is_valid(void) const {
    auto end = data + kSize;
    auto valid = true;
    for (auto it = data; valid && it < end; it += kStep) {
      valid = valid && (*it == kMagic);
    }
    return valid;
  }
};

template<typename T>
struct validate_data : public hypercomm::component {
  validate_data(const id_t& _1) : component(_1) {}

  virtual std::size_t n_inputs(void) const override { return 1; }

  virtual std::size_t n_outputs(void) const override { return 0; }

  virtual bool keep_alive(void) const override {
    return true;
  }

  virtual value_set action(value_set&& values) override {
    auto val = std::move(values[0]);
    auto is_buf = dynamic_cast<buffer_value*>(val.get()) != nullptr;
    if (is_buf) {
      CkPrintf("com%lu> big data has arrived... via zerocopy!.\n", this->id);
      auto typed = value2typed<big_data<T>>(std::move(val));
      auto valid = (*typed)->is_valid();
      CkEnforceMsg(valid, "bad data!");
    }
    return {};
  }
};

using data_type = int;
PUPbytes(big_data<data_type>);

struct locality : public vil<CBase_locality, int> {
  entry_port_ptr port;
  component::id_t com;
  int n;

  locality(int _1)
  : n(_1), port(std::make_shared<persistent_port>(0x420)),
    com(this->emplace_component<validate_data<data_type>>()) {
    this->connect(port, com, 0);
    this->activate_component(com);
  }

  void run(CkMessage* msg) {
    const auto numIters = typed_value<int>::from_message(msg)->value();
    for (auto it = 0; it < numIters; it++) {
      auto mem = make_typed_value<big_data<data_type>>();
      auto idx = conv2idx<CkArrayIndex>((reinterpret_index<int>(thisIndexMax) + it) % n);
      interceptor::send_async(thisProxy[idx], port, std::move(mem));
    }
  }
};

#define CK_TEMPLATES_ONLY
#include "tester.def.h"
#undef CK_TEMPLATES_ONLY

#include "tester.def.h"
