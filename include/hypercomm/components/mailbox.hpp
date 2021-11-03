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

    component_id_t com;
    components::base_::listener_type listener;

    template <typename... Args>
    request(const predicate_type& pred_, Args&&... args)
        : pred(pred_), dst(std::forward<Args>(args)...), com(0) {}

    inline bool matches(const value_type& t) {
      return !pred || pred->action(t);
    }

    inline void action(value_type&& value) {
      passthru_context_(dst, std::move(value));
    }
  };

 protected:
  std::list<request> requests_;
  std::deque<value_type> buffer_;
  std::shared_ptr<weak_ref_t> weak_;

  using reqiter_t = typename decltype(requests_)::iterator;

 public:
  mailbox(const id_t& _1) : parent_t(_1), weak_(new weak_ref_t(this)) {
    this->persistent = true;
  }

  ~mailbox() { weak_->reset(nullptr); }

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
                             const component_port_t& port) {
    auto search = this->find_in_buffer(pred);
    if (search == std::end(this->buffer_)) {
      this->requests_.emplace_front(pred, com, port);
    } else {
      QdProcess(1);
      passthru_context_(std::make_pair(com, port), std::move(*search));
      this->buffer_.erase(search);
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
      // the action, precluding infinite loops
      this->requests_.erase(search);
      try {
        req.action(std::move(value));
      } catch (bad_destination& ex) {
        // try redelivering until we find a valid destination
        ((components::base_*)this)->accept(0, std::move(ex.dev));
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

  using request_iterator = typename decltype(requests_)::iterator;

  inline reqiter_t find_matching(const value_type& value) {
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

  static void on_status_change(const components::base_*,
                               components::status_ status, void* arg) {
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
