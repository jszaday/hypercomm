#include <hypercomm/core/generic_locality.hpp>
#include <hypercomm/messaging/interceptor.hpp>
#include <hypercomm/sections/imprintable.hpp>
#include <hypercomm/utilities/macros.hpp>

namespace hypercomm {

CkpvDeclare(CProxy_interceptor, interceptor_);

namespace messaging {
void initialize(void) {
  // register the messaging module (on rank 0)
  if (CkMyRank() == 0) {
    _registermessaging();
  }
  // register the handler for delivery
  delivery::handler();
  // zero the per-pe interceptor proxy
  CkpvInitialize(CProxy_interceptor, interceptor_);
  CkAssert(((CkGroupID)CkpvAccess(interceptor_)).isZero());
}
}  // namespace messaging

// registers delivery::handler_ as a converse handler
const int& delivery::handler(void) {
  return CmiAutoRegister(delivery::handler_);
}

// locally delivers the payload to the interceptor with immediacy
void delivery::handler_(delivery* msg) {
  auto* local = interceptor::local_branch();
  local->deliver(msg->aid, msg->idx, std::move(msg->payload), true);
  delete msg;
}

// try to send any messages buffered for a given idx
void interceptor::resync_queue(const CkArrayID& aid, const CkArrayIndex& idx) {
  auto& queue = this->queued_[aid];
  auto search = queue.find(idx);
  if (search != std::end(queue)) {
    // take the buffer from the map then erase it
    // (preventing multiple send attempts of the same msg)
    auto buffer = std::move(search->second);
    queue.erase(search);
    // then try to redeliver each buffered message
    for (auto& msg : buffer) {
      this->deliver(aid, idx, msg);
      QdProcess(1);  // processing (1) to offset QdCreate
    }
  }
}

// recursively unravel forwarding requests until the destination is found
const CkArrayIndex& interceptor::dealias(const CkArrayID& aid,
                                         const CkArrayIndex& idx) const {
  auto findAid = this->fwdReqs_.find(aid);
  if (findAid != std::end(this->fwdReqs_)) {
    auto& reqMap = findAid->second;
    auto findIdx = reqMap.find(idx);
    if (findIdx != std::end(reqMap)) {
      return this->dealias(aid, findIdx->second);
    }
  }
  return idx;
}

// create a forwarding record for "from" to "to" at the home pe of "from"
void interceptor::forward(const CkArrayID& aid, const CkArrayIndex& from,
                          const CkArrayIndex& to) {
  auto* locMgr = CProxy_ArrayBase(aid).ckLocMgr();
  auto homePe = locMgr->homePe(from);
  if (homePe == CkMyPe()) {
    auto& reqMap = this->fwdReqs_[aid];
    auto search = reqMap.find(from);
    if (search == std::end(reqMap)) {
#if CMK_VERBOSE
      CkPrintf("%d> forwarding from %d to %d.\n", CkMyPe(), *(from.data()),
               *(to.data()));
#endif  // CMK_VERBOSE
      reqMap.emplace(from, to);
      // resync the send queue in case it has messages to "from"
      this->resync_queue(aid, from);
    } else {
      CkAbort("duplicate forwarding request");
    }
  } else {
    thisProxy[homePe].forward(aid, from, to);
  }
}

// delete any forwarding records for the given idx, at its home pe
void interceptor::stop_forwarding(const CkArrayID& aid,
                                  const CkArrayIndex& idx) {
  auto* locMgr = CProxy_ArrayBase(aid).ckLocMgr();
  auto homePe = locMgr->homePe(idx);
  if (homePe == CkMyPe()) {
    auto& reqMap = this->fwdReqs_[aid];
    auto search = reqMap.find(idx);
    if (search != std::end(reqMap)) {
      reqMap.erase(search);
    }
    // TODO ( should this resync the send queue for idx? )
  } else {
    thisProxy[homePe].stop_forwarding(aid, idx);
  }
}

void interceptor::subscribe_to(CkArray* arr) {
  auto* locMgr = arr->getLocMgr();
  auto& gid = locMgr->ckGetGroupID();
  auto search = this->locMgrs_.find(gid);
  // if we have not already subscribed to updates
  if (search == std::end(this->locMgrs_)) {
    // register the locmgr and its array
    // (may fail with bound arrays?)
    this->locMgrs_[locMgr->ckGetGroupID()] = arr->CkGetGroupID();
    // and add us as a listener to location updates
    using namespace std::placeholders;
    locMgr->addListener(
        std::bind(&interceptor::on_location_update, this, _1, _2, _3));
  }
}

// resync our queue when the loc mgr undergoes an update
void interceptor::on_location_update(const CkGroupID& gid, const CmiUInt8& id,
                                     const int& pe) {
  auto& aid = this->locMgrs_[gid];
  auto* loc = CProxy_CkLocMgr(gid).ckLocalBranch();
  this->resync_queue(aid, loc->lookupIdx(id));
}

void interceptor::deliver(const CkArrayID& aid, const CkArrayIndex& pre,
                          detail::payload_ptr&& payload,
                          const bool& immediate) {
  auto* arr = CProxy_ArrayBase(aid).ckLocalBranch();
  auto& post = this->dealias(aid, pre);
  auto* elt = arr->lookup(post);
  // if the elt is locally available
  if (elt != nullptr) {
    // process it with appropriate immediacy
    detail::payload::process(elt, std::move(payload), immediate);
  } else {
    auto msg = payload->release();
    auto homePe = arr->homePe(post);
    auto lastPe = arr->lastKnown(post);
    auto ourElt = homePe == lastPe;
    // if we are the elt's home (and its last known loc)
    if (ourElt && (homePe == CkMyPe())) {
      // subscribe to location updates in case it (re)manifests
      this->subscribe_to(arr);
      // then buffer the msg and create (1) to delay QD
      this->queued_[aid][post].push_back(msg);
      QdCreate(1);
    } else {
      // if we lost an elt, the home pe will know its location
      // (or at least buffer it)
      auto destPe = ourElt ? homePe : lastPe;
      thisProxy[destPe].deliver(aid, post, CkMarshalledMessage(msg));
    }
  }
}

namespace detail {
void payload::process(ArrayElement* elt, payload_ptr&& payload,
                      const bool& immediate) {
  CkAssert(payload->valid());

  auto* cast = dynamic_cast<generic_locality_*>(elt);
  auto& opts = payload->options_;

  if (immediate) {
    cast->update_context();

    if (payload->type_ == kMessage) {
      auto& msg = opts.msg_;
#if CMK_VERBOSE
      CkPrintf("pe%d> delivering msg %p to idx %s!\n", CkMyPe(), msg,
               utilities::idx2str(elt->ckGetArrayIndex()).c_str());
#endif
      cast->receive_message(msg);
      msg = nullptr;
    } else {
      auto& port = opts.value_.port_;
#if CMK_VERBOSE
      CkPrintf("pe%d> delivering a value to port %s of idx %s.\n", CkMyPe(),
               (port->to_string()).c_str(),
               utilities::idx2str(elt->ckGetArrayIndex()).c_str());
#endif
      // dump both the port and value since we don't need them after this
      cast->receive_value(std::move(port), std::move(opts.value_.value_));
    }
  } else {
    auto& aid = elt->ckGetArrayID();
    auto& idx = elt->ckGetArrayIndex();
#if CMK_VERBOSE
    CkPrintf("pe%d> pushing a message/value onto the queue for %s.\n", CkMyPe(),
             utilities::idx2str(elt->ckGetArrayIndex()).c_str());
#endif
    CmiPushPE(CkMyRank(), new delivery(aid, idx, std::move(payload)));
  }
}
}  // namespace detail

void interceptor::send_to_root(
    const CkArrayID& aid, const std::shared_ptr<imprintable_base_>& section,
    CkMessage* msg) {
  auto* root = section->pick_root(aid);
  if (root) {
    interceptor::send_async(aid, *root, msg);
  } else {
    NOT_IMPLEMENTED;
  }
}
}  // namespace hypercomm

#include <hypercomm/messaging/messaging.def.h>
