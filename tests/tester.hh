#ifndef __TESTER_HH__
#define __TESTER_HH__

#include <hypercomm/core/broadcaster.hpp>
#include "tester.decl.h"

using namespace hypercomm;

template<typename Index>
void locality_base<Index>::send_action(const array_proxy_ptr& p, const Index& i, action_type&& a) {
  auto size = hypercomm::size(a);
  auto msg = CkAllocateMarshallMsg(size);
  auto packer = serdes::make_packer(msg->msgBuf);
  pup(packer, a);
  CProxyElement_locality_base_ peer(p->id(), conv2idx<impl_index_type>(i));
  peer.execute(msg);
}

template<typename Index>
void locality_base<Index>::broadcast(const section_ptr& section, hypercomm_msg* _msg) {
  auto identity = this->identity_for(section);
  auto root = section->index_at(0);
  auto msg = std::static_pointer_cast<hypercomm_msg>(utilities::wrap_message(_msg));
  auto action = std::make_shared<broadcaster<Index>>(section, std::move(msg));

  using index_type = array_proxy::index_type;
  if (root == this->__index__()) {
    this->receive_action(action);
  } else {
    auto collective =
        std::dynamic_pointer_cast<array_proxy>(this->__proxy__());
    send_action(collective, root, action);
  }
}

void forwarding_callback::send(callback::value_type&& value) {
  auto index = this->proxy->index();
  auto msg = hypercomm_msg::make_message(0x0, this->port);
  CProxyElement_locality_base_ base(this->proxy->id(), index);
  base.demux(msg);
}

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

struct entry_method_like : public hypercomm::component {
  entry_method_like(const id_t& _1) : component(_1) {}

  virtual bool keep_alive(void) const override { return true; }

  virtual int num_expected(void) const override { return 1; }
};

struct say_hello : virtual public entry_method_like {
  say_hello(const id_t& _1) : entry_method_like(_1) {}
  virtual value_t action(void) override;
};

struct my_redn_com : virtual public entry_method_like {
  locality* self;
  my_redn_com(const id_t& _1, locality* _2) : entry_method_like(_1), self(_2) {}
  virtual value_t action(void) override;
};

struct nop_combiner : public combiner {
  virtual combiner::return_type send(combiner::argument_type&& args) override {
    return args.empty() ? combiner::return_type{} : args[0];
  }

  virtual void __pup__(hypercomm::serdes&) override {}
};

#endif
