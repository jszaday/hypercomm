#ifndef __HYPERCOMM_CORE_BROADCASTER_HPP__
#define __HYPERCOMM_CORE_BROADCASTER_HPP__

#include "../sections/imprintable.hpp"
#include "../messaging/interceptor.hpp"

namespace hypercomm {
template <typename BaseIndex, typename Index>
class broadcaster : public immediate_action<void(indexed_locality_<Index>*)> {
 public:
  using imprintable_ptr = std::shared_ptr<imprintable<Index>>;
  using stamp_type = std::pair<Index, reduction_id_t>;

 private:
  std::unique_ptr<stamp_type> stamp_;
  imprintable_ptr imprintable_;
  std::shared_ptr<hypercomm::message> msg_;

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

  virtual void action(indexed_locality_<Index>* locality) override {
    // gather all the information for this broadcaster's imprintable
    const auto& identity = locality->identity_for(imprintable_);
    auto& mine = identity->mine();
    auto& root = this->stamp_ ? this->stamp_->first : mine;
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
      interceptor::send_async(locality->__element_at__(idx), pack_action(next));
    }

    // after that is done, check whether we should locally accept the bcast
    if (identity->accept_broadcast(count)) {
      locality->receive(utilities::unwrap_message(std::move(msg_)));
    }
  }

  virtual void __pup__(serdes& s) override {
    s | this->stamp_;
    s | this->imprintable_;
    s | this->msg_;
  }
};
}  // namespace hypercomm

#endif
