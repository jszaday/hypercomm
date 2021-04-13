#include "component.hpp"
#include <map>
#include <deque>

#ifndef CMK_STACKSIZE_DEFAULT
#define CMK_STACKSIZE_DEFAULT 32768
#endif

struct manager {
  manager(void) = default;

  using component_t = std::shared_ptr<component>;
  using buffer_t = std::deque<std::pair<component::id_t, component::value_t>>;

  std::map<component::id_t, component_t> components;
  std::map<component::id_t, buffer_t> buffers;

  void recv_value(const component::id_t& from, const component::id_t& to,
                  component::value_t&& msg) {
    auto search = this->components.find(to);
    if (search == this->components.end()) {
      this->buffers[to].emplace_back(std::make_pair(from, std::move(msg)));
    } else {
      auto& ptr = search->second;
      if (ptr->alive) {
        ptr->accept(from, std::move(msg));
        this->try_action(ptr);
      } else {
        ptr->erase_incoming(from);
        this->try_collect(ptr);
      }
    }
  }

  void recv_invalidation(const component::id_t& from,
                         const component::id_t& to) {
    auto& ptr = this->components[to];
    ptr->invalidation(from);
    this->try_action(ptr);
  }

  void emplace(component_t&& which) {
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

  void try_action(const component::id_t& which) {
    this->try_action(this->components[which]);
  }

  void try_action(component_t& ptr) {
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

  void try_collect(component::id_t& which) {
    this->try_collect(this->components[which]);
  }

  void try_collect(component_t& ptr) {
    if (ptr && ptr->collectible()) {
      const auto& id = ptr->id;
      if (ptr.use_count() != 1) {
        CkPrintf("%lu> warning: component replicated\n", id);
      }
      this->components.erase(id);
      QdProcess(1);
    }
  }

  static CthThread launch_action(component* ptr) {
    return CthCreate((CthVoidFn)trigger_action, ptr, 0);
  }

  static void trigger_action(component*);
};

using manager_ptr_t = std::unique_ptr<manager>;
CkpvDeclare(manager_ptr_t, manager_);
CkpvDeclare(uint32_t, counter_);

void initialize(void) {
  if (!CkpvInitialized(counter_)) {
    CkpvInitialize(uint32_t, counter_);
  }

  if (!CkpvInitialized(manager_)) {
    CkpvInitialize(manager_ptr_t, manager_);
  }

  CkpvAccess(counter_) = 0;
  (CkpvAccess(manager_)).reset(new manager());
}

void set_identity(component::id_t& id) {
  auto pe = ((component::id_t)CkMyPe()) << ((8 * sizeof(component::id_t)) / 2);
  id = pe | (++CkpvAccess(counter_));
}

int get_home(const component::id_t& id) {
  return (int)(id >> ((8 * sizeof(component::id_t)) / 2));
}

void emplace_component(std::shared_ptr<component>&& which) {
  CkpvAccess(manager_)->emplace(std::move(which));
}

void component::send_value(const id_t& from, const id_t& to,
                           component::value_t&& msg) {
  auto home = get_home(to);
  if (home == CkMyPe()) {
    (CkpvAccess(manager_))->recv_value(from, to, std::move(msg));
  } else {
    CkAbort("remote sends unavailable");
  }
}

void component::send_invalidation(const id_t& from, const id_t& to) {
  auto home = get_home(to);
  if (home == CkMyPe()) {
    (CkpvAccess(manager_))->recv_invalidation(from, to);
  } else {
    CkAbort("remote invalidations unavailable");
  }
}

void manager::trigger_action(component* ptr) {
  auto msg = ptr->action();
  ptr->alive = false;
  ptr->send(std::move(msg));
  CkpvAccess(manager_)->try_collect(ptr->id);
}
