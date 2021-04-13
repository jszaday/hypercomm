#include <ck.h>

#include <memory>
#include <vector>
#include <algorithm>

namespace util {
std::shared_ptr<CkMessage> copy(const std::shared_ptr<CkMessage>& msg) {
  return {};
}
}

class manager;

struct component {
  friend class manager;

  using value_t = std::shared_ptr<CkMessage>;
  using id_t = long unsigned int;

  std::vector<id_t> incoming;
  std::vector<id_t> outgoing;
  bool alive;
  id_t id;

  std::vector<std::shared_ptr<CkMessage>> buffer;

  virtual void accept(const id_t& from, value_t&& msg) {
    CkAssert(this->alive && "only living components can accept values");
    this->erase_incoming(from);
    this->buffer.emplace_back(std::move(msg));
  }

  virtual value_t action(void) = 0;

  virtual int num_expected(void) const { return this->num_available(); }

  int num_available() const {
    return (this->incoming.size() + this->buffer.size());
  }

  bool ready(void) const {
    return this->alive && (this->buffer.size() == this->num_expected());
  }

  bool collectible(void) const {
    return !this->alive && this->incoming.empty();
  }

 protected:
  virtual void send(value_t&& msg) {
    CkAssert(!this->alive && "a living component cannot send values");

    while (!this->outgoing.empty()) {
      const id_t to = this->outgoing.back();
      this->outgoing.pop_back();

      if (this->outgoing.empty()) {
        component::send_value(this->id, to, std::move(msg));
      } else {
        component::send_value(this->id, to, std::move(util::copy(msg)));
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
    this->alive = (min >= avail);

    while (!(this->alive || this->outgoing.empty())) {
      const id_t to = this->outgoing.back();
      this->outgoing.pop_back();
      component::send_invalidation(this->id, to);
    }
  }

  void erase_incoming(const id_t& from) {
    auto search = std::find(this->incoming.begin(), this->incoming.end(), from);
    CkAssert(search != this->incoming.end() && "could not find in-port");
    this->incoming.erase(search);
  }

 public:
  static void send_value(const id_t& from, const id_t& to, value_t&& msg);

  static void send_invalidation(const id_t& from, const id_t& to);

  static void connect(const std::shared_ptr<component>& from,
                      const std::shared_ptr<component>& to) {
    to->connection(from->id, to->id);
    from->connection(from->id, to->id);
  }
};

struct passthru_component : public virtual component {
  virtual std::shared_ptr<CkMessage> action(void) override {
    CkAssert(this->buffer.size() == 1 &&
             "passthru components only expect one value");
    std::shared_ptr<CkMessage> msg = std::move(this->buffer[0]);
    this->buffer.clear();
    return msg;
  }
};

struct monovalue_component : public virtual component {
  virtual int num_expected(void) const { return 1; }
};

struct mux_component : public monovalue_component, public passthru_component {
  virtual bool screen(const std::shared_ptr<CkMessage>&) const = 0;

  virtual void accept(const component::id_t& from,
                      std::shared_ptr<CkMessage>&& msg) {
    CkAssert(this->alive && "only living components can accept values");

    this->erase_incoming(from);

    if (this->screen(msg)) {
      this->buffer.emplace_back(std::move(msg));
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
