#ifndef __HYPERCOMM_CORE_LOCALITY_HPP__
#define __HYPERCOMM_CORE_LOCALITY_HPP__

#include "broadcaster.hpp"
#include "port_opener.hpp"

#include <hypercomm/messaging/packing.hpp>

#include "locality.decl.h"

struct locality_base_ : public CBase_locality_base_,
                        public virtual hypercomm::common_functions_<CkArrayIndex> {
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

  virtual std::shared_ptr<hypercomm::collective_proxy<CkArrayIndex>> __proxy__(void) const override {
    return hypercomm::make_proxy(const_cast<this_ptr>(this)->thisProxy);
  }

  virtual std::shared_ptr<hypercomm::element_proxy<CkArrayIndex>> __element__(void) const override {
    return hypercomm::make_proxy((const_cast<this_ptr>(this)->thisProxy)[this->ckGetArrayIndex()]);
  }
};

namespace hypercomm {
template<typename Index>
/* static */ void locality_base<Index>::send_action(const collective_ptr<CkArrayIndex>& p, const CkArrayIndex& i, const action_type& a) {
  const auto& thisCollection = static_cast<const CProxy_locality_base_&>(p->c_proxy());
  auto msg = hypercomm::pack(a);
  (const_cast<CProxy_locality_base_&>(thisCollection))[i].execute(msg);
}

// TODO make this more generic
template<typename Index>
void send2port(const element_ptr<Index>& proxy, const entry_port_ptr& port, std::shared_ptr<CkMessage>&& value) {
  auto msg = static_cast<message*>(utilities::unwrap_message(std::move(value)));
  auto env = UsrToEnv(msg);
  auto msgIdx = env->getMsgIdx();
  if (msgIdx == message::__idx) {
    const auto& base =
        static_cast<const CProxyElement_locality_base_&>(proxy->c_proxy());
    // TODO should this be a move?
    msg->dst = port;
    const_cast<CProxyElement_locality_base_&>(base).demux(msg);
  } else {
    // TODO repack to hypercomm in this case (when HYPERCOMM_NO_COPYING is undefined)
    CkAbort("expected a hypercomm msg, but got %s instead\n", _msgTable[msgIdx]->name);
  }
}

template<typename Index>
/* static */ void locality_base<Index>::send_future(const future& f, std::shared_ptr<CkMessage>&& value) {
  auto proxy =
      std::dynamic_pointer_cast<element_proxy<CkArrayIndex>>(f.source);
  auto port = std::make_shared<future_port>(f);
  send2port(proxy, port, std::move(value));
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
    send_action(collective, conv2idx<CkArrayIndex>(root), action);
  }
}

template<typename Index>
void locality_base<Index>::request_future(const future& f, const callback_ptr& cb) {
  auto ourPort = std::make_shared<future_port>(f);
  auto ourElement = this->__element__();

  this->open(ourPort, cb);

  auto& source = *f.source;
  if (!ourElement->equals(source)) {
    auto theirElement =
        dynamic_cast<element_proxy<CkArrayIndex>*>(&source);
    // CkPrintf("%s> remotely requesting %s\n", ourElement->to_string().c_str(), f.to_string().c_str());
    auto fwd = std::make_shared<forwarding_callback>(ourElement, ourPort);
    auto opener = std::make_shared<port_opener<Index>>(ourPort, fwd);
    send_action(theirElement->collection(), theirElement->index(), opener);
  }
}

void forwarding_callback::send(callback::value_type&& value) {
  send2port(std::dynamic_pointer_cast<element_proxy<CkArrayIndex>>(this->proxy),
            this->port, std::move(value));
}
}

#include "locality.def.h"

#endif
