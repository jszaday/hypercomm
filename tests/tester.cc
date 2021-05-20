#include <charm++.h>

#include "tester.hh"

#ifndef DECOMP_FACTOR
#define DECOMP_FACTOR 4
#endif

constexpr auto kDecompFactor = DECOMP_FACTOR;
constexpr auto kVerbose = false;

struct persistent_port : public virtual entry_port {
  components::port_id_t id = 0;

  persistent_port(PUP::reconstruct) {}
  persistent_port(const decltype(id)& _1): id(_1) {}

  virtual bool keep_alive(void) const override { return true; }

  virtual bool equals(const std::shared_ptr<comparable>& other) const override {
    auto theirs = std::dynamic_pointer_cast<persistent_port>(other);
    return this->id == theirs->id;
  }

  virtual hash_code hash(void) const override  {
    return hash_code(id);
  }

  virtual void __pup__(serdes& s) override  {
    s | id;
  }
};

void enroll_polymorphs(void) {
  hypercomm::init_polymorph_registry();

  if (CkMyRank() == 0) {
    hypercomm::enroll<persistent_port>();
    hypercomm::enroll<reduction_port<int>>();
    hypercomm::enroll<broadcaster<int>>();
    hypercomm::enroll<generic_section<int>>();
  }
}

static constexpr auto kMaxReps = 129;

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

std::string port2str(const entry_port_ptr& port) {
  if (!port) {
    return "nullptr";
  }
  auto redn_port = std::dynamic_pointer_cast<reduction_port<int>>(port);
  if (redn_port) {
    std::stringstream ss;
    ss << "reducer_port(id=" << redn_port->id << ",idx=" << redn_port->index << ")";
    return ss.str();
  }
  auto persist_port = std::dynamic_pointer_cast<persistent_port>(port);
  if (persist_port) {
    std::stringstream ss;
    ss << "persistent_port(id=" << persist_port->id << ")";
    return ss.str();
  }
  return "unknown";
}

void forwarding_callback::send(callback::value_type&& value) {
  auto index = this->proxy->index();
#if CMK_DEBUG
  CkPrintf("info> message being forwarded to port %s at %d\n",
           port2str(this->port).c_str(), reinterpret_index<int>(index));
#endif
  auto msg = hypercomm_msg::make_message(0x0, this->port);
  CProxyElement_locality_base_ base(this->proxy->id(), index);
  base.demux(msg);
}

struct locality_base_ : public CBase_locality_base_,
                        public virtual common_functions_ {
  using this_ptr = locality_base_*;

  locality_base_(void) {}

  virtual void demux(hypercomm_msg* msg) {
    throw std::runtime_error("you should never see this!");
  }

  virtual void execute(CkMessage* msg) {
    throw std::runtime_error("you should never see this!");
  }

  virtual const CkArrayIndex& __index_max__(void) const override {
    return this->thisIndexMax;
  }

  virtual std::shared_ptr<hypercomm::proxy> __proxy__(void) const override {
    return hypercomm::make_proxy(const_cast<this_ptr>(this)->thisProxy);
  }
};

struct nil_combiner : public combiner {
  virtual combiner::return_type send(combiner::argument_type&& args) override {
    return args.empty() ? combiner::return_type{} : args[0];
  }

  virtual void __pup__(hypercomm::serdes&) override {}
};

struct entry_method_like : public hypercomm::component {
  entry_method_like(const id_t& _1) : component(_1) {}

  virtual bool keep_alive(void) const override { return true; }

  virtual int num_expected(void) const override { return 1; }
};

struct say_hello : virtual public entry_method_like {
  say_hello(const id_t& _1) : entry_method_like(_1) {}

  virtual value_t action(void) override {
    if (kVerbose) {
      CkPrintf("com%lu> hi, hi!\n", id);
    }

    return {};
  }
};

struct my_redn_com : virtual public entry_method_like {
  locality* self;

  my_redn_com(const id_t& _1, locality* _2) : entry_method_like(_1), self(_2) {}

  virtual value_t action(void) override;
};

struct locality : public CBase_locality, public locality_base<int> {
  entry_port_ptr bcast_port, redn_port;
  int n;
  section_ptr<int> section;

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
    this->section = std::make_shared<generic_section<int>>(std::move(indices));

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
#if CMK_DEBUG
    CkPrintf("%d> received a value for %s\n", this->__index__(),
             port2str(msg->dst).c_str());
#endif

    this->receive_value(msg->dst, hypercomm::utilities::wrap_message(msg));
  }
};

typename my_redn_com::value_t my_redn_com::action(void) {
  auto& head = this->accepted[0];

  std::tuple<array_proxy::index_type, int> tmp;
  auto& idx = std::get<0>(tmp);
  auto& numIters = std::get<1>(tmp);
  auto pkr = serdes::make_unpacker(head, utilities::get_message_buffer(head));
  hypercomm::pup(pkr, tmp);

  auto fn = std::make_shared<nil_combiner>();
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
