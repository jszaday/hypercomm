#ifndef __HYPERCOMM_CORE_BROADCASTER_HPP__
#define __HYPERCOMM_CORE_BROADCASTER_HPP__

#include "../sections/imprintable.hpp"
#include "../messaging/interceptor.hpp"

namespace hypercomm {
template <typename BaseIndex, typename Index>
class broadcaster : public immediate_action<void(indexed_locality_<Index>*)> {
 public:
  using imprintable_ptr = std::shared_ptr<imprintable<Index>>;

 private:
  // TODO ( add a "progenitor" field to ensure b/c's aren't delivered twice )
  std::unique_ptr<Index> last_;
  imprintable_ptr imprintable_;
  std::shared_ptr<hypercomm::message> msg_;

 public:
  broadcaster(PUP::reconstruct) {}

  broadcaster(const Index& _1, const imprintable_ptr& _2, decltype(msg_)&& _3)
      : last_(new Index(_1)), imprintable_(_2), msg_(_3) {}

  broadcaster(const Index& _1, const imprintable_ptr& _2,
              hypercomm::message* _3)
      : broadcaster(_1, _2, utilities::wrap_message(_3)) {}

  broadcaster(const imprintable_ptr& _2, hypercomm::message* _3)
      : last_(nullptr), imprintable_(_2), msg_(utilities::wrap_message(_3)) {}

  virtual void action(indexed_locality_<Index>* locality) override {
    // gather all the information for this broadcaster's imprintable
    const auto& identity = locality->identity_for(imprintable_);
    auto mine = identity->mine();
    auto upstream = identity->upstream();
    auto downstream = identity->downstream();

    auto helper = [&](const std::vector<Index>& indices) {
      // for all the indices in the list
      for (const auto& idx : indices) {
        if (this->last_ && idx == (*this->last_)) {
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
          interceptor::send_async(locality->__element_at__(idx),
                                  pack_action(next));
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
