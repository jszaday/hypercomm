#ifndef __HYPERCOMM_CORE_LOCALITY_HPP__
#define __HYPERCOMM_CORE_LOCALITY_HPP__

#include <hypercomm/messaging/packing.hpp>
#include <hypercomm/core/broadcaster.hpp>

#include "locality.decl.h"

struct locality_base_ : public CBase_locality_base_,
                        public virtual hypercomm::common_functions_ {
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

namespace hypercomm {
template<typename Index>
void locality_base<Index>::send_action(const array_proxy_ptr& p, const Index& i, action_type&& a) {
  auto msg = hypercomm::pack(a);
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
  // creates a proxy to the locality
  auto dstIdx = this->proxy->index();
  CProxyElement_locality_base_ base(this->proxy->id(), dstIdx);
  auto env = UsrToEnv(value.get());
  auto msgIdx = env->getMsgIdx();
  if (msgIdx == message::__idx) {
    auto msg = static_cast<message*>(utilities::unwrap_message(std::move(value)));
    // TODO should this be a move?
    msg->dst = this->port;
    base.demux(msg);
  } else {
    // TODO repack to hypercomm in this case (when HYPERCOMM_NO_COPYING is undefined)
    CkAbort("expected a hypercomm msg, but got %s instead\n", _msgTable[msgIdx]->name);
  }
}
}

#include "locality.def.h"

#endif
