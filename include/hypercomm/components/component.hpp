#ifndef __HYPERCOMM_COMPONENTS_COMPONENT_HPP__
#define __HYPERCOMM_COMPONENTS_COMPONENT_HPP__

#include "identifiers.hpp"
#include "../core.hpp"

namespace hypercomm {

class component : virtual public impermanent {
 public:
  using id_t = component_id_t;
  using value_type = core::value_ptr;
  using port_type = components::port_id_t;
  using value_set = std::map<port_type, value_type>;
  using incoming_type = std::deque<value_set>;

  const id_t id;

  component(const id_t& _1): id(_1) {}

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
  virtual std::size_t n_expected(void) const {
    return this->n_inputs();
  }

  // action called when a value set is ready
  virtual value_set action(value_set&& values) = 0;

  // used to send a value to a component
  // null values are treated as invalidations
  void receive_value(const port_type& port, value_type&& value);

  // updates the cb that a port's value is sent to
  void update_destination(const port_type& port, const callback_ptr& cb);

  // called when a component is first "activated" in the RTS
  void activate(void);

 protected:
  // staging area for incomplete value sets
  incoming_type incoming;
  // buffer of yet-unsent values
  std::map<port_type, std::deque<value_type>> outgoing;
  // buffer of unfulfilled callbacks
  std::map<port_type, std::deque<callback_ptr>> routes;

 private:
  void stage_action(incoming_type::reverse_iterator*);

  void unspool_values(value_set&&);
};

using component_ptr = std::shared_ptr<component>;
}

#endif
