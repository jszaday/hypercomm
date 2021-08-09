#ifndef __HYPERCOMM_CORE_BROADCASTER_HPP__
#define __HYPERCOMM_CORE_BROADCASTER_HPP__

#include "../sections/imprintable.hpp"

namespace hypercomm {
template <typename BaseIndex, typename Index>
class broadcaster : public immediate_action<void(indexed_locality_<Index>*)> {
 public:
  using imprintable_ptr = std::shared_ptr<imprintable<Index>>;

 private:
  // TODO ( add a "progenitor" field to ensure b/c's aren't delivered twice )
  Index last_;
  imprintable_ptr imprintable_;
  std::shared_ptr<hypercomm::message> msg_;

 public:
  broadcaster(PUP::reconstruct) {}

  broadcaster(const Index& _1, const imprintable_ptr& _2, decltype(msg_)&& _3)
      : last_(_1), imprintable_(_2), msg_(_3) {}

  broadcaster(const Index& _1, const imprintable_ptr& _2,
              hypercomm::message* _3)
      : broadcaster(_1, _2,
                    std::static_pointer_cast<hypercomm::message>(
                        utilities::wrap_message(_3))) {}

  virtual void action(indexed_locality_<Index>* locality) override {
    // gather all the information for this broadcaster's imprintable
    const auto& identity = locality->identity_for(imprintable_);
    auto mine = identity->mine();
    auto upstream = identity->upstream();
    auto downstream = identity->downstream();

    auto helper = [&](const std::vector<Index>& indices) {
      // for all the indices in the list
      for (const auto& idx : indices) {
        if (this->last_ == idx) {
          continue;  // besides the element that created this
                     // broadcaster (to prevent recurrences)
        } else {
          // copy the broadcasted message
          auto copy = std::static_pointer_cast<message>(
              utilities::copy_message(this->msg_));
          // use it to create the broadcaster for the next element
          auto next = std::make_shared<broadcaster<BaseIndex, Index>>(
              mine, this->imprintable_, std::move(copy));
          // then send it along, remotely executing it
          send_action(locality->__element_at__(idx), std::move(next));
        }
      }
    };

    // apply the helper function to all incoming/outgoing edges
    helper(downstream);
    helper(upstream);

    // after that is done, send the message to this element
    locality->receive_message(static_cast<hypercomm::message*>(
        utilities::unwrap_message(std::move(msg_))));
  }

  virtual void __pup__(serdes& s) override {
    s | this->last_;
    s | this->imprintable_;
    s | this->msg_;
  }
};
}  // namespace hypercomm

#endif
