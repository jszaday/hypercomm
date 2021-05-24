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

  main(CkArgMsg* m): numIters((m->argc <= 1) ? 4 : atoi(m->argv[1])), numReps(numIters / 2 + 1) {
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

      localities[root].run(numIters);

      CkWaitQD();

      auto end = CkWallTimer();

      avgTime += end - start;
    }

    CkPrintf("main> on average, each batch of %d iterations took: %f s\n", numIters, avgTime / numReps);

    CkExit();
  }
};

struct locality : public CBase_locality, public locality_base<int> {
  entry_port_ptr bcast_port, redn_port;
  int n;
  section_ptr section;

  locality(int _1)
  : n(_1),
    bcast_port(std::make_shared<persistent_port>(0)),
    redn_port(std::make_shared<persistent_port>(1)) {
    const auto& com1 = this->emplace_component<my_redn_com>(this);
    auto com1_in = com1->open_in_port();
    this->open(bcast_port, std::make_pair(com1->id, com1_in));
    this->activate_component(com1);

    std::vector<int> indices(n / 2);
    std::iota(std::begin(indices), std::end(indices), 0);
    std::transform(std::begin(indices), std::end(indices), std::begin(indices), [](const int& i) {
      return i * 2;
    });
    this->section = sectionify(std::move(indices));

    if (this->__index__() == 0) {
      const auto& com2 = this->emplace_component<say_hello>();
      auto com2_in = com2->open_in_port();
      this->open(redn_port, std::make_pair(com2->id, com2_in));
      this->activate_component(com2);
    }
  }

  void run(const int& numIters) {
    std::tuple<array_proxy::index_type, int> tmp;
    auto& idx = std::get<0>(tmp);
    idx = this->ckGetArrayIndex();
    std::get<1>(tmp) = numIters;

    auto sz  = size(tmp);
    auto msg = hypercomm_msg::make_message(sz, bcast_port);
    auto pkr = serdes::make_packer(msg->payload);
    hypercomm::pup(pkr, tmp);

    this->broadcast(section, msg);
  }

  virtual void execute(CkMessage* _1) override {
    action_type action{};
    auto msg = utilities::wrap_message(_1);
    auto unpack = serdes::make_unpacker(msg, utilities::get_message_buffer(msg));
    hypercomm::pup(unpack, action);
    this->receive_action(action);
  }

  virtual void demux(hypercomm_msg* msg) override {
    this->receive_value(msg->dst, hypercomm::utilities::wrap_message(msg));
  }
};

typename say_hello::value_t say_hello::action(void) {
  if (kVerbose) {
    CkPrintf("com%lu> hi, hi!\n", id);
  }

  return {};
}

typename my_redn_com::value_t my_redn_com::action(void) {
  auto& head = this->accepted[0];

  std::tuple<array_proxy::index_type, int> tmp;
  auto& idx = std::get<0>(tmp);
  auto& numIters = std::get<1>(tmp);
  auto pkr = serdes::make_unpacker(head, utilities::get_message_buffer(head));
  hypercomm::pup(pkr, tmp);

  auto fn = std::make_shared<nop_combiner>();
  auto cb =
      std::make_shared<forwarding_callback>(make_proxy(self->thisProxy[idx]), self->redn_port);

  for (auto it = 0; it < numIters; it++) {
#if CMK_DEBUG
    CkPrintf("com%lu@%d> contributing a value\n", this->id, self->__index__());
#endif
    auto msg = hypercomm_msg::make_message(0x0, {});
    self->local_contribution(self->section, hypercomm::utilities::wrap_message(msg), fn, cb);
  }

  return {};
}

#define CK_TEMPLATES_ONLY
#include "tester.def.h"
#undef CK_TEMPLATES_ONLY

#include "tester.def.h"
