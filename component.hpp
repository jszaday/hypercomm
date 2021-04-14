#include <ck.h>

#include <map>
#include <memory>
#include <vector>
#include <algorithm>

namespace util {
std::shared_ptr<CkMessage> copy(const std::shared_ptr<CkMessage>& msg) {
  auto msg_raw = msg.get();
  auto msg_copy = (CkMessage*)CkCopyMsg((void**)&msg_raw);
  return std::shared_ptr<CkMessage>(msg_copy,
                                    [](CkMessage* msg) { CkFreeMsg(msg); });
}

void pack_message(CkMessage* msg) {
  auto idx = UsrToEnv(msg)->getMsgIdx();
  if (_msgTable[idx]->pack) {
    auto newMsg = _msgTable[idx]->pack(msg);
    CkAssert(msg == newMsg && "message changed due to packing!");
  }
}

void unpack_message(CkMessage* msg) {
  auto idx = UsrToEnv(msg)->getMsgIdx();
  if (_msgTable[idx]->unpack) {
    auto newMsg = _msgTable[idx]->unpack(msg);
    CkAssert(msg == newMsg && "message changed due to packing!");
  }
}
}

class manager;
class placeholder;

struct component {
  friend class manager;

  using value_t = std::shared_ptr<CkMessage>;
  using id_t = long unsigned int;

  id_t port_authority = std::numeric_limits<id_t>::max() << (sizeof(id_t) / 2);
  std::vector<value_t> accepted;
  std::vector<id_t> incoming;
  std::vector<id_t> outgoing;
  std::map<id_t, value_t> inbox;
  std::map<id_t, value_t> outbox;
  bool alive;
  id_t id;

  virtual void accept(const id_t& from, value_t&& msg) {
    CkAssert(this->alive && "only living components can accept values");
    auto search = std::find(this->incoming.begin(), this->incoming.end(), from);
    if (search == this->incoming.end()) {
      this->inbox.emplace(from, std::move(msg));
    } else {
      this->accepted.emplace_back(std::move(msg));
      this->incoming.erase(search);
    }
  }

  virtual value_t action(void) = 0;

  virtual int num_expected(void) const { return this->num_available(); }

  int num_available() const {
    return (this->incoming.size() + this->accepted.size());
  }

  bool ready(void) const {
    return this->alive && (this->accepted.size() == this->num_expected());
  }

  bool collectible(void) const {
    return !this->alive && this->incoming.empty() && this->outgoing.empty() && this->outbox.empty();
  }

  placeholder put_placeholder(const bool& input);

  void fill_placeholder(const placeholder&, const id_t&);

 protected:
  virtual void send(value_t&& msg) {
    CkAssert(!this->alive && "a living component cannot send values");

    while (!this->outgoing.empty()) {
      const id_t to = this->outgoing.back();
      this->outgoing.pop_back();

      value_t&& ready{};
      if (this->outgoing.empty()) {
        ready = std::move(msg);
      } else {
        ready = std::move(util::copy(msg));
      }

      if (is_placeholder(to)) {
        this->outbox.emplace(to, std::forward<value_t>(ready));
      } else {
        component::send_value(this->id, to, std::forward<value_t>(ready));
      }
    }
  }

  void connection(const id_t& from, const id_t& to) {
    CkAssert(!this->alive && "cannot connect to a living component");

    if (this->id == to) {
      this->incoming.push_back(from);
    } else if (this->id == from) {
      this->outgoing.push_back(to);
    } else {
      CkAbort("%lu> unable to link %lu to %lu", this->id, from, to);
    }
  }

  void invalidation(const id_t& from) {
    CkAssert(this->alive &&
             "only living components can invalidate connections");

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

  void erase_incoming(const id_t& from) {
    auto search = std::find(this->incoming.begin(), this->incoming.end(), from);
    CkAssert(search != this->incoming.end() && "could not find in-port");
    this->incoming.erase(search);
  }

 public:
  static bool is_placeholder(const component::id_t& id);

  static void send_value(const id_t& from, const id_t& to, value_t&& msg);

  static void send_invalidation(const id_t& from, const id_t& to);

  static void send_connection(const placeholder&, const id_t&);

  static void connect(const placeholder&, const std::shared_ptr<component>&);

  static void connect(const std::shared_ptr<component>& from,
                      const std::shared_ptr<component>& to) {
    to->connection(from->id, to->id);
    from->connection(from->id, to->id);
  }
};

struct threaded_component : public virtual component {};

struct passthru_component : public virtual component {
  virtual std::shared_ptr<CkMessage> action(void) override {
    CkAssert((this->accepted.size() == 1) &&
             "passthru components only expect one value");
    std::shared_ptr<CkMessage> msg = std::move(this->accepted[0]);
    this->accepted.clear();
    return msg;
  }
};

struct monovalue_component : public virtual component {
  virtual int num_expected(void) const { return 1; }
};

struct independent_component : public virtual component {
  virtual int num_expected(void) const { return 0; }
};

struct mux_component : public monovalue_component, public passthru_component {
  virtual bool screen(const std::shared_ptr<CkMessage>&) const = 0;

  virtual void accept(const component::id_t& from,
                      std::shared_ptr<CkMessage>&& msg) {
    if (this->screen(msg)) {
      component::accept(from, std::move(msg));
    } else if (this->incoming.empty()) {
      CkAbort("a multiplexer must accept at least one value");
    }
  }
};

struct demux_component : public monovalue_component, public passthru_component {
  virtual id_t route(const std::shared_ptr<CkMessage>&) const = 0;

  virtual std::shared_ptr<CkMessage> action(void) override {
    auto msg = passthru_component::action();
    auto chosen = this->route(msg);
    bool found = false;

    while (!this->outgoing.empty()) {
      const component::id_t to = this->outgoing.back();
      this->outgoing.pop_back();

      if (chosen == to) {
        found = true;
      } else {
        component::send_invalidation(this->id, to);
      }
    }

    CkAssert(found && "did not find selected component among outgoing");
    this->outgoing.push_back(chosen);

    return msg;
  }
};

struct placeholder {
  component::id_t source;
  component::id_t port;
  bool input;

  inline bool is_output(void) const { return !input; }
  inline const bool& is_input(void) const { return input; }
};

PUPbytes(placeholder);

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
