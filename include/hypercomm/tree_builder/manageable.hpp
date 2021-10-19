#ifndef __HYPERCOMM_TREE_BUILDER_MANAGEABLE_HPP__
#define __HYPERCOMM_TREE_BUILDER_MANAGEABLE_HPP__

#include "managed_imprintable.hpp"

#include "../core/locality.hpp"

namespace hypercomm {
template <typename T>
class manageable : public T {
  using index_type_ = typename T::index_type;
  using port_type_ = reduction_port<index_type_>;
  using identity_type_ = identity<index_type_>;
  using imprintable_type_ = imprintable<index_type_>;
  using identity_ptr_ = std::shared_ptr<identity_type_>;

  const identity_ptr_& identity_;

  virtual stamp_type __stamp__(const CkArrayIndex* idx) const override {
    stamp_type stamp;
    for (const auto& entry : this->identities) {
      if (idx == nullptr || (entry.first)->is_member(*idx)) {
        stamp.emplace(entry.first, (entry.second)->last_reduction());
      }
    }
    return std::move(stamp);
  }

  enum transaction_type_ { kReplace, kDelete };

  // a replace or delete event for downstream children
  struct transaction_ {
    transaction_type_ type;
    CkArrayIndex from;
    CkArrayIndex to;
    stamp_type stamp;

    transaction_(const CkArrayIndex& _1, stamp_type&& _2)
        : type(kDelete), from(_1), stamp(_2) {}

    transaction_(const CkArrayIndex& _1, const CkArrayIndex& _2,
                 stamp_type&& _3)
        : type(kReplace), from(_1), to(_2), stamp(_3) {}
  };

  std::vector<transaction_> staged_;

  // call a function on all ports affected by the specified stamp
  template <typename Fn>
  void affect_ports(const index_type_& idx, const stamp_type& stamp,
                    const Fn& fn) {
    auto copy = this->entry_ports;
    for (auto& val : copy) {
      auto port = std::dynamic_pointer_cast<port_type_>(val.first);
      if (port && port->affected_by(stamp) && (port->index == idx)) {
        fn(port);
      }
    }
  }

  // attempt to resolve all staged transactions, recursing until none
  // can be resolved (e.g., affected index not found)
  void resolve_transactions(void) {
    auto& ds = this->association_->downstream_;
    for (auto it = std::begin(this->staged_); it != std::end(this->staged_);
         it++) {
      auto& idx = reinterpret_index<index_type_>(it->from);
      auto search = std::find(std::begin(ds), std::end(ds), it->from);
      if (search != std::end(ds)) {
        switch (it->type) {
          case kDelete: {
            ds.erase(search);
            this->affect_ports(idx, it->stamp,
                               [&](const std::shared_ptr<port_type_>& port) {
#if CMK_VERBOSE
                                 CkPrintf("info> sending invalidation to %s.\n",
                                          std::to_string(idx).c_str());
#endif
                                 // deletions send an invalidation/null value to
                                 // the reducer
                                 this->receive(deliverable(value_ptr(), port));
                               });
            break;
          }
          case kReplace: {
            *search = it->to;
            auto& next = reinterpret_index<index_type_>(it->to);
            this->affect_ports(idx, it->stamp,
                               [&](const std::shared_ptr<port_type_>& port) {
#if CMK_VERBOSE
                                 CkPrintf("info> replacing %s with %s.\n",
                                          std::to_string(idx).c_str(),
                                          std::to_string(next).c_str());
#endif
                                 // find the port within our map
                                 auto iter = this->entry_ports.find(port);
                                 // copy the old dest to keep it alive
                                 auto dst = std::move(iter->second);
                                 // erase it from the port map
                                 // ( could use C++17 extract for this)
                                 this->entry_ports.erase(iter);
                                 // update the index of the port
                                 port->index = next;
                                 // reopen the port with its updated index/hash
                                 this->open(port, std::move(dst));
                               });
            break;
          }
          default:
            break;
        }
        this->staged_.erase(it);
        this->resolve_transactions();
        return;
      }
    }
  }

