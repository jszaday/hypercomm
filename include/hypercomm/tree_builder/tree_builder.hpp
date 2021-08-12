#ifndef __HYPERCOMM_TREE_BUILDER_TREE_BUILDER_HPP__
#define __HYPERCOMM_TREE_BUILDER_TREE_BUILDER_HPP__

#if CMK_SMP
#include <atomic>
#endif

#include <hypercomm/tree_builder/tree_builder.decl.h>

#include "../core/math.hpp"
#include "../messaging/interceptor.hpp"
#include "../utilities/backstage_pass.hpp"
#include "back_inserter.hpp"
#include "manageable.hpp"

namespace hypercomm {

namespace detail {
std::vector<CkMigratable *> CkArray::*get(backstage_pass);

// Explicitly instantiating the class generates the fn declared above.
template class access_bypass<std::vector<CkMigratable *> CkArray::*,
                             &CkArray::localElemVec, backstage_pass>;
}  // namespace detail

class tree_builder : public CBase_tree_builder, public array_listener {
  tree_builder_SDAG_CODE;

 private:
  CksvStaticDeclare(CProxy_tree_builder, singleton_);

 public:
  using index_type = CkArrayIndex;
  using element_type = manageable_base_ *;

  friend class back_inserter;

  static void initialize(void) {
#if CMK_SMP
    back_inserter::handler();
#endif

    if (CkMyRank() == 0) {
      _registertree_builder();
    }
  }

 protected:
#if CMK_SMP
  CmiNodeLock lock_;
  std::vector<int> lockCounts_;
#endif

  using record_type = std::pair<element_type, int>;
  using elements_type =
      std::unordered_map<index_type, record_type, array_index_hasher>;

  template <typename Value>
  using array_id_map = std::unordered_map<CkArrayID, Value, array_id_hasher>;

  // tracks the quiscence status of each array (produce/consume counts)
  using qd_count_t = std::int64_t;
  array_id_map<qd_count_t> arrays_;
  // tracks the pointers to an array's elements
  array_id_map<elements_type> elements_;
  // tracks whether an array currently permits static insertions
  array_id_map<bool> insertion_statuses_;
  // tracks whether the root of an array's spanning tree has been set
  array_id_map<CkArrayIndex> endpoints_;

#if CMK_SMP
  struct counter_helper_ {
    std::atomic<int> count;
    CkCallback cb;

    counter_helper_(const CkCallback &_1) : count(CkMyNodeSize()), cb(_1) {}
  };

  // tracks the callback and number of remaining contributors during
  // registration
  array_id_map<counter_helper_> startup_;
#endif

  // indicates whether an element is local or remote
  struct endpoint_ {
    int node_;
    element_type elt_;
    endpoint_(const int &node) : elt_(nullptr), node_(node) {}
    endpoint_(const element_type &elt) : elt_(elt), node_(-1) {}
  };

  inline void lock(void) {
#if CMK_SMP
    auto &count = this->lockCounts_[CkMyRank()];
    if ((count++) == 0) {
      CmiLock(this->lock_);
    }
#endif
  }

  inline void unlock(void) {
#if CMK_SMP
    auto &count = this->lockCounts_[CkMyRank()];
    if ((--count) == 0) {
      CmiUnlock(this->lock_);
    }
#endif
  }

  /* register existing members of the array with this tree_builder
   * and subscribe it to array's updates. if smp, contribute to the
   * startup reduction.
   */
  void startup_process_(const CkArrayID &aid) {
    auto *local = aid.ckLocalBranch();
    auto &elems = local->*detail::get(detail::backstage_pass());
    for (auto &elem : elems) {
      this->ckElementCreated((ArrayElement *)elem);
    }
    local->addListener(this);
#if CMK_SMP
    auto &entry = this->startup_.at(aid);
    if (--entry.count <= 0) {
      this->contribute(entry.cb);
      this->startup_.erase(aid);
    }
#endif
  }

  using refnum_t = CMK_REFNUM_TYPE;

  // compress an array id into a cmk refnum
  // source ( https://stackoverflow.com/a/3058296/1426075 )
  static refnum_t compress(const CkArrayID &aid) {
    constexpr auto prime = 34283;
    constexpr auto nbits = 8 * sizeof(refnum_t);
    auto hash = array_id_hasher()(aid);
    constexpr auto mask = utilities::bitmask<decltype(hash)>(nbits);
    hash = ((hash >> nbits) ^ ((hash & mask) * prime)) & mask;
#if CMK_VERBOSE
    CkPrintf("info> compressed aid %d to %lx.\n", ((CkGroupID)aid).idx, hash);
#endif
    return (refnum_t)hash;
  }

