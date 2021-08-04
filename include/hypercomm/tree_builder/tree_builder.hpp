#ifndef __HYPERCOMM_TREE_BUILDER_TREE_BUILDER_HPP__
#define __HYPERCOMM_TREE_BUILDER_TREE_BUILDER_HPP__

#include <completion.h>

#include "manageable.hpp"

#include "../core/math.hpp"
#include "../messaging/interceptor.hpp"

#include <hypercomm/tree_builder/tree_builder.decl.h>

namespace hypercomm {

class tree_builder : public CBase_tree_builder, public array_listener {
 public:
  using index_type = CkArrayIndex;
  using element_type = manageable_base_ *;
  using detector_type = CProxy_CompletionDetector;

  bool set_endpoint_ = false;

 protected:
  CmiNodeLock lock_;

  using record_type = std::pair<element_type, int>;
  using elements_type =
      std::unordered_map<index_type, record_type, array_index_hasher>;

  std::unordered_map<CkArrayID, detector_type, array_id_hasher> arrays_;
  std::unordered_map<CkArrayID, elements_type, array_id_hasher> elements_;
  std::unordered_map<CkArrayID, bool, array_id_hasher> insertion_statuses_;

  inline CompletionDetector *detector_for(const CkArrayID &aid) const {
#if CMK_ERROR_CHECKING
    CkAssertMsg(this->is_inserting(aid), "missing call to begin_inserting");
    auto search = this->arrays_.find(aid);
    CkAssertMsg(search != std::end(this->arrays_),
                "missing completion detector");
    const auto &proxy = search->second;
#else
    const auto &proxy = this->arrays_[aid];
#endif
    CompletionDetector *inst = nullptr;
    while (nullptr == (inst = proxy.ckLocalBranch())) {
      CsdScheduler(0);
    }
    return inst;
  }

  CkArray *spin_to_win(const CkArrayID &aid, const int &rank) {
    if (rank == CkMyRank()) {
      return aid.ckLocalBranch();
    } else {
      CkArray *inst = nullptr;
      while (nullptr == (inst = aid.ckLocalBranchOther(rank))) {
        CsdScheduler(0);
      }
      return inst;
    }
  }

  struct endpoint_ {
    int node_;
    element_type elt_;
    endpoint_(const int &node) : elt_(nullptr), node_(node) {}
    endpoint_(const element_type &elt) : elt_(elt), node_(-1) {}
  };

  struct contribute_helper_ {
    CProxy_tree_builder manager_;
    CkCallback cb_;
    CkArrayID aid_;

    static void start_action_(contribute_helper_ *self, void *msg) {
      self->manager_.ckLocalBranch()->contribute(self->cb_);
      delete self;
      CkFreeMsg(msg);
    }

    static void finish_action_(contribute_helper_ *self, void *msg) {
      self->manager_.insertion_complete_(self->aid_, self->cb_);
      delete self;
      CkFreeMsg(msg);
    }

    static CkCallback start_action(tree_builder *manager, const CkArrayID &aid,
                                   const CkCallback &cb) {
      auto instance = new contribute_helper_{
          .manager_ = manager->thisProxy, .cb_ = cb, .aid_ = aid};
      return CkCallback((CkCallbackFn)&contribute_helper_::start_action_,
                        instance);
    }

    static CkCallback finish_action(tree_builder *manager, const CkArrayID &aid,
                                    const CkCallback &cb) {
      auto instance = new contribute_helper_{
          .manager_ = manager->thisProxy, .cb_ = cb, .aid_ = aid};
      return CkCallback((CkCallbackFn)&contribute_helper_::finish_action_,
                        instance);
    }
  };

 public:
  tree_builder(void) : CkArrayListener(0), lock_(CmiCreateLock()) {}

  virtual const PUP::able::PUP_ID &get_PUP_ID(void) const { NOT_IMPLEMENTED; }

