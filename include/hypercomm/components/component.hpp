#ifndef __HYPERCOMM_COMPONENTS_COMPONENT_HPP__
#define __HYPERCOMM_COMPONENTS_COMPONENT_HPP__

#include "../core.hpp"

#include "identifiers.hpp"
#include "status_listener.hpp"

namespace hypercomm {

class component : virtual public impermanent {
 public:
  friend class generic_locality_;

  using id_t = component_id_t;
  using value_type = value_ptr;
  using port_type = components::port_id_t;
  using value_set = std::map<port_type, value_type>;
  using incoming_type = std::deque<value_set>;
  using status_listener = components::status_listener;

  using listener_ptr = std::shared_ptr<status_listener>;

  const id_t id;
  bool activated;

  component(const id_t& _1) : id(_1), activated(false) {}

  virtual ~component() {
    // try to return all unused values
    for (auto& set : this->incoming) {
      for (auto& pair : set) {
        try_return(std::move(pair.second));
      }
    }

    // invalidate all unfulfilled routes
    for (auto& pair : this->routes) {
      for (auto& route : pair.second) {
        route->send(value_type{});
      }
    }

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
  inline void add_listener(const listener_ptr& l) {
    this->listeners_.emplace_back(l);
  }

  // unsubscribes a status listener
  inline void remove_listener(const listener_ptr& l) {
    auto end = std::end(this->listeners_);
    auto search = std::find(std::begin(this->listeners_), end, l);
    if (search != end) {
      this->listeners_.erase(search);
    }
  }

  // sends invalidation or completion notifications
  template <bool Invalidation>
  inline void notify_listeners(void) {
#if CMK_VERBOSE
    CkPrintf("com%lu> notifying %lu listener(s) of status change to %s.\n",
             this->id, this->listeners_.size(),
             Invalidation ? "invalid" : "complete");
#endif
    for (const auto& l : this->listeners_) {
      if (Invalidation) {
        l->on_invalidation(*this);
      } else {
        l->on_completion(*this);
      }
    }

    this->listeners_.clear();
  }

 protected:
  std::vector<listener_ptr> listeners_;

  // staging area for incomplete value sets
  incoming_type incoming;
  // buffer of yet-unsent values
  std::map<port_type, std::deque<value_type>> outgoing;
  // buffer of unfulfilled callbacks
  std::map<port_type, std::deque<callback_ptr>> routes;

 private:
  void stage_action(incoming_type::reverse_iterator*);

  void on_invalidation(void);

  void unspool_values(value_set&);
};
}  // namespace hypercomm

#endif
