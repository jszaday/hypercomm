#include <hypercomm/components.hpp>

#ifndef CMK_STACKSIZE_DEFAULT
#define CMK_STACKSIZE_DEFAULT 32768
#endif

namespace hypercomm {
namespace components {

void manager::recv_value(const component::id_t& from, const component::id_t& to,
                         component::value_t&& msg) {
  auto search = this->components.find(to);
  if (search == this->components.end()) {
    this->buffers[to].emplace_back(std::make_pair(from, std::move(msg)));
  } else {
    auto& ptr = search->second;
    if (ptr->alive) {
      if (msg) {
        ptr->accept(from, std::move(msg));
      } else {
        ptr->invalidation(from);
      }

      this->try_action(ptr);
    } else {
      ptr->erase_incoming(from);
      this->try_collect(ptr);
    }
  }
}

void manager::recv_invalidation(const component::id_t& from,
                                const component::id_t& to) {
  component::value_t empty{};
  this->recv_value(from, to, std::move(empty));
}

void manager::recv_connection(const placeholder& p, const component::id_t& id) {
  auto& ours = this->components[p.source];
  CkAssert(ours && "could not find placeholder issuer!");
  ours->fill_placeholder(p, id);
  this->try_collect(ours);
}

void manager::emplace(component_t&& which) {
  QdCreate(1);
  const auto& id = which->id;
  which->alive = true;
  this->components.emplace(id, std::move(which));
  auto search = this->buffers.find(id);
  if (search == this->buffers.end()) {
    this->try_action(id);
  } else {
    auto& buffer = search->second;
    do {
      auto& value = buffer.front();
      this->recv_value(value.first, id, std::move(value.second));
      buffer.pop_front();
    } while (!buffer.empty());
    this->buffers.erase(search);
  }
}

void manager::try_action(const component::id_t& which) {
  this->try_action(this->components[which]);
}

void manager::try_action(component_t& ptr) {
  if (ptr && ptr->ready()) {
    auto* raw_ptr = ptr.get();

    if (dynamic_cast<threaded_component*>(raw_ptr)) {
      CthResume(launch_action(raw_ptr));
    } else {
      trigger_action(raw_ptr);
    }
  } else {
    this->try_collect(ptr);
  }
}

void manager::try_collect(const component::id_t& which) {
  this->try_collect(this->components[which]);
}

void manager::try_collect(component_t& ptr) {
  if (ptr && ptr->collectible()) {
    const auto& id = ptr->id;
    if (ptr.use_count() != 1) {
      CkPrintf("%lu> warning: component replicated\n", id);
    }
    this->components.erase(id);
    QdProcess(1);
  }
}

CthThread manager::launch_action(component* ptr) {
  return CthCreate((CthVoidFn)trigger_action, ptr, 0);
}

using manager_t = manager::manager_t;

CkpvDeclare(manager_t, manager_);

manager_t& manager::local(void) { return CkpvAccess(manager_); }

void manager::trigger_action(component* ptr) {
  auto msg = ptr->action();
  ptr->alive = false;
  ptr->send(std::move(msg));
  (local())->try_collect(ptr->id);
}
}
}