  // adds idx as a child of any collective operations that are underway,
  // starting at the epoch number specified by stamp
  void add_downstream(const CkArrayIndex& idx, const stamp_type& stamp) {
    // add the downstream member to future reductions
    auto& down = reinterpret_index<index_type_>(idx);
    this->association_->downstream_.emplace_back(idx);
    // find the components that are affected by stamp
    for (auto& pair : this->components) {
      auto rdcr = dynamic_cast<reducer*>(pair.second.get());
      if (rdcr && rdcr->affected_by(stamp)) {
        auto port =
            std::make_shared<reduction_port<index_type_>>(rdcr->stamp(), down);
        // open another input port in the reducer (via increment)
        access_context_()->connect(port, rdcr->id, rdcr->n_ustream++);
      }
    }
  }

  // initialize identities based on our parent's stamp -- returning
  // the managed identity
  const identity_ptr_& initialize_identities(stamp_type&& stamp) {
    using namespace std::placeholders;
    // make a function that returns true when it finds the managed_imprintable
    auto& seek = managed_imprintable<index_type_>::instance();
    auto fn = std::bind(
        comparable_comparator<std::shared_ptr<imprintable_type_>>(), seek, _1);
    // then, for all entries in the stamp
    const identity_ptr_* res = nullptr;
    for (const auto& entry : stamp) {
      // validate we are a member of the imprintable
      auto& gen = entry.first;
      CkAssert(gen->is_member(this->ckGetArrayIndex()));
      // then create the identity using the seed
      auto cast = std::dynamic_pointer_cast<imprintable_type_>(std::move(gen));
      auto& ins = this->emplace_identity(cast, std::move(entry.second));
      // and, if it is the managed_identity
      if (fn(cast)) {
        // this will only be hit if the fn is not working correctly
        CkAssertMsg(res == nullptr, "duplicate managed imprintables found");
        // set it as the return value
        res = &ins;
      }
    }
    // if we could not find the managed identity
    if (res == nullptr) {
      // then create one
      return this->emplace_identity(seek, {});
    } else {
      // otherwise, return it
      return *res;
    }
  }

 public:
  // used for static insertion, default initialize
  manageable(void)
      : identity_(this->emplace_identity(
            managed_imprintable<index_type_>::instance(), {})) {}

  // used for dynamic insertion, imprints child with parent data
  manageable(association_ptr_&& association, stamp_type&& stamp)
      : identity_(
            this->initialize_identities(std::forward<stamp_type>(stamp))) {
    this->set_association_(std::forward<association_ptr_>(association));
  }

  // version of replace_downstream that is callable as an EP
  virtual void replace_downstream(CkMessage* msg) override {
    CkArrayIndex from;
    std::vector<CkArrayIndex> to;
    stamp_type stamp;

    hypercomm::unpack(msg, from, to, stamp);

    this->replace_downstream(from, std::move(to), std::move(stamp));
  }

  /* replaces downstream child (from) with the members of (to),
   * starting at the epoch number specified by (stamp).
   *
   * collectives before the stamp will remain unchanged, since
   * we should expect a value from them.
   */
  void replace_downstream(const CkArrayIndex& from,
                          std::vector<CkArrayIndex>&& to, stamp_type&& stamp) {
    this->update_context();
    // if there are no children
    if (to.empty()) {
      // stage a deletion event
      this->staged_.emplace_back(from, std::move(stamp));
    } else {
      // otherwise, children 1:n, add them as downstream directly
      for (auto it = (std::begin(to) + 1); it != std::end(to); it += 1) {
        this->add_downstream(*it, stamp);
      }
      // then replace (from) with child 0
      this->staged_.emplace_back(from, *(std::begin(to)), std::move(stamp));
    }
    // resolve the staged transactions
    this->resolve_transactions();
  }

  // debugging helper method
  inline void ckPrintTree(const char* msg) const {
    std::stringstream ss;

    auto upstream =
        !(this->association_ && this->association_->valid_upstream_)
            ? "unset"
            : (this->association_->upstream_.empty()
                   ? "endpoint"
                   : utilities::idx2str(this->association_->upstream_.front()));

    ss << utilities::idx2str(this->ckGetArrayIndex()) << "@nd" << CkMyNode();
    ss << "> " << msg << ", has upstream " << upstream << " and downstream [";
    for (const auto& ds : this->association_->downstream_) {
      ss << utilities::idx2str(ds) << ",";
    }
    ss << "]";

    CkPrintf("%s\n", ss.str().c_str());
  }
};
}  // namespace hypercomm

#endif