  void unreg_array(const CkArrayID &, const CkCallback &) { NOT_IMPLEMENTED; }

  void reg_array(const detector_type &detector, const CkArrayID &aid,
                 const CkCallback &cb) {
    CmiLock(lock_);
    auto search = this->arrays_.find(aid);
    if (search == std::end(this->arrays_)) {
      this->arrays_[aid] = detector;
    } else {
      CkAbort("array registered twice!");
    }
    CmiUnlock(lock_);

    // TODO this won't work if there's any simultaneous activity
    for (auto i = 0; i < CkMyNodeSize(); i += 1) {
      // TODO copy existing members into our table!
      spin_to_win(aid, i)->addListener(this);
    }

    this->contribute(cb);
  }

  ArrayElement *lookup(const CkArrayID &aid, const index_type &idx) {
    return dynamic_cast<ArrayElement *>(this->lookup(aid, idx, true));
  }

  using stamp_type = manageable_base_::stamp_type;

  std::pair<association_ptr_, stamp_type> create_child(
      const element_type &elt, const index_type &child) {
    return this->create_child(elt->ckGetArrayID(), elt->ckGetArrayIndex(),
                              child);
  }

  std::pair<association_ptr_, stamp_type> create_child(
      const CkArrayID &aid, const index_type &parent, const index_type &child) {
    CmiLock(this->lock_);
    auto &rec = this->record_for(aid, parent, false);
    auto last = rec.first->__stamp__();
    rec.first->put_downstream_(child);
    // this avoids un/locking the individual record
    CkAssertMsg(rec.second == CkMyRank(),
                "a child must be created on the same pe as its parent");
    CmiUnlock(this->lock_);
    association_ptr_ a(new hypercomm::association_);
    a->valid_upstream_ = true;
    a->upstream_.emplace_back(parent);
    return std::make_pair(std::move(a), last);
  }

  void begin_inserting(const CkArrayID &aid, const CkCallback &start,
                       const CkCallback &finish) {
    this->insertion_statuses_[aid] = true;
    if (thisIndex == 0) {
      auto begin = contribute_helper_::start_action(this, aid, start);
      auto end = contribute_helper_::finish_action(this, aid, finish);
      this->arrays_[aid].start_detection(CkNumNodes(), begin, {}, end, 0);
    } else {
      this->contribute(start);
    }
  }

  void done_inserting(const CkArrayID &aid) { this->detector_for(aid)->done(); }

  void insertion_complete_(const CkArrayID &aid, const CkCallback &cb) {
    this->insertion_statuses_[aid] = false;
    this->contribute(cb);
  }

  void make_endpoint(const CkArrayID &aid, const CkArrayIndex &idx) {
    CmiLock(lock_);
    auto *elt = this->lookup(aid, idx, false);
    if (elt) {
      this->make_endpoint(elt);
      this->detector_for(aid)->consume();
    } else {
      // element has migrated
      auto pe = aid.ckLocalBranch()->lastKnown(idx);
      this->thisProxy[CkNodeOf(pe)].make_endpoint(aid, idx);
    }
    CmiUnlock(lock_);
  }

  void receive_upstream(const int &node, const CkArrayID &aid,
                        const index_type &idx) {
#if CMK_VERBOSE
    CkPrintf("nd%d> received upstream from %d, idx=%s.\n", CkMyNode(), node,
             utilities::idx2str(idx).c_str());
#endif
    CmiLock(lock_);
    this->reg_upstream(endpoint_(node), aid, idx);
    this->detector_for(aid)->consume();
    CmiUnlock(lock_);
  }

