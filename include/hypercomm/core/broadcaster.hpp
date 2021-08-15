#ifndef __HYPERCOMM_CORE_BROADCASTER_HPP__
#define __HYPERCOMM_CORE_BROADCASTER_HPP__

#include "../sections/imprintable.hpp"
#include "../sections/root_listener.hpp"
#include "../messaging/interceptor.hpp"

namespace hypercomm {
template <typename BaseIndex, typename Index>
class broadcaster : public detail::root_listener,
                    public immediate_action<void(indexed_locality__<Index>*)>,
                    public virtual_enable_shared_from_this<broadcaster<BaseIndex, Index>> {
 public:
  using imprintable_ptr = std::shared_ptr<imprintable<Index>>;
  using stamp_type = std::pair<Index, reduction_id_t>;

 private:
  std::unique_ptr<stamp_type> stamp_;
  imprintable_ptr imprintable_;
  std::shared_ptr<hypercomm::message> msg_;
  indexed_locality_<Index>* locality_ = nullptr;

 public:
  broadcaster(PUP::reconstruct) {}

  broadcaster(const stamp_type& _1, const imprintable_ptr& _2,
              decltype(msg_)&& _3)
      : stamp_(new stamp_type(_1)), imprintable_(_2), msg_(_3) {}

  broadcaster(const stamp_type& _1, const imprintable_ptr& _2,
              hypercomm::message* _3)
      : broadcaster(_1, _2, utilities::wrap_message(_3)) {}

  broadcaster(const imprintable_ptr& _2, hypercomm::message* _3)
      : stamp_(nullptr), imprintable_(_2), msg_(utilities::wrap_message(_3)) {}

  bool try_broadcast(void) {
    // gather all the information for this broadcaster's imprintable
    const auto& identity = this->locality_->identity_for(imprintable_);
    auto& mine = identity->mine();
    auto& root = this->stamp_ ? this->stamp_->first : mine;

    if (mine == root) {
      auto* known = identity->root();
      if (!known || mine != *known) {
        // received a broadcast ahead of schedule
        return true;
      }
    }

    auto count =
        this->stamp_ ? this->stamp_->second : identity->next_broadcast();
    auto upstream = identity->upstream();

    // for all the indices in the list
    for (const auto& idx : upstream) {
      // copy the broadcasted message
      auto copy = std::static_pointer_cast<message>(
          utilities::copy_message(this->msg_));
      // use it to create the broadcaster for the next element
      auto next = std::make_shared<broadcaster<BaseIndex, Index>>(
          std::make_pair(root, count), this->imprintable_, std::move(copy));
      // then send it along, remotely executing it
      interceptor::send_async(this->locality_->__element_at__(idx), pack_action(next));
    }

    // after that is done, check whether we should locally accept the bcast
    if (identity->accept_broadcast(count)) {
      this->locality_->receive_message(utilities::unwrap_message(std::move(msg_)));
    }

    return false;
  }

  virtual void action(indexed_locality_<Index>* locality) override {
    this->locality_ = locality;
    this->try_broadcast();
  }

  virtual void __pup__(serdes& s) override {
    if (this->locality_) {
      CkAbort("cannot serialize broadcast in transient state");
    }

    s | this->stamp_;
    s | this->imprintable_;
    s | this->msg_;
  }

  virtual bool on_root_assignment(const CkArrayID&, const CkArrayIndex&) {
    return this->try_broadcast();
  }
};
}  // namespace hypercomm

#endif
