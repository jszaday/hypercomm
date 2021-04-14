#include <hypercomm/utilities.hpp>
#include <hypercomm/components.hpp>

namespace hypercomm {
namespace components {

CkpvExtern(int, counter_);
CkpvExtern(int, value_handler_idx_);
CkpvExtern(int, invalidation_handler_idx_);
CkpvExtern(int, connection_handler_idx_);

void component::activate(std::shared_ptr<component>&& ptr) {
  (manager::local())->emplace(std::move(ptr));
}

void component::generate_identity(component::id_t& id) {
  auto pe = ((component::id_t)CkMyPe()) << ((8 * sizeof(component::id_t)) / 2);
  id = pe | (++CkpvAccess(counter_));
}

void component::accept(const id_t& from, value_t&& msg) {
  CkAssert(this->alive && "only living components can accept values");
  auto search = std::find(this->incoming.begin(), this->incoming.end(), from);
  if (search == this->incoming.end()) {
    this->inbox.emplace(from, std::move(msg));
  } else {
    this->accepted.emplace_back(std::move(msg));
    this->incoming.erase(search);
  }
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
         this->outbox.empty();
}

void component::connection(const id_t& from, const id_t& to) {
  CkAssert(!this->alive && "cannot connect to a living component");

  if (this->id == to) {
    this->incoming.push_back(from);
  } else if (this->id == from) {
    this->outgoing.push_back(to);
  } else {
    CkAbort("%lu> unable to link %lu to %lu", this->id, from, to);
  }
}

void component::invalidation(const id_t& from) {
  CkAssert(this->alive && "only living components can invalidate connections");

  auto min = this->num_expected();
  this->erase_incoming(from);
  auto avail = this->num_available();
  this->alive = (avail >= min);

  auto curr = this->outgoing.begin();
  while (!this->alive && curr != this->outgoing.end()) {
    const id_t& to = *curr;
    if (is_placeholder(to)) {
      this->outbox[to] = {};
      curr++;
    } else {
      component::send_invalidation(this->id, to);
      curr = this->outgoing.erase(curr);
    }
  }
}

void component::erase_incoming(const id_t& from) {
  auto search = std::find(this->incoming.begin(), this->incoming.end(), from);
  CkAssert(search != this->incoming.end() && "could not find in-port");
  this->incoming.erase(search);
}

void component::send(value_t&& msg) {
  CkAssert(!this->alive && "a living component cannot send values");

  while (!this->outgoing.empty()) {
    const id_t to = this->outgoing.back();
    this->outgoing.pop_back();

    value_t&& ready{};
    if (this->outgoing.empty()) {
      ready = std::move(msg);
    } else {
      ready = std::move(utilities::copy_message(msg));
    }

    if (is_placeholder(to)) {
      this->outbox.emplace(to, std::forward<value_t>(ready));
    } else {
      component::send_value(this->id, to, std::forward<value_t>(ready));
    }
  }
}

int get_home(const component::id_t& id) {
  return (int)(id >> ((8 * sizeof(component::id_t)) / 2));
}

std::tuple<component::id_t&, component::id_t&> get_from_to(envelope* env) {
  auto hdr =
      reinterpret_cast<char*>(env) + CmiReservedHeaderSize + sizeof(UInt);
  CkAssert(((2 * sizeof(component::id_t)) <= sizeof(ck::impl::u_type)) &&
           "insufficient header space");
  // TODO fixme?
  // auto& total_sz = *(reinterpret_cast<UInt*>(hdr +
  // sizeof(ck::impl::u_type)));
  // CkPrintf("pe=%d> env_size=%x, total_size=%x\n", CkMyPe(),
  // env->getTotalsize(), total_sz);
  // CkAssert((total_sz == env->getTotalsize()) && "did not find total size");
  component::id_t& first = *(reinterpret_cast<component::id_t*>(hdr));
  return std::forward_as_tuple(first, *(&first + 1));
}

bool component::is_placeholder(const component::id_t& id) {
  return get_home(id) < 0;
}

void component::connect(const std::shared_ptr<component>& from,
                        const std::shared_ptr<component>& to) {
  to->connection(from->id, to->id);
  from->connection(from->id, to->id);
}

void component::connect(const placeholder& p,
                        const std::shared_ptr<component>& c) {
  if (p.is_input()) {
    c->connection(c->id, p.source);
  } else {
    c->connection(p.source, c->id);
  }

  component::send_connection(p, c->id);
}

void component::send_connection(const placeholder& p,
                                const component::id_t& id) {
  auto home = get_home(p.source);
  if (home == CkMyPe()) {
    (manager::local())->recv_connection(p, id);
  } else {
#if CMK_DEBUG
    CkPrintf("pe=%d> connecting %lx and %lx via msg\n", CkMyPe(), p.source, id);
#endif
    auto tup = std::make_tuple(p, id);
    auto size = PUP::size(tup);
    auto total_size = size + CmiMsgHeaderSizeBytes;
    auto msg = (char*)CmiAlloc(total_size);
    PUP::toMemBuf(tup, msg + CmiMsgHeaderSizeBytes, size);
    CmiSetHandler(msg, CkpvAccess(connection_handler_idx_));
    CmiSyncSendAndFree(home, total_size, msg);
  }
}

void component::send_value(const id_t& from, const id_t& to,
                           component::value_t&& msg) {
  auto home = get_home(to);
  if (home == CkMyPe()) {
    (manager::local())->recv_value(from, to, std::move(msg));
  } else {
    CkMessage* msg_raw = msg ? msg.get() : nullptr;
    if (msg_raw == nullptr) {
      component::send_invalidation(from, to);
    } else {
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
      utilities::pack_message(msg_ready);
      CmiSetHandler(env, CkpvAccess(value_handler_idx_));
      CmiSyncSendAndFree(home, env->getTotalsize(), reinterpret_cast<char*>(env));
    }
  }
}

void component::send_invalidation(const id_t& from, const id_t& to) {
  auto home = get_home(to);
  if (home == CkMyPe()) {
    (manager::local())->recv_invalidation(from, to);
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

placeholder component::put_placeholder(const bool& input) {
  auto port = (++this->port_authority);

  if (input) {
    this->connection(port, this->id);
  } else {
    this->connection(this->id, port);
  }

  return placeholder{.source = this->id, .port = port, .input = input};
}

void component::fill_placeholder(const placeholder& p,
                                 const component::id_t& id) {
  CkAssert(((p.source == this->id) && component::is_placeholder(p.port)) &&
           "invalid placeholder");

  auto end = p.is_input() ? this->incoming.end() : this->outgoing.end();
  auto search = p.is_input() ? std::find(this->incoming.begin(), end, p.port)
                             : std::find(this->outgoing.begin(), end, p.port);

  if (search != end) {
    *search = id;

    if (p.is_input()) {
      auto&& msg = std::move(this->inbox[id]);
      component::send_value(id, this->id, std::move(msg));
      this->inbox.erase(id);
    }
  } else if (p.is_output()) {
    auto&& msg = std::move(this->outbox[p.port]);
    component::send_value(this->id, id, std::move(msg));
    this->outbox.erase(p.port);
  }
}
}
}
