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

  void recv_invalidation(const component::id_t& from,
                         const component::id_t& to) {
    component::value_t empty{};
    this->recv_value(from, to, std::move(empty));
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
CkpvDeclare(int, value_handler_idx_);
CkpvDeclare(int, invalidation_handler_idx_);

void value_handler_(envelope*);
void invalidation_handler_(envelope*);

void initialize(void) {
  CkpvInitialize(uint32_t, counter_);
  CkpvAccess(counter_) = 0;
  CkpvInitialize(manager_ptr_t, manager_);
  (CkpvAccess(manager_)).reset(new manager());
  CkpvInitialize(int, value_handler_idx_);
  CkpvAccess(value_handler_idx_) =
      CmiRegisterHandler((CmiHandler)value_handler_);
  CkpvInitialize(int, invalidation_handler_idx_);
  CkpvAccess(invalidation_handler_idx_) =
      CmiRegisterHandler((CmiHandler)invalidation_handler_);
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

std::tuple<component::id_t&, component::id_t&> get_from_to(envelope* env) {
  auto hdr =
      reinterpret_cast<char*>(env) + CmiReservedHeaderSize + sizeof(UInt);
  CkAssert(((2 * sizeof(component::id_t)) <= sizeof(ck::impl::u_type)) &&
           "insufficient header space");
  auto& total_sz = *(reinterpret_cast<UInt*>(hdr + sizeof(ck::impl::u_type)));
  CkAssert((total_sz == env->getTotalsize()) && "did not find total size");
  component::id_t& first = *(reinterpret_cast<component::id_t*>(hdr));
  return std::forward_as_tuple(first, *(&first + 1));
}

void component::send_value(const id_t& from, const id_t& to,
                           component::value_t&& msg) {
  auto home = get_home(to);
  if (home == CkMyPe()) {
    (CkpvAccess(manager_))->recv_value(from, to, std::move(msg));
  } else {
    CkMessage* msg_raw = msg.get();
    CkMessage* msg_ready = nullptr;
    if (msg.use_count() == 1) {
      msg_ready = msg_raw;
      ::new (&msg) component::value_t{};
    } else {
      msg_ready = (CkMessage*)CkCopyMsg((void**)&msg_raw);
    }
    auto env = UsrToEnv(msg_ready);
    auto tup = get_from_to(env);
    std::get<0>(tup) = from;
    std::get<1>(tup) = to;
    util::pack_message(msg_ready);
    CmiSetHandler(env, CkpvAccess(value_handler_idx_));
    CmiSyncSendAndFree(home, env->getTotalsize(), reinterpret_cast<char*>(env));
  }
}

void component::send_invalidation(const id_t& from, const id_t& to) {
  auto home = get_home(to);
  if (home == CkMyPe()) {
    (CkpvAccess(manager_))->recv_invalidation(from, to);
  } else {
    auto msg = CkAllocateMarshallMsg(0);
    auto env = UsrToEnv(msg);
    auto tup = get_from_to(env);
    std::get<0>(tup) = from;
    std::get<1>(tup) = to;
    CmiSetHandler(env, CkpvAccess(invalidation_handler_idx_));
    CmiSyncSendAndFree(home, env->getTotalsize(), reinterpret_cast<char*>(env));
  }
}

void manager::trigger_action(component* ptr) {
  auto msg = ptr->action();
  ptr->alive = false;
  ptr->send(std::move(msg));
  CkpvAccess(manager_)->try_collect(ptr->id);
}

void value_handler_(envelope* env) {
  auto tup = get_from_to(env);
  auto msg = std::shared_ptr<CkMessage>((CkMessage*)EnvToUsr(env),
                                        [](CkMessage* msg) { CkFreeMsg(msg); });
  util::unpack_message(msg.get());
  CkpvAccess(manager_)
      ->recv_value(std::get<0>(tup), std::get<1>(tup), std::move(msg));
}

void invalidation_handler_(envelope* env) {
  auto tup = get_from_to(env);
  CkpvAccess(manager_)->recv_invalidation(std::get<0>(tup), std::get<1>(tup));
  CkFreeMsg(EnvToUsr(env));
}
