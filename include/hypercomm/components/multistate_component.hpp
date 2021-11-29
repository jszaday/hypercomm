#ifndef __HYPERCOMM_COMPONENTS_MULTISTATE_COMPONENT_HPP__
#define __HYPERCOMM_COMPONENTS_MULTISTATE_COMPONENT_HPP__

#include "component.hpp"
#include "microstack.hpp"
#include "state_server.hpp"

namespace hypercomm {

template <typename T>
struct multistate_inbox_ {
  using inbox_t = std::map<std::size_t, T>;
  using iterator = typename inbox_t::iterator;

  inbox_t inbox_;

  void empty_buffers(void) {
    auto it = std::begin(this->inbox_);
    while (it != std::end(this->inbox_)) {
      components::inbox_<T>::empty_buffer(it->second);
      it = this->inbox_.erase(it);
    }
  }

  inline iterator find_ready(void) {
    for (auto it = this->begin(); it != this->end(); it++) {
      if (components::inbox_<T>::is_ready(it->second)) {
        return it;
      }
    }

    return this->end();
  }

  template <typename... Args>
  inline iterator emplace(Args&&... args) {
    auto pair = this->inbox_.emplace(std::forward<Args>(args)...);
    CkAssertMsg(pair.second, "insertion did not occur!");
    return pair.first;
  }

  template <std::size_t I>
  inline iterator find_gap(std::size_t tag) {
    auto search = this->inbox_.find(tag);
    CkAssert((search == this->end()) || !std::get<I>(search->second));
    return search;
  }

  inline void erase(const iterator& it) { this->inbox_.erase(it); }

  inline iterator begin(void) { return std::begin(this->inbox_); }

  inline iterator end(void) { return std::end(this->inbox_); }
};

template <typename Inputs, typename Outputs>
struct multistate_acceptor_;

template <typename Inputs, typename Outputs>
class multistate_component
    : public components::component<Inputs, Outputs, multistate_acceptor_> {
  using parent_t = components::component<Inputs, Outputs, multistate_acceptor_>;
  using acceptor_type = typename parent_t::acceptor_type;

 protected:
  using state_t = microstack;
  using server_t = state_server<state_t>;
  using in_set = typename parent_t::in_set;

  multistate_component(std::size_t id) : parent_t(id) {}

  ~multistate_component() {
    if (this->shared_) {
      this->shared_->erase_subscriber(this->id);
    }
  }

  const utilities::unshared_ptr<state_t>& get_state(void) {
    return (this->state_).second;
  }

 public:
  friend acceptor_type;

  void set_server(const std::shared_ptr<server_t>& server) {
    this->shared_ = server;
    this->shared_->emplace_subscriber(this->id);
  }

 private:
  std::shared_ptr<server_t> shared_;
  typename server_t::state_type state_;
};

template <typename Inputs, typename Outputs>
class multistate_acceptor_ {
  using component_type =
      components::component<Inputs, Outputs, multistate_acceptor_>;
  using multistate_component_type = multistate_component<Inputs, Outputs>;
  using in_set = typename component_type::in_set;

  template <std::size_t I>
  using in_elt_t = typename component_type::template in_elt_t<I>;

  static constexpr auto n_inputs_ = component_type::n_inputs_;

  static void update_server_(multistate_component_type* self) {
    // we persist until the server is done!
    self->persistent = !self->shared_->done();
    self->shared_->release_state(std::move(self->state_));
  }

  template <std::size_t I>
  inline static typename std::enable_if<(I == 0) && (n_inputs_ == 1)>::type
  direct_stage(multistate_component_type* self, in_elt_t<I>&& val) {
    auto& tag = self->state_.first;
    in_set set(std::move(val));
    if (!self->stage_action(set, [&](void) { update_server_(self); })) {
      // this should be consistent with "find_ready"
      // and the opposite of "find_gap"
      self->incoming_.emplace(tag, std::move(set));
    }
  }

  template <std::size_t I>
  inline static typename std::enable_if<(I >= 0) && (n_inputs_ > 1)>::type
  direct_stage(multistate_component_type* self, in_elt_t<I>&& val) {
    CkAbort("-- unreachable --");
  }

 public:
  using incoming_type = multistate_inbox_<in_set>;

  // TODO add support for invalidations?
  template <std::size_t I>
  static bool accept(components::base_* base, component_port_t port,
                     deliverable& dev) {
    auto tag = port / n_inputs_;
    auto* self = static_cast<multistate_component_type*>(base);
    auto sseek = self->shared_->find_state(tag);
    if (self->shared_->valid_state(sseek)) {
      auto val = dev_conv_<in_elt_t<I>>::convert(std::move(dev));

      if (!(val || self->permissive)) {
        CkError("warning> multi-stage component invalidation unsupported\n");
      } else if (n_inputs_ == 1) {
        // capture the state
        self->state_ = self->shared_->acquire_state(self->id, sseek);
        direct_stage<I>(self, std::move(val));
      } else {
        auto iseek = self->incoming_.template find_gap<I>(port);
        auto created = iseek == self->incoming_.end();
        if (created) {
          iseek = self->incoming_.emplace(port, in_set());
        }
        std::get<I>(iseek->second) = std::move(val);
        if (!created && components::inbox_<in_set>::is_ready(iseek->second)) {
          self->state_ = self->shared_->acquire_state(self->id, sseek);
          stage_action(self, iseek);
        }
      }

      return true;
    } else {
      return false;
    }
  }

  inline static void stage_action(
      component_type* base, const typename incoming_type::iterator& search) {
    base->stage_action(search->second, [&](void) {
      auto* self = static_cast<multistate_component_type*>(base);
      self->incoming_.erase(search);
      update_server_(self);
    });
  }
};
}  // namespace hypercomm

#endif
