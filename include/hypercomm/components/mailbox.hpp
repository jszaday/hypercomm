#ifndef __HYPERCOMM_COMPONENTS_MAILBOX_HPP__
#define __HYPERCOMM_COMPONENTS_MAILBOX_HPP__

#include "../core/typed_value.hpp"
#include "../core/common.hpp"

namespace hypercomm {

template <typename T>
class mailbox : public component {
 public:
  using predicate_type = std::shared_ptr<immediate_action<bool(const T&)>>;
  using action_type = callback_ptr;

  class request : public component::status_listener,
                  public std::enable_shared_from_this<request> {
   public:
    std::shared_ptr<component> com;
    predicate_type pred;
    action_type act;
    mailbox<T>* self;

    request(mailbox<T>* _1, const predicate_type& _2, const callback_ptr& _3)
        : self(_1), pred(_2), act(_3) {}

    ~request() {}

    virtual void on_completion(const component&) override {
      // TODO this may not be necessary?
      self->pop_request(this->shared_from_this());
    }

    virtual void on_invalidation(const component&) override {
      self->pop_request(this->shared_from_this());
    }

    inline bool matches(const T& t) { return !pred || pred->action(t); }

    inline void action(value_type&& value) { act->send(std::move(value)); }
  };

  using request_type = std::shared_ptr<request>;

 protected:
  std::deque<request_type> requests_;
  std::deque<std::shared_ptr<typed_value<T>>> buffer_;

 public:
  mailbox(const id_t& _1) : component(_1) {}

  virtual std::size_t n_inputs(void) const override { return 1; }
  virtual std::size_t n_outputs(void) const override { return 0; }
  virtual bool keep_alive(void) const override { return true; }

  inline request_type put_request(const predicate_type& pred, const callback_ptr& cb) {
    auto search = this->find_in_buffer(pred);
    if (search == std::end(this->buffer_)) {
      auto req = std::make_shared<request>(this, pred, cb);
      this->requests_.emplace_back(req);
      return req;
    } else {
      QdProcess(1);
      cb->send(std::move(*search));
      this->buffer_.erase(search);
      return {};
    }
  }

  inline void put_request_to(const predicate_type& pred, const component_id_t& com, const component::port_type& port) {
    auto cb = local_connector_(com, port);
    auto req = this->put_request(pred, cb);
    if (req) {
      (access_context_()->components[com])->add_listener(req);
    }
  }

  inline void pop_request(const request_type& req) {
    auto end = std::end(this->requests_);
    auto search = std::find(std::begin(this->requests_), end, req);
    if (search != end) {
      this->requests_.erase(search);
    }
  }

  virtual value_set action(value_set&& accepted) override {
    auto value = value2typed<T>(std::move(accepted[0]));
    auto search = this->find_matching(value);

    if (search == std::end(this->requests_)) {
      QdCreate(1);
      this->buffer_.emplace_back(std::move(value));
    } else {
      auto req = *search;
      this->requests_.erase(search);
      // delete req before sending cb to preclude
      // feedback loops
      req->action(std::move(value));
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

  inline request_iterator find_matching(
      const std::shared_ptr<typed_value<T>>& _1) {
    const auto& value = _1->value();
    request_iterator search = std::begin(this->requests_);
    for (; search != std::end(this->requests_); search++) {
      if ((*search)->matches(value)) {
        break;
      }
    }
    return search;
  }
};
}

#endif
