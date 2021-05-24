#ifndef __HYPERCOMM_CORE_LOCALITY_HPP__
#define __HYPERCOMM_CORE_LOCALITY_HPP__

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
}

#include "locality.def.h"

#endif
