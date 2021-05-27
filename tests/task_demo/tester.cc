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
    // hypercomm::enroll<persistent_port>();
    // hypercomm::enroll<reduction_port<int>>();
    // hypercomm::enroll<broadcaster<int>>();
    // hypercomm::enroll<generic_section<int>>();
  }
}

// NOTE this mirrors the "secdest" Charm++ benchmark at:
//      https://github.com/Wingpad/charm-benchmarks/tree/main/secdest
struct main : public CBase_main {
  main(CkArgMsg* m) {
    auto n = kDecompFactor * CkNumPes();

    CkPrintf("main> kDecompFactor=%d, kNumPes=%d\n", kDecompFactor, CkNumPes());

    CProxy_locality::ckNew(n, n);

    CkExitAfterQuiescence();
  }
};

struct locality : public CBase_locality, public locality_base<int> {
  int n;

  locality(int _1): n(_1) {
    auto com0 = this->emplace_component<gen_random<value_type>>(n);
    auto com1 = this->emplace_component<print_values<value_type>>();

    connect(this, com0, com1);

    this->activate_component(com1);
    this->activate_component(com0);
  }

  // NOTE ( this is a mechanism for remote task invocation )
  virtual void execute(CkMessage* msg) override {
    action_type action{};
    hypercomm::unpack(msg, action);
    this->receive_action(action);
  }

  /* NOTE ( this is a mechanism for demux'ing an incoming message
   *        to the appropriate entry port )
   */
  virtual void demux(hypercomm_msg* msg) override {
    this->receive_value(msg->dst, hypercomm::utilities::wrap_message(msg));
  }
};

template<typename T>
typename gen_random<T>::value_t gen_random<T>::action(void) {
  auto msg_size = sizeof(T) * this->n + sizeof(this->n);
  auto msg = message::make_message(msg_size, {});
  *((decltype(this->n)*)msg->payload) = this->n;
  auto arr = (T*)(msg->payload + sizeof(this->n));

  for (auto i = 0; i < this->n; i += 1) {
    arr[i] = (T)(rand() % this->n + 1);
  }

  return utilities::wrap_message(msg);
}

template<typename T>
typename triple_values<T>::value_t triple_values<T>::action(void) {
  return {};
}

template<typename T>
typename print_values<T>::value_t print_values<T>::action(void) {
  auto msg = std::static_pointer_cast<message>(this->accepted[0]);
  auto& n = *((std::size_t*)msg->payload);
  auto* arr = (T*)(msg->payload + sizeof(n));

  std::stringstream ss;
  ss << "com" << this->id << "@loc?> ";
  ss << "[ ";
  for (auto i = 0; i < n; i += 1) {
    ss << arr[i] << " ";
  }
  ss << "]";

  ckout << ss.str().c_str() << endl;

  return msg;
}

#define CK_TEMPLATES_ONLY
#include "tester.def.h"
#undef CK_TEMPLATES_ONLY

#include "tester.def.h"
