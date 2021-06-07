#ifndef __HYPERCOMM_COMPONENTS_MAILBOX_HPP__
#define __HYPERCOMM_COMPONENTS_MAILBOX_HPP__

#include "../core/typed_value.hpp"

#include "component.hpp"

namespace hypercomm {

template<typename T>
class mailbox: public virtual component,
               public virtual value_source,
               public std::enable_shared_from_this<mailbox<T>> {
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

  virtual void take_back(value_type&& value) override {
    this->receive_value(0, std::move(value));
  }

  virtual value_set action(value_set&& accepted) override {
    auto value = value2typed<T>(std::move(accepted[0]));
    auto search = this->find_matching(value);
    value->source = this->shared_from_this();

    if (search == std::end(this->requests_)) {
      QdCreate(1);
      this->buffer_.emplace_back(std::move(value));
    } else {
      search->second->send(std::move(value));
      this->requests_.erase(search);
    }

    return {};
  }

 protected:
  using buffer_iterator = typename decltype(buffer_)::iterator;

  inline buffer_iterator find_in_buffer(const predicate_type& pred) {
    buffer_iterator search = std::begin(this->buffer_);
    for (; search != std::end(this->buffer_); search++) {
      if (!pred || pred->action((*search)->value())) {
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
      if (!search->first || search->first->action(value)) {
        break;
      }
    }
    return search;
  }
};

}

#endif
