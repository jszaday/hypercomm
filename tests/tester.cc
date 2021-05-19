#include <charm++.h>

#include "tester.hh"

#ifndef DECOMP_FACTOR
#define DECOMP_FACTOR 4
#endif

constexpr auto kDecompFactor = DECOMP_FACTOR;

void enroll_polymorphs(void) {
  hypercomm::init_polymorph_registry();

  if (CkMyRank() == 0) {
    hypercomm::enroll<reduction_port<int>>();
    hypercomm::enroll<broadcaster<int>>();
    hypercomm::enroll<generic_section<int>>();
  }
}

struct say_hello : public hypercomm::component {
  say_hello(const id_t& _1) : component(_1) {}

  virtual value_t action(void) override {
    CkPrintf("com%lu> hi, hi!\n", id);

    return {};
  }
};

struct main : public CBase_main {
  main(CkArgMsg* msg) {
    auto n = kDecompFactor * CkNumPes();
    CProxy_locality::ckNew(n, n);
    CkExitAfterQuiescence();
  }
};

std::string port2str(const entry_port_ptr& port) {
  if (!port) {
    return "bogus";
  }
  auto port_typed = std::dynamic_pointer_cast<reduction_port<int>>(port);
  if (!port_typed) {
    return "unknown";
  }
  std::stringstream ss;
  ss << "reducer_port(id=" << port_typed->id << ",idx=" << port_typed->index
     << ")";
  return ss.str();
}

void forwarding_callback::send(callback::value_type&& value) {
  auto msg = hypercomm_msg::make_message(0x0, this->port);
  auto index = this->proxy->index();
  CProxyElement_locality_base_ base(this->proxy->id(), index);
  CkPrintf("info> message being forwarded to port %s at %d\n",
           port2str(this->port).c_str(), reinterpret_index<int>(index));
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

struct my_redn_com : public hypercomm::component {
  locality* self;

  my_redn_com(const id_t& _1, locality* _2) : component(_1), self(_2) {}

  virtual value_t action(void) override;
};

struct locality : public CBase_locality, public locality_base<int> {
  entry_port_ptr bcast_port, redn_port;
  int n;
  section_ptr<int> section;

  locality(int _1)
  : n(_1),
    bcast_port(std::make_shared<reduction_port<int>>(21, 21)),
    redn_port(std::make_shared<reduction_port<int>>(42, 42)) {
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

      this->start_broadcast();
    }
  }

  void start_broadcast(void) {
    auto idx = this->ckGetArrayIndex();
    auto sz  = size(idx);
    auto msg = hypercomm_msg::make_message(sz, bcast_port);
    auto pkr = serdes::make_packer(msg->payload);
    hypercomm::pup(pkr, idx);

    this->broadcast(section, msg);
  }

  virtual void execute(CkMessage* msg) override {
    action_type action{};
    auto unpack = serdes::make_unpacker(
        utilities::wrap_message(msg), utilities::get_message_buffer(msg));
    hypercomm::pup(unpack, action);
    this->receive_action(action);
  }

  virtual void demux(hypercomm_msg* msg) override {
    CkPrintf("%d> received a value for %s\n", this->__index__(),
             port2str(msg->dst).c_str());

    this->receive_value(msg->dst, hypercomm::utilities::wrap_message(msg));
  }
};

typename my_redn_com::value_t my_redn_com::action(void) {
  CkPrintf("com%lu@%d> contributing a value\n", this->id, self->__index__());

  auto& head = this->accepted[0]; 

  temporary<array_proxy::index_type> idx;
  auto pkr = serdes::make_unpacker(head, utilities::get_message_buffer(head));
  hypercomm::pup(pkr, idx);

  auto fn = std::make_shared<nil_combiner>();
  auto cb =
      std::make_shared<forwarding_callback>(make_proxy(self->thisProxy[idx.value()]), self->redn_port);
  auto msg = hypercomm_msg::make_message(0x0, {});
  self->local_contribution(self->section, hypercomm::utilities::wrap_message(msg), fn, cb);

  return {};
}

#define CK_TEMPLATES_ONLY
#include "tester.def.h"
#undef CK_TEMPLATES_ONLY

#include "tester.def.h"
