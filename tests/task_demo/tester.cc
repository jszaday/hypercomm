#include <charm++.h>

#include "tester.hh"

#include <ctime>
#include <cstdlib>

using value_type = std::vector<float>;

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
        CkPrintf("main> nodegroup-like, numNodes=%d, numPes=%d\n", CkNumNodes(),
                 CkNumPes());
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
    com0->template output_to<0, 0>(*com1);
    // gen_values.(1) => add values
    com0->template output_to<1, 0>(*com2);
    // gen_values.(2) => recv_value@neighbor
    auto neighborIdx = conv2idx<CkArrayIndex>((selfIdx + 1) % n);
    auto fwd = forward_to(thisProxy[neighborIdx], recv_value);
    com0->template output_to<2>(std::move(fwd));
    // recv_value@here => add values
    this->connect(recv_value, com2, 1);
    // add_values => print values 2
    com2->template output_to<0, 0>(*com3);

    this->activate_component(com3);
    this->activate_component(com2);
    this->activate_component(com1);
    this->activate_component(com0);

    // kickoff the sequence
    deliverable dev(make_unit_value());
    CkEnforce(this->passthru(std::make_pair(com0->id, 0), dev));
  }
};

template <typename T>
typename gen_values<T>::out_set gen_values<T>::action(in_set&) {
  value_type arr(n);

  for (auto i = 0; i < this->n; i += 1) {
    arr[i] = (typename T::value_type)(i % this->n + this->selfIdx + 1);
  }

  return std::make_tuple(
    make_typed_value<value_type>(arr),
    make_typed_value<value_type>(arr),
    make_typed_value<value_type>(arr)
  );
}

template <typename T>
typename add_values<T>::out_set add_values<T>::action(in_set& set) {
  auto& lhs = std::get<0>(set)->value();
  auto& rhs = std::get<1>(set)->value();

  auto m = lhs.size();
  auto n = rhs.size();

  if (m != n) {
    CkAbort("add_values> array size mismatch");
  }

  for (auto i = 0; i < n; i += 1) {
    lhs[i] += rhs[i];
  }

  return std::move(std::get<0>(set));
}

template <typename T>
typename std::tuple<> print_values<T>::action(std::tuple<typed_value_ptr<T>>& set) {
  auto& val = std::get<0>(set)->value();
  std::size_t n = val.size();

  std::stringstream ss;
  ss << "com" << this->id << "@vil" << selfIdx << "> ";
  ss << "[ ";
  for (auto i = 0; i < n; i += 1) {
    ss << val[i] << " ";
  }
  ss << "]";

  ckout << ss.str().c_str() << endl;

  return {};
}

#define CK_TEMPLATES_ONLY
#include "tester.def.h"
#undef CK_TEMPLATES_ONLY

#include "tester.def.h"
