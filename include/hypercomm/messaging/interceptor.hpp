#ifndef __HYPERCOMM_MESSAGING_INTERCEPTOR_MSG_HPP__
#define __HYPERCOMM_MESSAGING_INTERCEPTOR_MSG_HPP__

#include <hypercomm/messaging/messaging.decl.h>

namespace hypercomm {

extern CProxy_interceptor interceptor_;

class interceptor : public CBase_interceptor {
  using queue_type =
      std::unordered_map<CkArrayIndex, std::vector<CkMessage*>, IndexHasher>;
  using req_map_type =
      std::unordered_map<CkArrayIndex, CkArrayIndex, IndexHasher>;

  std::unordered_map<CkArrayID, req_map_type, ArrayIDHasher> fwdReqs_;
  std::unordered_map<CkArrayID, queue_type, ArrayIDHasher> queued_;

 public:
  interceptor(void) = default;

  // try to send any messages buffered for a given idx
  void resync_queue(const CkArrayID& aid, const CkArrayIndex& idx) {
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

  // look up the home PE and last known PE of the given index
  inline std::pair<int, int> lastKnown(const CkArrayID& aid,
                                       const CkArrayIndex& idx) {
    auto locMgr = CProxy_ArrayBase(aid).ckLocMgr();
    return std::make_pair(locMgr->homePe(idx), locMgr->whichPe(idx));
  }

  // recursively unravel forwarding requests until the destination is found
  const CkArrayIndex& dealias(const CkArrayID& aid,
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

 public:
  // create a forwarding record for "from" to "to" at the home pe of "from"
  void forward(const CkArrayID& aid, const CkArrayIndex& from,
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
  void stop_forwarding(const CkArrayID& aid, const CkArrayIndex& idx) {
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

  inline void deliver(const CkArrayID& aid, const CkArrayIndex& raw,
                      CkMarshalledMessage&& msg) {
    this->deliver(aid, raw, msg.getMessage());
  }

  void deliver(const CkArrayID& aid, const CkArrayIndex& raw, CkMessage* msg) {
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

  // asynchronously send a message to the specified element
  inline static void send_async(const CProxyElement_ArrayElement& proxy,
                                CkMessage* msg) {
    interceptor::send_async(proxy.ckGetArrayID(), proxy.ckGetIndex(), msg);
  }

  // asynchronously send a message to the specified index of aid
  inline static void send_async(const CkArrayID& aid, const CkArrayIndex& idx,
                                CkMessage* msg) {
    if (((CkGroupID)interceptor_).isZero()) {
#if CMK_VERBOSE
      CkError("warning> unable to deliver through interceptor.\n");
#endif
      // TODO ( is there a better function for this? )
      CProxyElement_ArrayBase::ckSendWrapper(aid, idx, msg,
                                             UsrToEnv(msg)->getEpIdx(), 0);
    } else {
      interceptor_[CkMyPe()].deliver(aid, idx, CkMarshalledMessage(msg));
    }
  }

  // get the local branch of interceptor_
  inline static interceptor* local_branch() {
    if (((CkGroupID)interceptor_).isZero()) {
      return nullptr;
    } else {
      return interceptor_.ckLocalBranch();
    }
  }
};

// a mainchare that initializes the singleton instance of interceptor
class interceptor_initializer_ : public CBase_interceptor_initializer_ {
 public:
  interceptor_initializer_(CkArgMsg* m) {
    interceptor_ = CProxy_interceptor::ckNew();
    delete this;
  }

  interceptor_initializer_(CkMigrateMessage* m)
      : CBase_interceptor_initializer_(m) {
    delete m;
  }
};
}  // namespace hypercomm

#endif
