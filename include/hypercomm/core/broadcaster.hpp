#ifndef __HYPERCOMM_CORE_BROADCASTER_HPP__
#define __HYPERCOMM_CORE_BROADCASTER_HPP__

#include "../sections/imprintable.hpp"

namespace hypercomm {
template <typename BaseIndex, typename Index>
class broadcaster : public immediate_action<void(indexed_locality_<Index>*)> {
 public:
  using imprintable_ptr = std::shared_ptr<imprintable<Index>>;

 private:
  imprintable_ptr imprintable_;
  std::shared_ptr<hypercomm::message> msg_;

 public:
  broadcaster(PUP::reconstruct) {}

  broadcaster(const imprintable_ptr& _1, decltype(msg_)&& _2)
      : imprintable_(_1), msg_(_2) {}

  broadcaster(const imprintable_ptr& _1, hypercomm::message* _2)
      : imprintable_(_1),
        msg_(std::static_pointer_cast<hypercomm::message>(
            utilities::wrap_message(_2))) {}

  virtual void action(indexed_locality_<Index>* locality) override {
    auto collective = std::dynamic_pointer_cast<collective_proxy<BaseIndex>>(
        locality->__gencon__());
    const auto& identity = this->imprintable_->imprint(locality);
    auto ustream = identity->upstream();
    for (const auto& up : ustream) {
      auto copy = std::static_pointer_cast<hypercomm_msg>(
          utilities::copy_message(msg_));
      auto next = std::make_shared<broadcaster<BaseIndex, Index>>(
          this->imprintable_, std::move(copy));
      send_action(collective, conv2idx<BaseIndex>(up), std::move(next));
    }

    locality->receive_message(static_cast<hypercomm::message*>(
        utilities::unwrap_message(std::move(msg_))));
  }

  virtual void __pup__(serdes& s) override {
    s | imprintable_;
    s | msg_;
  }
};
}

#endif
