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

    CkCallback cb(CkIndex_main::run(NULL), thisProxy);
    CProxy_locality::ckNew(n, n, cb);
  }

  void run(CkArrayCreatedMsg* msg) {
    CProxy_locality localities(msg->aid);
    double avgTime = 0.0;

    auto root = conv2idx<CkArrayIndex>(0);
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

    CkPrintf("main> on average, each batch of %d iterations took: %f s\n",
             numIters, avgTime / numReps);

    CkExit();
  }
};

struct locality : public vil<CBase_locality, int> {
  entry_port_ptr bcast_port, redn_port;
  int n;
  section_ptr section;

  locality(int _1)
      : n(_1),
        bcast_port(std::make_shared<persistent_port>(0)),
        redn_port(std::make_shared<persistent_port>(1)) {
    // places a my_redn_com on all elements, then opens an entry port to it
    // so it can receive messages from other chares -- this is, effectively,
    // a dynamic entry method
    const auto& com1 = this->emplace_component<my_redn_com>(this);
    this->connect(bcast_port, com1, 0);
    this->activate_component(com1);
    // creates a (lightweight) section with only the even elements 
    std::vector<int> indices(n / 2);
    std::iota(std::begin(indices), std::end(indices), 0);
    std::transform(std::begin(indices), std::end(indices), std::begin(indices),
                   [](const int& i) { return i * 2; });
    this->section = sectionify(std::move(indices));
    // NOTE the say_hello "entry method" is only created on idx 0
    if (this->__index__() == 0) {
      const auto& com2 = this->emplace_component<say_hello>();
      this->connect(redn_port, com2, 0);
      this->activate_component(com2);
    }
  }

  void run(const int& numIters) {
    std::tuple<array_proxy::index_type, int> tmp;
    auto& idx = std::get<0>(tmp);
    idx = this->ckGetArrayIndex();
    std::get<1>(tmp) = numIters;
    // pack the root's address and the number of iters
    // (specify the destination port as bcast_port)
    auto msg = hypercomm::pack_to_port(bcast_port, tmp);
    // do a broadcast over the section to start the reductions
    this->broadcast(section, msg);
  }
};

// NOTE ( this is effectively the body for the say_hello entry method )
typename say_hello::value_set say_hello::action(value_set&& accepted) {
  if (kVerbose) {
    CkPrintf("com%lu> hi, hi!\n", id);
  }

  return {};
}

// NOTE ( this is effectively the body for the my_redn_com entry method )
typename my_redn_com::value_set my_redn_com::action(value_set&& accepted) {
  // the accepted pool contains the first message received by this action
  // TODO <- fix this up once consume is added ->
  using tuple_type = std::tuple<array_proxy::index_type, int>;
  // unpack it to an index/int pair
  auto tmp = value2typed<tuple_type>(std::move(accepted[0]));
  auto& idx = std::get<0>(tmp->value());
  auto& numIters = std::get<1>(tmp->value());
  // make the function and callback
  auto fn = std::make_shared<null_combiner>();
  auto cb = forward_to(self->thisProxy[idx], self->redn_port);
  // then do numIters reductions, each with empty messages
  for (auto it = 0; it < numIters; it++) {
#if CMK_DEBUG
    CkPrintf("com%lu@%d> contributing a value\n", this->id, self->__index__());
#endif
    auto val = hypercomm::make_unit_value();
    // NOTE ( this is equivalent to Charm++'s contribute but, sadly, we cannot overload it )
    self->local_contribution(self->section, std::move(val), fn, cb);
  }
  // no return value
  return {};
}

#define CK_TEMPLATES_ONLY
#include "tester.def.h"
#undef CK_TEMPLATES_ONLY

#include "tester.def.h"
