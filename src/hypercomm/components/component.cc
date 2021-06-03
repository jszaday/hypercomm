#include <hypercomm/components/component.hpp>
#include <algorithm>

namespace hypercomm {

bool component::keep_alive(void) const {
  return false;
}

bool component::permissive(void) const {
  return false;
}

bool component::collectible(void) const {
  return !this->alive &&
         std::all_of(
             std::begin(this->outgoing), std::end(this->outgoing),
             [](const typename decltype(this->outgoing)::value_type& value) {
               return value.second.empty();
             });
}

void component::activate(void) {
  this->alive = true;

  if (this->n_inputs() == 0) {
    this->stage_action({});
  }
}

using incoming_type = std::deque<component::value_set>;

inline incoming_type::iterator find_without(incoming_type& incoming,
                                            const component::port_type& which) {
  auto search = incoming.begin();
  for (; search != incoming.end(); search++) {
    if (search->find(which) == search->end()) {
      return search;
    }
  }
  return search;
}

void component::stage_action(value_set&& values) {
  if (this->alive) {
    this->unspool_values(this->action(std::move(values)));

    this->alive = this->keep_alive();
  } else {
    // buffer values in precond
    CkAbort("not yet implemented");
  }
}

void component::unspool_values(value_set&& pairs) {
  CkAssert(pairs.size() == this->n_outputs() && "invalid nbr of outputs");
  for (auto& pair : pairs) {
    auto& port = pair.first;
    CkAssert(port < this->n_outputs() && "output out of range");
    auto& clbk = this->routes[port];
    if (clbk.empty()) {
      this->outgoing[port].push_back(std::move(pair.second));
    } else {
      clbk.front()->send(std::move(pair.second));
      clbk.pop_front();
    }
  }
}

void component::update_destination(const port_type& port,
                                   const callback_ptr& cb) {
  auto& values = this->outgoing[port];
  if (values.empty()) {
    this->routes[port].push_back(cb);
  } else {
    cb->send(std::move(values.front()));
    values.pop_front();
  }
}

void component::receive_value(const port_type& port, value_type&& value) {
  CkAssert(port < this->n_inputs() && "port must be within range");

  auto search = find_without(this->incoming, port);

  if (!value && !this->permissive()) {
    // NOTE this is probably unnecessary
    // if (search != this->incoming.end()) {
    //   this->incoming.erase(search);
    // }

    this->alive = this->keep_alive();
  } else {
    if (search == this->incoming.end()) {
      search = this->incoming.insert(search,
                                     {std::make_pair(port, std::move(value))});
    } else {
      (*search)[port] = value;
    }

    if (search->size() == this->n_inputs()) {
      this->stage_action(std::move(*search));
      this->incoming.erase(search);
    }
  }
}
}
