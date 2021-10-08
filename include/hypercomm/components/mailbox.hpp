#ifndef __HYPERCOMM_COMPONENTS_MAILBOX_HPP__
#define __HYPERCOMM_COMPONENTS_MAILBOX_HPP__

#include "../core/typed_value.hpp"
#include "../core/generic_locality.hpp"

namespace hypercomm {

template <typename T>
class mailbox : public component {
 public:
  using predicate_type = std::shared_ptr<immediate_action<bool(const T&)>>;
  using action_type = callback_ptr;
  using reqcount_t = std::size_t;

  class request {
   public:
    mailbox<T>* self;
    reqcount_t id;
    predicate_type pred;
    action_type act;

    request(mailbox<T>* _1, const reqcount_t& _2, const predicate_type& _3,
            const callback_ptr& _4)
        : self(_1), id(_2), pred(_3), act(_4) {}

    inline bool matches(const T& t) { return !pred || pred->action(t); }

    inline void action(value_type&& value) { act->send(std::move(value)); }
  };

  class comlink : public component::status_listener {
   public:
    mailbox<T>* self;
    reqcount_t id;

    comlink(mailbox<T>* _1, const reqcount_t& _2) : self(_1), id(_2) {}

    virtual void on_completion(const component&) override {
      // TODO this may not be necessary?
      self->pop_request(this->id);
    }

    virtual void on_invalidation(const component&) override {
      self->pop_request(this->id);
    }
  };

 protected:
  std::deque<std::unique_ptr<typed_value<T>>> buffer_;
  std::deque<std::unique_ptr<request>> requests_;
  reqcount_t reqcount;

 public:
  mailbox(const id_t& _1) : component(_1), reqcount(0) {}

  virtual std::size_t n_inputs(void) const override { return 1; }
  virtual std::size_t n_outputs(void) const override { return 0; }
  virtual bool keep_alive(void) const override { return true; }

  inline request* put_request(const predicate_type& pred,
                              const callback_ptr& cb) {
    auto search = this->find_in_buffer(pred);
    if (search == std::end(this->buffer_)) {
      auto req = new request(this, ++this->reqcount, pred, cb);
      this->requests_.emplace_back(req);
      return req;
    } else {
      QdProcess(1);
      cb->send(std::move(*search));
      this->buffer_.erase(search);
      return nullptr;
    }
  }

  inline void put_request_to(const predicate_type& pred,
                             const component_id_t& com,
                             const component::port_type& port) {
    auto cb = access_context_()->make_connector(com, port);
    auto req = this->put_request(pred, cb);
    if (req) {
      (access_context_()->components[com])
          ->add_listener(std::make_shared<comlink>(this, req->id));
    }
  }

  inline void pop_request(const reqcount_t& req) {
    using request_type = typename decltype(this->requests_)::value_type;
    auto end = std::end(this->requests_);
    auto search = std::find_if(
        std::begin(this->requests_), end,
        [&](const request_type& other) -> bool { return req == other->id; });
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
      auto req = std::move(*search);
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
      const std::unique_ptr<typed_value<T>>& _1) {
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
}  // namespace hypercomm

#endif
