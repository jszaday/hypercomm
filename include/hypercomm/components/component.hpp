#ifndef __HYPERCOMM_COMPONENTS_COMPONENT_HPP__
#define __HYPERCOMM_COMPONENTS_COMPONENT_HPP__

#include "../core.hpp"

#include "identifiers.hpp"

namespace hypercomm {

class component : virtual public impermanent {
 protected:
  struct listener_;
  std::list<listener_> listeners_;

 public:
  friend class generic_locality_;

  using id_t = component_id_t;
  using value_type = value_ptr;
  using port_type = components::port_id_t;
  using value_set = std::map<port_type, value_type>;
  using incoming_type = std::deque<value_set>;

  enum status { kCompletion, kInvalidation };

  using listener_type = typename decltype(listeners_)::iterator;
  using status_listener_fn = void (*)(const component*, status, void*);

  const id_t id;
  bool activated;

  component(const id_t& _1) : id(_1), activated(false) {}

  virtual ~component() {
    // dumps all held values and propagates invalidations downstream
    this->ret_inv_values();
#if CMK_VERBOSE
    // we have values but nowhere to send 'em
    auto n_outgoing = this->outgoing.size();
    if (n_outgoing > 0) {
      CkError("warning> com%lu destroyed with %lu unsent message(s).\n",
              this->id, n_outgoing);
    }
#endif
  }

  // determeines whether the component should stay
  // "alive" after its acted
  virtual bool keep_alive(void) const override;

  // determines how components respond to invalidations
  // => true -- tolerates invalidations
  // => false -- invalidations are destructive
  // (note: it is false by default)
  virtual bool permissive(void) const;

  // used by the RTS to determine when to GC a component
  // (typically, after all staged values have been sent)
  virtual bool collectible(void) const;

  // the component's number of input ports
  virtual std::size_t n_inputs(void) const = 0;

  // the component's number of output ports
  virtual std::size_t n_outputs(void) const = 0;

  // number of expected values (typically #inputs)
  virtual std::size_t n_expected(void) const { return this->n_inputs(); }

  // action called when a value set is ready
  virtual value_set action(value_set&& values) = 0;

  // used to send a value to a component
  // null values are treated as invalidations
  void receive_value(const port_type& port, value_type&& value);

  // updates the cb that a port's value is sent to
  void update_destination(const port_type& port, const callback_ptr& cb);

  // called when a component is first "activated" in the RTS
  void activate(void);

  // subscribes a status listener
  template <typename... Args>
  inline listener_type add_listener(Args... args) {
    this->listeners_.emplace_front(std::forward<Args>(args)...);
    return std::begin(this->listeners_);
  }

  inline void remove_listener(const listener_type& it) {
    if (it != std::end(this->listeners_)) {
      this->listeners_.erase(it);
    }
  }

  // sends invalidation or completion notifications
  template <status Status>
  inline void notify_listeners(void) {
#if CMK_VERBOSE
    CkPrintf("com%lu> notifying %lu listener(s) of status change to %s.\n",
             this->id, this->listeners_.size(),
             (Status == kInvalidation) ? "invalid" : "complete");
#endif
    while (!this->listeners_.empty()) {
      auto& l = this->listeners_.back();
      l(this, Status);
      this->listeners_.pop_back();
    }
  }

  template <typename... Args>
  // create a value set and initialize it a variable number of port/value pairs
  inline static value_set make_varset(Args&&... args) {
    using value_type = typename value_set::value_type;
    value_set set;
    auto insert = [&](value_type&& value) -> void {
      set.insert(std::move(value));
    };
    using expander = int[];
    (void)expander{0, (void(insert(std::move(args))), 0)...};
    return std::move(set);
  }

  // create a value set and initialize it with the port/value pair
  inline static value_set make_set(const port_type& port, value_type&& value) {
    value_set set;
    set.emplace(port, std::move(value));
    return std::move(set);
  }

 protected:
  struct listener_ {
    using fn_t = status_listener_fn;
    using deleter_t = void (*)(void*);

    fn_t fn;
    void* arg;
    deleter_t deleter;

    listener_(listener_&&) = delete;
    listener_(const listener_&) = delete;
    listener_(const fn_t& fn_, void* arg_ = nullptr,
              const deleter_t& deleter_ = nullptr)
        : fn(fn_), arg(arg_), deleter(deleter_) {}

    ~listener_() {
      if (this->arg) this->deleter(this->arg);
    }

    inline void operator()(const component* self, status nu) {
      fn(self, nu, this->arg);
      this->arg = nullptr;
    }
  };

  // staging area for incomplete value sets
  incoming_type incoming;
  // buffer of yet-unsent values
  std::map<port_type, std::deque<value_type>> outgoing;
  // buffer of unfulfilled callbacks
  std::map<port_type, std::deque<callback_ptr>> routes;

 private:
  void stage_action(incoming_type::iterator*);

  void on_invalidation(void);

  void unspool_values(value_set&);

  void ret_inv_values(void);
};
}  // namespace hypercomm

#endif
