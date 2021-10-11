#ifndef __HYPERCOMM_COMPONENTS_MAILBOX_HPP__
#define __HYPERCOMM_COMPONENTS_MAILBOX_HPP__

#include "../core/typed_value.hpp"
#include "../utilities/weak_ref.hpp"
#include "../core/generic_locality.hpp"

namespace hypercomm {

template <typename T>
class mailbox : public component {
 public:
  using predicate_type = std::shared_ptr<immediate_action<bool(const T&)>>;
  using action_type = callback_ptr;
  using weak_ref_t = utilities::weak_ref<mailbox>;

  class request {
   public:
    predicate_type pred;
    action_type act;

    component::id_t com;
    component::listener_type listener;

    request(const predicate_type& _1, callback_ptr&& _2)
        : pred(_1), act(_2), com(0) {}

    request(const predicate_type& _1, const callback_ptr& _2)
        : pred(_1), act(_2), com(0) {}

    inline bool matches(const T& t) { return !pred || pred->action(t); }

    inline void action(value_type&& value) { act->send(std::move(value)); }
  };

 protected:
  std::deque<std::unique_ptr<typed_value<T>>> buffer_;
  std::list<request> requests_;
  std::shared_ptr<weak_ref_t> weak_;

  using reqiter_t = typename decltype(requests_)::iterator;

 public:
  mailbox(const id_t& _1) : component(_1), weak_(new weak_ref_t(this)) {}

  ~mailbox() { weak_->reset(nullptr); }

  virtual std::size_t n_inputs(void) const override { return 1; }
  virtual std::size_t n_outputs(void) const override { return 0; }
  virtual bool keep_alive(void) const override { return true; }

  inline reqiter_t put_request(const predicate_type& pred,
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

  inline void put_request_to(const predicate_type& pred,
                             const component_id_t& com,
                             const component::port_type& port) {
    auto* ctx = access_context_();
    auto search = this->find_in_buffer(pred);
    if (search == std::end(this->buffer_)) {
      this->requests_.emplace_front(pred, ctx->make_connector(com, port));
      auto req = this->requests_.begin();
      req->com = com;
      req->listener =
          (ctx->components[com])
              ->add_listener(&on_status_change,
                             new listener_type(this->weak_, req),
                             [](void* value) { delete (listener_type*)value; });
    } else {
      QdProcess(1);
      ctx->components[com]->receive_value(port, std::move(*search));
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
      auto req = std::move(*search);
      // cleanup the request before triggering
      // its action (from our list, and com's!)
      this->requests_.erase(search);
      if (req.com != 0) {
        (access_context_()->components[req.com])->remove_listener(req.listener);
      }
      // this precludes feedback loops
      req.action(std::move(value));
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

  inline reqiter_t find_matching(const std::unique_ptr<typed_value<T>>& _1) {
    auto& value = _1->value();
    for (auto it = (this->requests_).rbegin(); it != (this->requests_).rend();
         it++) {
      if (it->matches(value)) {
        return --(it.base());
      }
    }
    return std::end(this->requests_);
  }

 private:
  using listener_type = std::pair<std::shared_ptr<weak_ref_t>, reqiter_t>;

  static void on_status_change(const component*, component::status status,
                               void* arg) {
    auto* listener = (listener_type*)arg;
    auto& self = *(listener->first);
    if (self) {
      auto& req = listener->second;
      if (req != std::end(self->requests_)) {
        self->requests_.erase(req);
      }
    }
    delete listener;
  }
};
}  // namespace hypercomm

#endif
