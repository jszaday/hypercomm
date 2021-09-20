#include <hypercomm/components.hpp>
#include <algorithm>

namespace hypercomm {

bool component::keep_alive(void) const { return false; }

bool component::permissive(void) const { return false; }

bool component::collectible(void) const {
  return this->activated && !this->alive &&
         std::all_of(
             std::begin(this->outgoing), std::end(this->outgoing),
             [](const typename decltype(this->outgoing)::value_type& value) {
               return value.second.empty();
             });
}

using incoming_type = component::incoming_type;

// find any records that are ready, i.e., they have all the expected values
inline incoming_type::iterator find_ready(incoming_type& incoming,
                                          const int& n_needed) {
  auto search = incoming.rbegin();
  for (; search != incoming.rend(); search++) {
    if (n_needed == search->size()) {
      return search.base() - 1;
    }
  }
  return std::end(incoming);
}

// activates a component, enabling it to execute
void component::activate(void) {
  // set the "activated" and "alive" flags
  // (a component is gc'd when activated=true, alive=false)
  this->activated = this->alive = true;
  // check the number of expected values
  auto n_expt = this->n_expected();
  if (n_expt == 0) {
    // if none, we can stage the action of the component
    this->stage_action(nullptr);
  } else {
    // otherwise, stage all ready values (while we are alive)
    while (this->alive) {
      auto search = find_ready(this->incoming, n_expt);
      if (search == std::end(this->incoming)) {
        break;
      } else {
        this->stage_action(&search);
      }
    }
  }
}

// find a value set that has a "gap", or missing value,
// at the specified port
inline incoming_type::iterator find_gap(incoming_type& incoming,
                                        const component::port_type& which) {
  if (!incoming.empty()) {
    auto search = incoming.rbegin();
    for (; search != incoming.rend(); search++) {
      if (search->find(which) == search->end()) {
        return search.base() - 1;
      }
    }
  }
  return std::end(incoming);
}

// this should only be called with a ready value set,
// this may be "nullptr" when components require no values.
void component::stage_action(incoming_type::iterator* search) {
  // when a component is alive:
  if (this->alive) {
    // use whichever values are available
    auto values = search ? this->action(std::move(**search)) : this->action({});
    // erase values from the list to consume them
    if (search) {
      CkAssert(*search != std::end(this->incoming));
      this->incoming.erase(*search);
    }
    // send the results downstream
    this->unspool_values(values);
    // determine whether we persist after acting
    this->alive = this->keep_alive();
    // then notify our listeners if we expired
    if (!this->alive) this->notify_listeners<false>();
  }
}

// propagate a value set downstream to our consumers
void component::unspool_values(value_set& pairs) {
  CkAssertMsg(pairs.size() == this->n_outputs(), "invalid nbr of outputs");
  // for each (port, value) pair the value set
  for (auto& pair : pairs) {
    auto& port = pair.first;
    CkAssertMsg(port < this->n_outputs(), "output out of range");
    // check whether we have callback(s) at the specified port
    auto& clbk = this->routes[port];
    if (clbk.empty()) {
      // if not, buffer the value until a callback is set
      this->outgoing[port].push_back(std::move(pair.second));
    } else {
      // otherwise, send the value to the callback, and pop it
      // (so it doesn't receive multiple values from the same port)
      clbk.front()->send(std::move(pair.second));
      clbk.pop_front();
    }
  }
}

void component::update_destination(const port_type& port,
                                   const callback_ptr& cb) {
  auto search = this->outgoing.find(port);
  // if no values are available at this port
  if ((search == std::end(this->outgoing)) || search->second.empty()) {
    // add the callback to the list of outgoing routes
    this->routes[port].push_back(cb);
  } else {
    auto& values = search->second;
    // if a value is found, send it
    cb->send(std::move(values.front()));
    // then discard it
    values.pop_front();
  }
}

void component::ret_inv_values(void) {
  // try to return all unused values and
  for (auto& set : this->incoming) {
    for (auto& pair : set) {
      try_return(std::move(pair.second));
    }
  }
  // clear values so they're not used again
  this->incoming.clear();
  // invalidate all unfulfilled routes
  for (auto& pair : this->routes) {
    for (auto& route : pair.second) {
      route->send(value_type{});
    }
  }
  // clear routes so they're not used again
  this->routes.clear();
}

// when a component expires, it:
void component::on_invalidation(void) {
  // dumps its values and propagates invalidations downstream:
  this->ret_inv_values();
  // notifies its listeners
  this->notify_listeners<true>();
}

void component::receive_value(const port_type& port, value_type&& value) {
  CkAssertMsg(port < this->n_inputs(), "port must be within range");
  // if a component receives an unpermitted invalidation
  if (!value && !this->permissive()) {
    // it expires when it is not resilient, and:
    this->alive = this->alive && this->keep_alive();
    // if it expires, it goes through a routine:
    if (!this->alive) this->on_invalidation();
  } else {
    // look for a value set missing a value for this port
    auto search = find_gap(this->incoming, port);
    // if one is not found:
    if (search == std::end(this->incoming)) {
      // create a new set, and put it at the head of the buffer
      this->incoming.emplace_front(make_set(port, std::move(value)));
      // then, update the search iterator
      search = std::begin(this->incoming);
    } else {
      // otherwise, update the found value set
      auto ins = (*search).emplace(port, std::move(value));
      CkAssertMsg(ins.second, "insertion did not occur!");
    }
    // if the value set at the iterator is ready
    if (search->size() == this->n_expected()) {
      // then stage its action
      this->stage_action(&search);
    }
  }
}
}  // namespace hypercomm
