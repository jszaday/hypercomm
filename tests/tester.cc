#include <charm++.h>

#include "tester.hh"
#include "tester.decl.h"

void enroll_polymorphs(void) {
  hypercomm::init_polymorph_registry();

  if (CkMyRank() == 0) {
    hypercomm::enroll<reduction_port<int>>();
  }
}

struct say_hello : public hypercomm::component {
  say_hello(const id_t& _1) : component(_1) {}

  virtual value_t action(void) override {
    CkPrintf("hello from your friendly neighborhood component!\n");

    return {};
  }
};

struct main : public CBase_main {
  main(CkArgMsg* msg) {
    auto n = CkNumPes();
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

struct locality : public CBase_locality, public locality_base<int> {
  entry_port_ptr ours;
  int n;

  locality(int _1)
      : n(_1), ours(std::make_shared<reduction_port<int>>(0x69, 0x69)) {
    auto comp = this->emplace_component<say_hello>();
    auto theirs = comp->open_in_port();
    this->open(ours, std::make_pair(comp->id, theirs));
    this->activate_component(comp);

    if (this->__index__() == 0) {
      thisProxy.consensus(this->ckGetArrayIndex());
    }
  }

  void consensus(const CkArrayIndex& idx) {
    std::vector<int> indices(n);
    std::iota(std::begin(indices), std::end(indices), 0);
    auto sect = std::make_shared<generic_section<int>>(std::move(indices));

    auto fn = std::make_shared<nil_combiner>();
    auto cb =
        std::make_shared<forwarding_callback>(make_proxy(thisProxy[idx]), ours);

    CkPrintf("%d> contributing a value\n", this->__index__());
    auto msg = hypercomm_msg::make_message(0x0, {});
    this->local_contribution(sect, hypercomm::utilities::wrap_message(msg), fn,
                             cb);
  }

  virtual void demux(hypercomm_msg* msg) override {
    CkPrintf("%d> received a value for %s\n", this->__index__(),
             port2str(msg->dst).c_str());

    this->receive_value(msg->dst, hypercomm::utilities::wrap_message(msg));
  }
};

#define CK_TEMPLATES_ONLY
#include "tester.def.h"
#undef CK_TEMPLATES_ONLY

#include "tester.def.h"