 public:
  tree_builder(void) : CkArrayListener(0) {
    CksvInitialize(CProxy_tree_builder, singleton_);
    CksvAccess(singleton_) = thisProxy;
#if CMK_SMP
    this->lockCounts_.resize(CkMyNodeSize());
    this->lock_ = CmiCreateLock();
#endif
  }

  ~tree_builder() {
#if CMK_SMP
    CmiDestroyLock(this->lock_);
#endif
  }

  /* this is not implemented because its behavior would be undefined.
   * arrays assume their listeners are conventionally pup'able, which a
   * node/group chare is not
   */
  virtual const PUP::able::PUP_ID &get_PUP_ID(void) const { NOT_IMPLEMENTED; }

  // this is not implemented because arrays do not permit removing listeners
  void unreg_array(const CkArrayID &, const CkCallback &) { NOT_IMPLEMENTED; }

  /* register an array with this tree builder:
   * 1) set it as inserting and start quiescence detection
   * 2) run the start up process on all ranks of this node (smp)
   * 3) "send" the callback when all nodes have finished
   */
  void reg_array(const CkArrayID &aid, const CkCallback &cb) {
    this->lock();
    auto search = this->arrays_.find(aid);
    if (search == std::end(this->arrays_)) {
      this->arrays_[aid] = 0;
      this->insertion_statuses_[aid] = true;
#if CMK_SMP
      this->startup_.emplace(aid, cb);
#endif
    } else {
      CkAbort("array registered twice!");
    }
    this->unlock();

    for (auto i = 0; i < CkMyNodeSize(); i += 1) {
      if (i == CkMyRank()) {
        this->startup_process_(aid);
      } else {
#if CMK_SMP
        CmiPushPE(i, new back_inserter(this->ckGetGroupID(), aid));
#else
        NOT_IMPLEMENTED;
#endif
      }
    }

#if !CMK_SMP
    this->contribute(cb);
#endif
  }

  // lookup an element through the registry
  // returns nullptr if not found
  ArrayElement *lookup(const CkArrayID &aid, const index_type &idx) {
    return static_cast<ArrayElement *>(this->lookup(aid, idx, true));
  }

  using stamp_type = manageable_base_::stamp_type;

  // overload of create_child
  std::pair<association_ptr_, stamp_type> create_child(
      const element_type &elt, const index_type &child) {
    return this->create_child(elt->ckGetArrayID(), elt->ckGetArrayIndex(),
                              child);
  }

  // register the specified index as a child of the specified element
  // returns an association and a stamp for constructing the child
  std::pair<association_ptr_, stamp_type> create_child(
      const CkArrayID &aid, const index_type &parent, const index_type &child) {
    this->lock();
    auto &rec = this->record_for(aid, parent, false);
    auto last = rec.first->__stamp__(&child);
    // add the child as a downstream member of future collective ops
    rec.first->put_downstream_(child);
    // this avoids un/locking the individual record
    CkAssertMsg(rec.second == CkMyRank(),
                "a child must be created on the same pe as its parent");
    this->unlock();
    // create an association for the child, with parent as its upstream
    association_ptr_ a(new hypercomm::association_);
    a->valid_upstream_ = true;
    a->upstream_.emplace_back(parent);
    return std::make_pair(std::move(a), last);
  }

  // begin a phase of inserting for an already-registered array
  // callback is triggered when all nodes are ready for insertions
  // ( note, this specifically pertains to out-of-tree insertions )
  void begin_inserting(const CkArrayID &aid, const CkCallback &cb) {
    this->lock();
    auto search = this->insertion_statuses_.find(aid);
    CkAssertMsg(search != std::end(this->insertion_statuses_),
                "could not find aid, was it registered?");
    search->second = true;  // set is_inserting == true
    this->contribute(cb);
    this->unlock();
  }

  void reg_endpoint(const int &src, const CkArrayID &aid,
                    const CkArrayIndex &idx) {
    this->reg_endpoint(endpoint_(src), aid, idx);
  }

  // register the specified element as the endpoint of its spanning
  // tree; as in, it has no upstream members, aka, root.
  void reg_endpoint(const endpoint_ &ep, const CkArrayID &aid,
                    const CkArrayIndex &idx) {
    this->lock();
    // set the endpoint in our registry
    auto ins = this->endpoints_.emplace(aid, idx);
    CkAssertMsg(ins.second, "insertion did not occur");
    // attempt to lookup the specified element
    auto *elt = ep.elt_ ? ep.elt_ : this->lookup(aid, idx, false);
    CkAssert(!elt || idx == elt->ckGetArrayIndex());
    // if we have it, set it as the endpoint
    if (elt) elt->set_endpoint_();
    // then propagate the information downstream
    auto &mine = this->thisIndex;
    auto leaves = binary_tree::leaves(mine, CkNumNodes());
    for (const auto &leaf : leaves) {
      thisProxy[leaf].reg_endpoint(mine, aid, idx);
    }
    // TODO stop including this in QD counts!
    this->arrays_[aid] += qd_count_t(leaves.size()) - 1;
    this->unlock();
  }