  void receive_downstream(const int &node, const CkArrayID &aid,
                          const index_type &idx) {
#if CMK_VERBOSE
    CkPrintf("nd%d> received downstream from %d, idx=%s.\n", CkMyNode(), node,
             utilities::idx2str(idx).c_str());
#endif
    CmiLock(lock_);
    auto target = this->reg_downstream(endpoint_(node), aid, idx);
    if (target != nullptr) {
      // we implicitly produce (1) at this point
      thisProxy[node].receive_upstream(CkMyNode(), aid,
                                       target->ckGetArrayIndex());
    } else {
      // so we only consume (1) in the else branch
      this->detector_for(aid)->consume();
    }
    CmiUnlock(lock_);
  }

  inline bool is_inserting(const CkArrayID &aid) const {
    auto search = this->insertion_statuses_.find(aid);
    return (search == std::end(this->insertion_statuses_)) ? false
                                                           : search->second;
  }

 protected:
  record_type &record_for(const CkArrayID &aid, const index_type &idx,
                          const bool &lock) {
    if (lock) CmiLock(lock_);
    auto &rec = this->elements_[aid].find(idx)->second;
    if (lock) CmiUnlock(lock_);
    return rec;
  }

  element_type lookup(const CkArrayID &aid, const index_type &idx,
                      const bool &lock) {
    if (lock) CmiLock(lock_);
    auto &elements = this->elements_[aid];
    auto search = elements.find(idx);
    auto *elt = (search == std::end(elements)) ? nullptr : search->second.first;
    if (lock) CmiUnlock(lock_);
    return elt;
  }

  void make_endpoint(const element_type &elt) {
    CkAssertMsg(!this->set_endpoint_, "cannot register an endpoint twice");
    this->set_endpoint_ = true;
    elt->set_endpoint_();
  }

  void send_upstream(const endpoint_ &ep, const CkArrayID &aid,
                     const index_type &idx) {
    auto mine = CkMyNode();
    auto parent = binary_tree::parent(mine);

    if (parent >= 0) {
      auto src = ep.elt_ != nullptr ? mine : ep.node_;
      thisProxy[parent].receive_downstream(src, aid, idx);
      this->detector_for(aid)->produce();
    } else if (ep.elt_ != nullptr) {
      this->make_endpoint(ep.elt_);
    } else {
      thisProxy[ep.node_].make_endpoint(aid, idx);
      this->set_endpoint_ = true;
      this->detector_for(aid)->produce();
    }
  }

  void send_downstream(const endpoint_ &ep, const CkArrayID &aid,
                       const index_type &idx) {
    // unclear when this would occur, initial sync'ing should have destinations
    NOT_IMPLEMENTED;
  }

  using iter_type = typename elements_type::iterator;

  inline iter_type find_target(const CkArrayID &aid, const bool &up) {
    auto &elements = this->elements_[aid];
    using value_type = typename elements_type::value_type;
    if (up) {
      return std::find_if(
          std::begin(elements), std::end(elements),
          [](const value_type &val) -> bool {
            auto &val_ = val.second.first;
            return !(val_->association_ && val_->association_->valid_upstream_);
          });
    } else {
      // TODO ( ensure this cannot form cycles? )
      return std::min_element(
          std::begin(elements), std::end(elements),
          [](const value_type &lhs, const value_type &rhs) -> bool {
            auto &lhs_ = lhs.second.first;
            auto &rhs_ = rhs.second.first;
            return (lhs_->num_downstream_() < rhs_->num_downstream_());
          });
    }
  }

  element_type reg_upstream(const endpoint_ &ep, const CkArrayID &aid,
                            const index_type &idx) {
    auto search = this->find_target(aid, true);
    if (search == std::end(this->elements_[aid])) {
      this->send_downstream(ep, aid, idx);
      return nullptr;
    } else {
      auto &target = search->second.first;
      target->put_upstream_(idx);
      return target;
    }
  }

  element_type reg_downstream(const endpoint_ &ep, const CkArrayID &aid,
                              const index_type &idx) {
    auto &elements = this->elements_[aid];
    if (elements.empty()) {
      this->send_upstream(ep, aid, idx);
      return nullptr;
    } else {
      auto search = this->find_target(aid, false);
      CkAssert(search != std::end(elements));
      auto &target = search->second.first;
      target->put_downstream_(idx);
      return target;
    }
  }

