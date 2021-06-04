#include <hypercomm/components.hpp>
#include <algorithm>

namespace hypercomm {

bool component::keep_alive(void) const { return false; }

bool component::permissive(void) const { return false; }

bool component::collectible(void) const {
  return !this->alive &&
         std::all_of(
             std::begin(this->outgoing), std::end(this->outgoing),
             [](const typename decltype(this->outgoing)::value_type& value) {
               return value.second.empty();
             });
}

using incoming_type = component::incoming_type;

inline incoming_type::reverse_iterator find_ready(incoming_type& incoming,
                                                  const int& n_needed) {
  auto search = incoming.rbegin();
  for (; search != incoming.rend(); search++) {
    if (n_needed == search->size()) {
      return search;
    }
  }
  return search;
}

void component::activate(void) {
  this->alive = true;
  auto n_expt = this->n_expected();
  if (n_expt == 0) {
    this->stage_action(nullptr);
  } else {
    auto search = find_ready(this->incoming, n_expt);
    if (search != this->incoming.rend()) {
      this->stage_action(&search);
    }
  }
}

inline incoming_type::reverse_iterator find_gap(
    incoming_type& incoming, const component::port_type& which) {
  auto search = incoming.rbegin();
  for (; search != incoming.rend(); search++) {
    if (search->find(which) == search->end()) {
      return search;
    }
  }
  return search;
}

void component::stage_action(incoming_type::reverse_iterator* search) {
  if (this->alive) {
    auto values = search ? this->action(std::move(**search)) : this->action({});
    if (search) this->incoming.erase(search->base());
    this->unspool_values(std::move(values));
    this->alive = this->keep_alive();
  }
}

void component::unspool_values(value_set& pairs) {
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

  if (!value && !this->permissive()) {
    this->alive = this->keep_alive();
  } else {
    auto search = find_gap(this->incoming, port);

    if (search == this->incoming.rend()) {
      this->incoming.push_front({std::make_pair(port, std::move(value))});
      search = this->incoming.rbegin();
    } else {
      (*search)[port] = value;
    }

    if (search->size() == this->n_expected()) {
      this->stage_action(&search);
    }
  }
}
}