  // receive a request to make idx an upstream member of a local chare,
  // from the specified node
  void receive_upstream(const int &node, const CkArrayID &aid,
                        const index_type &idx) {
#if CMK_VERBOSE
    CkPrintf("nd%d> received upstream from %d, idx=%s.\n", CkMyNode(), node,
             utilities::idx2str(idx).c_str());
#endif
    this->lock();
    this->reg_upstream(endpoint_(node), aid, idx);
    this->arrays_[aid] -= 1;  // qd consume (1)
    this->unlock();
  }

  // receive a request to make idx an downstream member of a local
  // chare, from the specified node
  void receive_downstream(const int &node, const CkArrayID &aid,
                          const index_type &idx) {
#if CMK_VERBOSE
    CkPrintf("nd%d> received downstream from %d, idx=%s.\n", CkMyNode(), node,
             utilities::idx2str(idx).c_str());
#endif
    this->lock();
    auto target = this->reg_downstream(endpoint_(node), aid, idx);
    // if we fail to locally register the idx, we need to send it upstream
    if (target != nullptr) {
      // so we implicitly produce (1) at this point
      thisProxy[node].receive_upstream(CkMyNode(), aid,
                                       target->ckGetArrayIndex());
    } else {
      // thus, we only consume (1) in the else branch
      this->arrays_[aid] -= 1;
    }
    this->unlock();
  }

  // determine whether aid is in a phase of static insertion
  inline bool is_inserting(const CkArrayID &aid) const {
    auto search = this->insertion_statuses_.find(aid);
    return (search == std::end(this->insertion_statuses_)) ? false
                                                           : search->second;
  }

  // get the array's endpoint (last known root of spanning tree)
  inline const index_type *endpoint_for(const CkArrayID &aid) const {
    auto search = this->endpoints_.find(aid);
    return (search == std::end(this->endpoints_)) ? nullptr : &(search->second);
  }

  static tree_builder *instance(void) {
    return CksvAccess(singleton_).ckLocalBranch();
  }

 protected:
  // pull the record for the element, fails if not found
  record_type &record_for(const CkArrayID &aid, const index_type &idx,
                          const bool &lock) {
    if (lock) this->lock();
    auto &rec = this->elements_[aid].find(idx)->second;
    if (lock) this->unlock();
    return rec;
  }

  // find the local pointer of an element, returns nullptr if not found
  element_type lookup(const CkArrayID &aid, const index_type &idx,
                      const bool &lock) {
    if (lock) this->lock();
    auto &elements = this->elements_[aid];
    auto search = elements.find(idx);
    auto *elt = (search == std::end(elements)) ? nullptr : search->second.first;
    if (lock) this->unlock();
    return elt;
  }

  // send the specified element ( upstream ) to find it a parent
  void send_upstream(const endpoint_ &ep, const CkArrayID &aid,
                     const index_type &idx) {
    // determine our parent in the node spanning tree
    auto mine = CkMyNode();
    auto parent = binary_tree::parent(mine);
    // if we are not at the root of the node spanning tree
    if (parent >= 0) {
      // we can send it ( upstream ), this means it will be
      // ( downstream ) relative to the receiver
      auto src = ep.elt_ != nullptr ? mine : ep.node_;
      thisProxy[parent].receive_downstream(src, aid, idx);
    } else {
      // we are at the root, so register the endpoint
      this->reg_endpoint(ep, aid, idx);
    }
    this->arrays_[aid] += 1;  // qd create (1)
  }

  void send_downstream(const endpoint_ &ep, const CkArrayID &aid,
                       const index_type &idx) {
    // unclear when this would occur, initial sync'ing should have destinations
    NOT_IMPLEMENTED;
  }

  using iter_type = typename elements_type::iterator;

  // find a target within the array, either:
  // a) seeking a target without an upstream member, or:
  // b) seeking the target with the fewest downstream members
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

  // attempt to find a downstream child for the given element
  // returns nullptr if a remote request had to be sent
  element_type reg_upstream(const endpoint_ &ep, const CkArrayID &aid,
                            const index_type &idx) {
    auto search = this->find_target(aid, true);
    if (search == std::end(this->elements_[aid])) {
      this->send_downstream(ep, aid, idx);
      return nullptr;
    } else {
      // set the element as the parent of the found target
      auto &target = search->second.first;
      target->put_upstream_(idx);
      return target;
    }
  }

