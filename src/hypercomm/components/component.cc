#include <hypercomm/utilities.hpp>
#include <hypercomm/components.hpp>

namespace hypercomm {
namespace components {

// CkpvExtern(int, counter_);
// CkpvExtern(int, value_handler_idx_);
// CkpvExtern(int, invalidation_handler_idx_);
// CkpvExtern(int, connection_handler_idx_);

// void component::activate(std::shared_ptr<component>&& ptr) {
//   (manager::local())->emplace(std::move(ptr));
// }

// void component::generate_identity(component::id_t& id) {
//   auto pe = ((component::id_t)CkMyPe()) << ((8 * sizeof(component::id_t)) /
//   2);
//   id = pe | (++CkpvAccess(counter_));
// }

void component::receive_value(const port_id_t& from, value_t&& msg) {
  // TODO unsure whether this should be enforced
  // CkAssert(this->alive && "only living components can accept values");
  QdCreate(1);

  auto search = std::find(this->incoming.begin(), this->incoming.end(), from);
  if (search == this->incoming.end()) {
    this->inbox.emplace(from, std::move(msg));
  } else {
    this->accepted.emplace_back(std::move(msg));
    this->incoming.erase(search);
  }

  this->resync_status();
}

int component::num_expected(void) const { return this->num_available(); }

int component::num_available() const {
  return (this->incoming.size() + this->accepted.size());
}

bool component::ready(void) const {
  return this->alive && (this->accepted.size() == this->num_expected());
}

bool component::collectible(void) const {
  return !this->alive && this->incoming.empty() && this->outgoing.empty() &&
         this->inbox.empty() && this->outbox.empty();
}

// void component::connection(const id_t& from, const id_t& to) {
//   CkAssert(!this->alive && "cannot connect to a living component");

//   if (this->id == to) {
//     this->incoming.push_back(from);
//   } else if (this->id == from) {
//     this->outgoing.push_back(to);
//   } else {
//     CkAbort("%lu> unable to link %lu to %lu", this->id, from, to);
//   }
// }

void component::receive_invalidation(const port_id_t& from) {
  CkAssert(this->alive && "only living components can invalidate connections");

  auto min = this->num_expected();
  this->erase_incoming(from);
  auto avail = this->num_available();
  this->alive = (avail >= min);

  if (!this->alive) {
    for (const auto& dst : this->outgoing) {
      this->try_send(dst, std::move(value_t{}));
    }

    this->outgoing.clear();
  }
}

void component::erase_incoming(const port_id_t& from) {
  auto search = std::find(this->incoming.begin(), this->incoming.end(), from);
  CkAssert(search != this->incoming.end() && "could not find in-port");
  this->incoming.erase(search);
}

void component::send(value_t&& msg) {
  CkAssert(!this->alive && "a living component cannot send values");

  for (auto it = std::begin(this->outgoing); it != std::end(this->outgoing);
       it = std::next(it)) {
    value_t&& ready{};
    if (it == std::prev(std::end(this->outgoing))) {
      ready = std::move(msg);
    } else {
      ready = std::move(utilities::copy_message(msg));
    }

    this->try_send(*it, std::forward<value_t>(ready));
  }

  this->outgoing.clear();
}
}
}
