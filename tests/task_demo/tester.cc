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

struct locality : public vil<CBase_locality, int> {
  int n, selfIdx;
  entry_port_ptr recv_value;

  // this implements the Charisma program:
  //
  // val values = or::placeholder<float, 1>(n); // creates a fixed-size (n) 1d channel
  // val sums = or::placeholder<float, 1>(n);
  //
  // for (i in 0 to n) {
  //   values[i] = self@[i].gen_values();  // equivalent to charm++'s thisProxy[i]
  //   print_values(values[i]);
  //   sums[i] = self@[i].add_values(values[i], values[(i + 1) % n]);
  //   print_values(sums[i]);
  // }

  locality(int _1)
      : selfIdx(this->__index__()),
        n(_1),
        // this should probably be a temporary port
        recv_value(std::make_shared<persistent_port>(0)) {
    auto com0 = this->emplace_component<gen_values<value_type>>(selfIdx, n);
    auto com1 = this->emplace_component<print_values<value_type>>(selfIdx);
    auto com2 = this->emplace_component<add_values<value_type>>();
    auto com3 = this->emplace_component<print_values<value_type>>(selfIdx);

    // gen random => print values 1
    this->connect(com0, com1);
    // print values 1 => add values
    this->connect(com1, com2);
    // print values 1 => recv_value@neighbor
    auto neighborIdx = conv2idx<CkArrayIndexMax>((selfIdx + 1) % n);
    auto neighbor = make_proxy(thisProxy[neighborIdx]);
    forward(com1, neighbor, recv_value);
    // recv_value@here => add values
    this->connect(recv_value, com2);
    // add_values => print values 2
    this->connect(com2, com3);

    this->activate_component(com3);
    this->activate_component(com2);
    this->activate_component(com1);
    this->activate_component(com0);
  }


};

template <typename T>
typename gen_values<T>::value_type gen_values<T>::action(void) {
  auto msg_size = sizeof(T) * this->n + sizeof(this->n);
  auto msg = message::make_message(msg_size, {});
  *((decltype(this->n)*)msg->payload) = this->n;
  auto arr = (T*)(msg->payload + sizeof(this->n));

  for (auto i = 0; i < this->n; i += 1) {
    arr[i] = (T)(i % this->n + this->selfIdx + 1);
  }

  return utilities::wrap_message(msg);
}

template <typename T>
typename add_values<T>::value_type add_values<T>::action(void) {
  auto& lhsMsg = this->accepted[0];
  auto& rhsMsg = this->accepted[1];

  std::size_t* m;
  T* lhs;
  unpack_array(lhsMsg, &m, &lhs);

  std::size_t* n;
  T* rhs;
  unpack_array(rhsMsg, &n, &rhs);

  if (*m != *n) {
    CkAbort("add_values> array size mismatch");
  }

  for (auto i = 0; i < *n; i += 1) {
    lhs[i] += rhs[i];
  }

  return lhsMsg;
}

template <typename T>
typename print_values<T>::value_type print_values<T>::action(void) {
  std::size_t* n;
  T* arr;
  const auto& msg = this->accepted[0];
  unpack_array(msg, &n, &arr);

  std::stringstream ss;
  ss << "com" << this->id << "@vil" << selfIdx << "> ";
  ss << "[ ";
  for (auto i = 0; i < *n; i += 1) {
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
