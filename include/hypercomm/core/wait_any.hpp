#ifndef __HYPERCOMM_CORE_WAIT_ANY_HPP__
#define __HYPERCOMM_CORE_WAIT_ANY_HPP__

#include "locality.hpp"
#include "resuming_callback.hpp"
#include "value_wrapper.hpp"

namespace hypercomm {

namespace {
using resumer_pair = std::pair<future, value_wrapper>;
using resumer_type = std::shared_ptr<resuming_callback<resumer_pair>>;
}  // namespace

struct wait_any_callback : public core::callback {
  resumer_type target;
  future fut;

  wait_any_callback(const resumer_type& _1, const future& _2)
      : target(_1), fut(_2) {}

  virtual void send(value_type&& value) override {
    if (target->ready()) {
      // return the value to the source
      fut.set(std::move(value));
    } else {
      auto paired = make_typed_value<resumer_pair>(fut, std::move(value));
      target->send(std::move(paired));
    }
  }

  virtual void __pup__(serdes& s) override { NOT_IMPLEMENTED; }
};

// returns a pair with a value and an iterator pointing to a future
// whose value was received, takes the first value that becomes available
template <typename InputIter>
inline std::pair<value_ptr, InputIter> wait_any(const InputIter& first,
                                                const InputIter& last) {
  auto* ctx = dynamic_cast<future_manager_*>(access_context_());
  auto cb = std::make_shared<typename resumer_type::element_type>();
  // for all the futures (unless a ready one is found first)
  for (auto it = first; it != last; it++) {
    // make a callback that will forward the value
    auto next = std::make_shared<wait_any_callback>(cb, *it);
    // will not be true after req, so we check this beforehand
    auto brk = it->ready();
    // then request the future's value
    ctx->request_future(*it, std::move(next));
    // break if the future was ready (to avoid unecessary reqs)
    if (brk) break;
  }
  // wait for the value to arrive
  cb->wait();
  // extract its parts
  auto& pair = cb->value();
  auto& which = pair.first;
  auto& value = *pair.second;
  // seek the future that was recvd
  auto search = std::find_if(first, last,
                             [&](const future& f) { return f.equals(which); });
  // return the iter/value pair
  return std::make_pair(std::move(value), search);
}
}  // namespace hypercomm

#endif
