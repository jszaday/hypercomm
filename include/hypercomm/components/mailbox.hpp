#ifndef __HYPERCOMM_COMPONENTS_MAILBOX_HPP__
#define __HYPERCOMM_COMPONENTS_MAILBOX_HPP__

#include "component.hpp"
#include "../core/immediate.hpp"

namespace hypercomm {

template<typename T>
class mailbox: public virtual component {
 public:
  using predicate_type = std::shared_ptr<immediate_action<bool(const T&)>>;
  using request_type = std::pair<predicate_type, callback_ptr>;

 protected:
  std::deque<request_type> requests_;
  std::deque<std::shared_ptr<typed_value<T>>> buffer_;

public:
  mailbox(const id_t& _1) : component(_1)  {}

  virtual std::size_t n_inputs(void) const override { return 1; }
  virtual std::size_t n_outputs(void) const override { return 0; }
  virtual bool keep_alive(void) const override { return true; }

  virtual void put_request(const predicate_type& pred, const callback_ptr& cb) {
    auto search = this->find_in_buffer(pred);
    if (search == std::end(this->buffer_)) {
      this->requests_.emplace_back(pred, cb);
    } else {
      QdProcess(1);
      cb->send(std::move(*search));
      this->buffer_.erase(search);
    }
  }

  virtual value_set action(value_set&& accepted) override {
    auto value = value2typed<T>(std::move(accepted[0]));
    auto search = this->find_matching(value);

    if (search == std::end(this->requests_)) {
      QdCreate(1);
      this->buffer_.emplace_back(std::move(value));
    } else {
      search->second->send(std::move(value));
    }

    return {};
  }

 protected:
  using buffer_iterator = typename decltype(buffer_)::iterator;

  inline buffer_iterator find_in_buffer(const predicate_type& pred) {
    buffer_iterator search = std::begin(this->buffer_);
    for (; search != std::end(this->buffer_); search++) {
      if (pred->action((*search)->value())) {
        break;
      }
    }
    return search;
  }

  using request_iterator = typename decltype(requests_)::iterator;

  inline request_iterator find_matching(const std::shared_ptr<typed_value<T>>& _1) {
    const auto& value = _1->value();
    request_iterator search = std::begin(this->requests_);
    for (; search != std::end(this->requests_); search++) {
      if (search->first->action(value)) {
        break;
      }
    }
    return search;
  }
};

}

#endif