  // attempt to find an upstream parent for the given element
  // returns nullptr if a remote request had to be sent
  element_type reg_downstream(const endpoint_ &ep, const CkArrayID &aid,
                              const index_type &idx) {
    auto search = this->find_target(aid, false);
    if (search == std::end(this->elements_[aid])) {
      this->send_upstream(ep, aid, idx);
      return nullptr;
    } else {
      // set the element as the child of the found target
      auto &target = search->second.first;
      target->put_downstream_(idx);
      return target;
    }
  }

  void try_reassociate(const element_type &elt) {
    CkError("warning> try_reassociate not implemented\n");
  }

  // called when elements are initially created to (re)associate them
  void associate(const CkArrayID &aid, const element_type &elt) {
    auto &assoc = elt->association_;
    // if we are (statically) inserting an unassociated element
    if (this->is_inserting(aid) && !assoc) {
      // create a new association for the element
      assoc.reset(new hypercomm::association_);
      // and set its up/downstream
      auto target = this->reg_downstream(endpoint_(elt), elt->ckGetArrayID(),
                                         elt->ckGetArrayIndex());
      // if the target is local,
      if (target != nullptr) {
        // set this element as a target
        elt->put_upstream_(target->ckGetArrayIndex());
      }
    } else {
      CkAssertMsg(assoc, "dynamic insertions must be associated");
      this->try_reassociate(elt);
    }
  }

  // called when elements are destroyed to disassociate them from the
  // per-element spanning tree
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
      auto interceptor = interceptor_[CkMyPe()];
      // replace this elt with its children in its parent
      // (there may be none! that is ok, effectively a "delete".)
      auto *msg = hypercomm::pack(curr, children, elt->__stamp__());
      UsrToEnv(msg)->setEpIdx(
          CkIndex_locality_base_::idx_replace_downstream_CkMessage());
      interceptor.deliver(aid, parent, msg);
      // if we had children,
      if (!children.empty()) {
#if CMK_VERBOSE
        CkPrintf("%s> forwarding messages to %s.\n",
                 utilities::idx2str(curr).c_str(),
                 utilities::idx2str(parent).c_str());
#endif
        // redirect their downstream messages to our parent
        interceptor.forward(aid, curr, parent);
      }
    }
  }

  // called whenever an element locally manifests
  void reg_element(ArrayElement *elt, const bool &created) {
    auto cast = dynamic_cast<element_type>(elt);
    auto &aid = cast->ckGetArrayID();
    auto &idx = cast->ckGetArrayIndex();
    this->lock();
    if (created) {
      auto *loc = interceptor::local_branch();
      loc->stop_forwarding(aid, idx);

      this->associate(aid, cast);
    }
    this->elements_[aid][idx] = std::make_pair(cast, CkMyRank());
    this->unlock();
  }

  // called whenever an element locally vanishes
  void unreg_element(ArrayElement *elt, const bool &died) {
    auto cast = dynamic_cast<element_type>(elt);
    auto &aid = cast->ckGetArrayID();
    auto &idx = cast->ckGetArrayIndex();
    this->lock();
    auto &elements = this->elements_[aid];
    auto search = elements.find(idx);
    if (search != std::end(elements)) {
      elements.erase(search);
    }
    if (died) disassociate(aid, cast);
    this->unlock();
  }

 public:
  // interface layer for charm++
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

template <typename Index>
const CkArrayIndex *managed_imprintable<Index>::pick_root(
    const CkArrayID &aid) const {
  return (tree_builder::instance())->endpoint_for(aid);
}

// TODO ( make this an init'd readonly )
CProxy_tree_builder tree_builder::CksvAccess(singleton_);

#if CMK_SMP
// register the handler for the back_inserter, returning its index
const int &back_inserter::handler(void) {
  CpvStaticDeclare(int, back_inserter_handler_);

  if (!CpvInitialized(back_inserter_handler_)) {
    CpvInitialize(int, back_inserter_handler_);
    CpvAccess(back_inserter_handler_) =
        CmiRegisterHandler((CmiHandler)back_inserter::handler_);
  }

  return CpvAccess(back_inserter_handler_);
}

// call the startup process for the given tree builder and aid
void back_inserter::handler_(back_inserter *msg) {
  auto *builder = CProxy_tree_builder(msg->gid).ckLocalBranch();
  builder->startup_process_(msg->aid);
  delete msg;
}
#endif
}  // namespace hypercomm

// TODO ( move to library )
#include <hypercomm/tree_builder/tree_builder.def.h>

#endif
