#include <hypercomm/messaging/interceptor.hpp>
#include <hypercomm/sections/imprintable.hpp>

namespace hypercomm {

CProxy_interceptor interceptor_;

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

void interceptor::deliver(const CkArrayID& aid, const CkArrayIndex& raw,
                          CkMessage* msg) {
  auto* env = UsrToEnv(msg);
  env->setMsgtype(ForArrayEltMsg);
  auto& numHops = env->getsetArrayHops();

  auto& idx = this->dealias(aid, raw);
  auto pair = this->lastKnown(aid, idx);
  auto& homePe = pair.first;
  auto& lastPe = (pair.second >= 0) ? pair.second : homePe;

  // if the last known pe was our pe
  if (lastPe == CkMyPe()) {
    // look up the element in our local branch
    auto* arr = CProxy_ArrayBase(aid).ckLocalBranch();
    auto* elt = arr->lookup(idx);
    // if it's present,
    if (elt != nullptr) {
      // invoke the entry method (per the msg's epIdx)
      elt->ckInvokeEntry(UsrToEnv(msg)->getEpIdx(), msg, true);
      return;
    } else if (homePe == CkMyPe()) {
      // otherwise, if we are its home PE, buffer the message
      this->queued_[aid][idx].push_back(msg);
      QdCreate(1);  // create (1) to delay QD

#if CMK_ERROR_CHECKING
      auto& reqMap = this->fwdReqs_[aid];
      if (reqMap.find(idx) != std::end(reqMap)) {
        CkAbort("%d> refusing to create (1) for %d.\n", CkMyPe(),
                *(idx.data()));
      }
#endif  // CMK_ERROR_CHECKING

      return;
    }
  }

  // otherwise, determine the next location to send the msg
  int destPe;
  if (numHops > 1 && (homePe != CkMyPe())) {
    numHops = 0;
    destPe = homePe;  // home PE if it's hopped, resetting hop count
  } else {
    numHops += 1;
    destPe = lastPe;  // otherwise, try last known PE
  }

  // then send it along
  thisProxy[destPe].deliver(aid, idx, CkMarshalledMessage(msg));
}

void interceptor::send_to_root(const CkArrayID& aid, const std::shared_ptr<imprintable_base_>& section, CkMessage* msg) {
  auto* root = section->pick_root(aid);
  if (root) {
    interceptor::send_async(aid, *root, msg);
  } else {
    NOT_IMPLEMENTED;
  }
}
}  // namespace hypercomm

#include <hypercomm/messaging/messaging.def.h>