  void try_reassociate(const element_type &elt) {
    CkError("warning> try_reassociate not implemented\n");
  }

  void associate(const CkArrayID &aid, const element_type &elt) {
    auto &assoc = elt->association_;
    // if we are (statically) inserting an unassociated element
    if (this->is_inserting(aid) && !assoc) {
      // create a new association for the element
      assoc.reset(new hypercomm::association_);
      // and set its up/downstream
      auto target = this->reg_downstream(endpoint_(elt), elt->ckGetArrayID(),
                                         elt->ckGetArrayIndex());
      if (target != nullptr) {
        elt->put_upstream_(target->ckGetArrayIndex());
      }
    } else {
      CkAssertMsg(assoc, "dynamic insertions must be associated");
      this->try_reassociate(elt);
    }
  }

  void disassociate(const CkArrayID &aid, const element_type &elt) {
    if (this->is_inserting(aid) || !elt->association_->valid_upstream_) {
      NOT_IMPLEMENTED;
    }

    auto &curr = elt->ckGetArrayIndex();

    if (elt->is_endpoint_()) {
      // reset endpoint status
      NOT_IMPLEMENTED;
    } else {
      auto &parent = *(std::begin(elt->association_->upstream_));
      auto &children = elt->association_->downstream_;
      auto interceptor = interceptor_[CkMyNode()];

      auto *msg = hypercomm::pack(curr, children, elt->__stamp__());
      UsrToEnv(msg)->setEpIdx(
          CkIndex_locality_base_::idx_replace_downstream_CkMessage());
      interceptor.deliver(aid, parent, msg);

      if (!children.empty()) {
#if CMK_VERBOSE
        CkPrintf("%s> forwarding messages to %s.\n",
                 utilities::idx2str(curr).c_str(),
                 utilities::idx2str(parent).c_str());
#endif
        // redirect downstream messages upstream
        interceptor.forward(aid, curr, parent);
      }
    }
  }

  void reg_element(ArrayElement *elt, const bool &created) {
    auto cast = dynamic_cast<element_type>(elt);
    auto &aid = cast->ckGetArrayID();
    auto &idx = cast->ckGetArrayIndex();
    CmiLock(lock_);
    if (created) associate(aid, cast);
    this->elements_[aid][idx] = std::make_pair(cast, CkMyRank());
    CmiUnlock(lock_);
  }

  void unreg_element(ArrayElement *elt, const bool &died) {
    auto cast = dynamic_cast<element_type>(elt);
    auto &aid = cast->ckGetArrayID();
    auto &idx = cast->ckGetArrayIndex();
    CmiLock(lock_);
    auto &elements = this->elements_[aid];
    auto search = elements.find(idx);
    if (search != std::end(elements)) {
      elements.erase(search);
    }
    if (died) disassociate(aid, cast);
    CmiUnlock(lock_);
  }

 public:
  virtual void ckRegister(CkArray *, int) override {}

  virtual bool ckElementCreated(ArrayElement *elt) override {
    this->reg_element(elt, true);
    return array_listener::ckElementCreated(elt);
  }

  virtual bool ckElementArriving(ArrayElement *elt) override {
    this->reg_element(elt, false);
    return array_listener::ckElementArriving(elt);
  }

  virtual void ckElementDied(ArrayElement *elt) override {
    this->unreg_element(elt, true);
    array_listener::ckElementDied(elt);
  }

  virtual void ckElementLeaving(ArrayElement *elt) override {
    this->unreg_element(elt, false);
    array_listener::ckElementLeaving(elt);
  }

  virtual void pup(PUP::er &p) override { CBase_tree_builder::pup(p); }
};
}

// TODO ( move to library )
#include <hypercomm/tree_builder/tree_builder.def.h>

#endif
