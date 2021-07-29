#include <charm++.h>

#include "tester.hh"

#include <ctime>
#include <cstdlib>

using value_type = float;

void enroll_polymorphs(void) {
  hypercomm::init_polymorph_registry();

  if (CkMyRank() == 0) {
    hypercomm::enroll<persistent_port>();
    hypercomm::enroll<forwarding_callback<CkArrayIndex>>();
  }
}

struct config_type {
  using chare_type = ChareType;

  chare_type ty;

  int decompFactor;
  int n;
};

struct main : public CBase_main {
  main(CkArgMsg* m) {
    config_type cfg{.ty = TypeGroup, .decompFactor = 0, .n = CkNumPes()};

    for (auto i = 1; i < m->argc; i += 1) {
      if (strcmp("-nn", m->argv[i]) == 0) {
        cfg = {.ty = TypeNodeGroup, .decompFactor = 0, .n = CkNumNodes()};
        break;
      } else if (strcmp("-np", m->argv[i]) == 0) {
        break;
      } else if (strcmp("-nd", m->argv[i]) == 0) {
        cfg = {.ty = TypeArray,
               .decompFactor = atoi(m->argv[++i]),
               .n = CkNumPes()};
        cfg.n *= cfg.decompFactor;
        break;
      }
    }

    switch (cfg.ty) {
      case config_type::chare_type::TypeGroup:
        CkPrintf("main> group-like, numPes=%d\n", CkNumPes());
        make_grouplike<CProxy_locality>(cfg.n);
        break;
      case config_type::chare_type::TypeNodeGroup:
        CkPrintf("main> nodegroup-like, numNodes=%d, numPes=%d\n", CkNumNodes(), CkNumPes());
        make_nodegrouplike<CProxy_locality>(cfg.n);
        break;
      case config_type::chare_type::TypeArray:
        CkPrintf("main> array-like, numElements=%d, numPes=%d\n", cfg.n,
                 CkNumPes());
        CProxy_locality::ckNew(cfg.n, cfg.n);
        break;
      default:
        CkAbort("error> unexpected chare type!!");
    }

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

    // gen_values.(0) => print values 1
    this->connect(com0, 0, com1, 0);
    // gen_values.(1) => add values
    this->connect(com0, 1, com2, 0);
    // gen_values.(2) => recv_value@neighbor
    auto neighborIdx = conv2idx<CkArrayIndex>((selfIdx + 1) % n);
    auto fwd = forward_to(thisProxy[neighborIdx], recv_value);
    this->connect(com0, 2, fwd);
    // recv_value@here => add values
    this->connect(recv_value, com2, 1);
    // add_values => print values 2
    this->connect(com2, 0, com3, 0);

    this->activate_component(com3);
    this->activate_component(com2);
    this->activate_component(com1);
    this->activate_component(com0);
  }
};

template <typename T>
typename gen_values<T>::value_set gen_values<T>::action(value_set&&) {
  auto msg_size = sizeof(T) * this->n + sizeof(this->n);
  auto msg = message::make_message(msg_size, {});
  *((decltype(this->n)*)msg->payload) = this->n;
  auto arr = (T*)(msg->payload + sizeof(this->n));

  for (auto i = 0; i < this->n; i += 1) {
    arr[i] = (T)(i % this->n + this->selfIdx + 1);
  }

  auto copy0 = utilities::copy_message(msg);
  auto copy1 = utilities::copy_message(msg);
  return {std::make_pair(0, msg2value(msg)),
          std::make_pair(1, msg2value(copy0)),
          std::make_pair(2, msg2value(copy1))};
}

template <typename T>
typename add_values<T>::value_set add_values<T>::action(value_set&& accepted) {
  auto& lhsMsg = accepted[0];
  auto& rhsMsg = accepted[1];

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

  return {std::make_pair(0, lhsMsg)};
}

template <typename T>
typename print_values<T>::value_set print_values<T>::action(
    value_set&& accepted) {
  const auto& msg = accepted[0];

  std::size_t* n;
  T* arr;
  unpack_array(msg, &n, &arr);

  std::stringstream ss;
  ss << "com" << this->id << "@vil" << selfIdx << "> ";
  ss << "[ ";
  for (auto i = 0; i < *n; i += 1) {
    ss << arr[i] << " ";
  }
  ss << "]";

  ckout << ss.str().c_str() << endl;

  return {};
}

#define CK_TEMPLATES_ONLY
#include "tester.def.h"
#undef CK_TEMPLATES_ONLY

#include "tester.def.h"
