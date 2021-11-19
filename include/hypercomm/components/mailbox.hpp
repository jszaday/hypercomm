#ifndef __HYPERCOMM_COMPONENTS_MAILBOX_HPP__
#define __HYPERCOMM_COMPONENTS_MAILBOX_HPP__

#include "../core/immediate.hpp"
#include "../core/typed_value.hpp"
#include "../utilities/weak_ref.hpp"
#include "../core/generic_locality.hpp"

namespace hypercomm {

template <typename T>
class mailbox : public component<T, std::tuple<>> {
 public:
  using parent_t = component<T, std::tuple<>>;
  using in_set = typename parent_t::in_set;
  using value_type = typename std::tuple_element<0, in_set>::type;

  using predicate_type =
      std::shared_ptr<immediate_action<bool(const value_type&)>>;
  using action_type = callback_ptr;
  using weak_ref_t = utilities::weak_ref<mailbox>;

  class request {
   public:
    predicate_type pred;
    destination dst;

    template <typename... Args>
    request(const predicate_type& pred_, Args&&... args)
        : pred(pred_), dst(std::forward<Args>(args)...) {}

    inline bool matches(const value_type& t) {
      return !pred || pred->action(t);
    }
  };

 protected:
  std::list<request> requests_;
  std::deque<value_type> buffer_;
  std::shared_ptr<weak_ref_t> weak_;

  using request_iterator = typename decltype(requests_)::iterator;

 public:
  mailbox(const id_t& _1) : parent_t(_1), weak_(new weak_ref_t(this)) {
    this->persistent = true;
  }

  ~mailbox() { weak_->reset(nullptr); }

  inline request_iterator put_request(const predicate_type& pred,
                                      const callback_ptr& cb) {
    auto search = this->find_in_buffer(pred);
    if (search == std::end(this->buffer_)) {
      this->requests_.emplace_front(pred, cb);
      return std::begin(this->requests_);
    } else {
      QdProcess(1);
      cb->send(std::move(*search));
      this->buffer_.erase(search);
      return std::end(this->requests_);
    }
  }

  inline request_iterator put_request_to(const predicate_type& pred,
                                         const component_id_t& com,
                                         const component_port_t& port) {
    auto search = this->find_in_buffer(pred);
    if (search == std::end(this->buffer_)) {
      this->requests_.emplace_front(pred, com, port);
      return this->requests_.begin();
    } else {
      QdProcess(1);
      deliverable dev(std::move(*search));
      this->buffer_.erase(search);
      auto* ctx = access_context_();
      CkAssert(dev && (ctx->magic == CHARE_MAGIC));
      auto status = ctx->passthru(std::make_pair(com, port), dev);
      CkAssert(status);
      return std::end(this->requests_);
    }
  }

  virtual std::tuple<> action(in_set& set) override {
    auto& value = std::get<0>(set);
    auto search = this->find_matching(value);

    if (search == std::end(this->requests_)) {
      QdCreate(1);
      this->buffer_.emplace_back(std::move(value));
    } else {
      auto req = std::move(*search);
      // cleanup the request before triggering
      // its action to prevent feedback loops
      this->requests_.erase(search);
      // package the value for delivery
      deliverable dev(std::move(value));
      // if delivery fails...
      if (!passthru_context_(req.dst, dev)) {
        // try again (via redelivery)!
        auto status = this->accept(0, dev);
        CkAssert(status);
      }
    }

    return {};
  }

 protected:
  using buffer_iterator = typename decltype(buffer_)::iterator;

  inline buffer_iterator find_in_buffer(const predicate_type& pred) {
    buffer_iterator search = std::begin(this->buffer_);
    for (; search != std::end(this->buffer_); search++) {
      if (!pred || pred->action(*search)) {
        break;
      }
    }
    return search;
  }

  inline request_iterator find_matching(const value_type& value) {
    for (auto it = (this->requests_).rbegin(); it != (this->requests_).rend();
         it++) {
      if (it->matches(value)) {
        return --(it.base());
      }
    }
    return std::end(this->requests_);
  }
};
}  // namespace hypercomm

#endif
