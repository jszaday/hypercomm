#ifndef __HYPERCOMM_CORE_BROADCASTER_HPP__
#define __HYPERCOMM_CORE_BROADCASTER_HPP__

#include "../sections/imprintable.hpp"

namespace hypercomm {
template <typename BaseIndex, typename Index>
class broadcaster : public immediate_action<void(indexed_locality_<Index>*)> {
 public:
  using imprintable_ptr = std::shared_ptr<imprintable<Index>>;

 private:
  Index last_;
  imprintable_ptr imprintable_;
  std::shared_ptr<hypercomm::message> msg_;

 public:
  broadcaster(PUP::reconstruct) {}

  broadcaster(const Index& _1, const imprintable_ptr& _2, decltype(msg_)&& _3)
      : last_(_1), imprintable_(_2), msg_(_3) {}

  broadcaster(const Index& _1, const imprintable_ptr& _2,
              hypercomm::message* _3)
      : broadcaster(_1, _2, std::static_pointer_cast<hypercomm::message>(
                                utilities::wrap_message(_3))) {}

  virtual void action(indexed_locality_<Index>* locality) override {
    auto collective = std::dynamic_pointer_cast<collective_proxy<BaseIndex>>(
        locality->__gencon__());

    const auto& identity = locality->identity_for(imprintable_);

    auto mine = identity->mine();
    auto upstream = identity->upstream();
    auto downstream = identity->downstream();

    std::vector<Index> children;
    auto helper = [&](const Index& idx) { return this->last_ != idx; };

    std::copy_if(std::begin(upstream), std::end(upstream),
                 std::back_inserter(children), helper);
    std::copy_if(std::begin(downstream), std::end(downstream),
                 std::back_inserter(children), helper);

    for (const auto& child : children) {
      auto copy = std::static_pointer_cast<hypercomm_msg>(
          utilities::copy_message(msg_));
      auto next = std::make_shared<broadcaster<BaseIndex, Index>>(
          mine, this->imprintable_, std::move(copy));
      send_action(collective, conv2idx<BaseIndex>(child), std::move(next));
    }

    locality->receive_message(static_cast<hypercomm::message*>(
        utilities::unwrap_message(std::move(msg_))));
  }

  virtual void __pup__(serdes& s) override {
    s | this->last_;
    s | this->imprintable_;
    s | this->msg_;
  }
};
}

#endif
