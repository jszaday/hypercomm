#ifndef __TESTER_HH__
#define __TESTER_HH__

#include <hypercomm/serialization/pup.hpp>
#include <hypercomm/messaging/messaging.hpp>
#include <hypercomm/core/immediate.hpp>
#include <hypercomm/utilities.hpp>
#include <hypercomm/components.hpp>

#include "tester.decl.h"

using namespace hypercomm;

using proxy_ptr = std::shared_ptr<hypercomm::proxy>;
using reduction_id_t = component_id_t;

std::string port2str(const entry_port_ptr& port);

template <typename Index>
struct reduction_port : public virtual entry_port {
  reduction_id_t id;
  Index index;

  reduction_port(PUP::reconstruct) {}

  reduction_port(const reduction_id_t& _1, const Index& _2)
      : id(_1), index(_2) {}

  virtual bool equals(const std::shared_ptr<comparable>& other) const {
    auto theirs = std::dynamic_pointer_cast<reduction_port>(other);
    return this->id == theirs->id && this->index == theirs->index;
  }

  virtual hash_code hash(void) const {
    return hash_combine(hash_code(id), hash_code(index));
  }

  virtual void __pup__(serdes& s) {
    s | id;
    s | index;
  }
};

struct reducer : public hypercomm::component {
  hypercomm::combiner_ptr combiner;

  reducer(const reduction_id_t& _1, const hypercomm::combiner_ptr& _2,
          value_t&& _3)
      : component(_1), combiner(_2) {
    QdCreate(1);
    this->accepted.emplace_back(std::move(_3));
  }

  virtual value_t action(void) {
    return this->combiner->send(std::move(this->accepted));
  }
};

template <typename Ordinal, typename Index>
struct section : public virtual comparable {
  virtual const std::vector<Index>& members(void) const = 0;

  virtual bool is_valid_ordinal(const Ordinal& ord) const {
    return (ord > -1) && (ord < this->num_members());
  }

  virtual std::size_t num_members() const { return (this->members()).size(); }

  virtual Ordinal ordinal_for(const Index& idx) const {
    const auto& indices = this->members();
    const auto search = std::find(indices.begin(), indices.end(), idx);
    return Ordinal((search == indices.end()) ? -1 : (search - indices.begin()));
  }

  virtual Index index_at(const Ordinal& ord) const {
    return (this->members())[ord];
  }

  virtual hash_code hash(void) const override { return 0x0; }
};

template <typename Index>
struct generic_section : public section<std::int64_t, Index>, public polymorph {
  using indices_type = std::vector<Index>;

  indices_type indices;

  generic_section(PUP::reconstruct) {}
  generic_section(indices_type&& _1) : indices(_1) {}

  virtual const indices_type& members(void) const override {
    return this->indices;
  }

  virtual bool equals(const std::shared_ptr<comparable>& other) const override {
    auto other_typed = std::dynamic_pointer_cast<generic_section<Index>>(other);
    return other_typed && (this->indices == other_typed->indices);
  }

  virtual void __pup__(serdes& s) override {
    s | indices;
  }
};

template <typename Index>
using section_ptr = std::shared_ptr<section<std::int64_t, Index>>;

template <typename Index>
class identity {
  reduction_id_t current_ = 0;

 public:
  reduction_id_t next_reduction(void) { return current_++; }
  virtual std::vector<Index> upstream(void) const = 0;
  virtual std::vector<Index> downstream(void) const = 0;
};

template <typename Index>
struct section_identity : public identity<Index> {
  section_ptr<Index> sect;
  Index mine;

  section_identity(const section_ptr<Index>& _1, const Index& _2)
      : sect(_1), mine(_2){};

  virtual std::vector<Index> downstream(void) const override {
    const auto ord = sect->ordinal_for(mine);
    CkAssert((sect->is_valid_ordinal(ord)) &&
             "identity> index must be member of section");
    const auto parent = binary_tree::parent(ord);
    auto rval = std::vector<Index>{};
    if (sect->is_valid_ordinal(parent)) rval.push_back(sect->index_at(parent));
    return rval;
  }

  virtual std::vector<Index> upstream(void) const override {
    const auto ord = sect->ordinal_for(mine);
    CkAssert((sect->is_valid_ordinal(ord)) &&
             "identity> index must be member of section");
    const auto left = binary_tree::left_child(ord);
    const auto right = binary_tree::right_child(ord);
    auto rval = std::vector<Index>{};
    if (sect->is_valid_ordinal(left)) rval.push_back(sect->index_at(left));
    if (sect->is_valid_ordinal(right)) rval.push_back(sect->index_at(right));
#if CMK_DEBUG
    CkPrintf("%d> has children %lu and %lu.\n", mine, left, right);
#endif
    return rval;
  }
};

struct forwarding_callback : public hypercomm::callback {
  std::shared_ptr<hypercomm::array_element_proxy> proxy;
  entry_port_ptr port;

  forwarding_callback(const proxy_ptr& _1, const entry_port_ptr& _2)
      : proxy(std::dynamic_pointer_cast<hypercomm::array_element_proxy>(_1)),
        port(_2) {}

  virtual void send(callback::value_type&&) override;

  virtual void __pup__(serdes& s) override {
    s | proxy;
    s | port;
  }
};

template <typename Index>
using identity_map = comparable_map<section_ptr<Index>,
                                    std::shared_ptr<section_identity<Index>>>;

using component_port_t = std::pair<component_id_t, components::port_id_t>;
using entry_port_map_t = comparable_map<entry_port_ptr, component_port_t>;
using component_map_t = std::unordered_map<component_id_t, component_ptr>;

template <typename Key>
using message_queue_t =
    comparable_map<Key, std::deque<std::shared_ptr<CkMessage>>>;

template <typename T>
T& reinterpret_index(CkArrayIndex& idx) {
  return *(reinterpret_cast<T*>(idx.data()));
}

template <typename T>
const T& reinterpret_index(const CkArrayIndex& idx) {
  return *(reinterpret_cast<const T*>(idx.data()));
}

template <typename Index, typename T>
inline Index conv2idx(const T& ord) {
  Index idx;
  idx.dimension = 1;
  reinterpret_index<T>(idx) = ord;
  return idx;
}

struct common_functions_ {
  virtual proxy_ptr __proxy__(void) const = 0;

  virtual const CkArrayIndex& __index_max__(void) const = 0;
};

template <typename Index>
struct locality_base : public virtual common_functions_ {
  message_queue_t<entry_port_ptr> port_queue;
  identity_map<Index> identities;
  entry_port_map_t entry_ports;
  component_map_t components;
  component_id_t component_authority = 0;

  const Index& __index__(void) const {
    return reinterpret_index<Index>(this->__index_max__());
  }

  using action_type = typename std::shared_ptr<immediate_action<void(locality_base<Index>*)>>;
  using impl_index_type = array_proxy::index_type;
  using array_proxy_ptr = std::shared_ptr<array_proxy>;

  static void send_action(const array_proxy_ptr& p, const Index& i, action_type&& a) {
    auto size = hypercomm::size(a);
    auto msg = CkAllocateMarshallMsg(size);
    auto packer = serdes::make_packer(msg->msgBuf);
    pup(packer, a);
    CProxyElement_locality_base_ peer(p->id(), conv2idx<impl_index_type>(i));
    peer.execute(msg);
  }

  void receive_action(const action_type& ptr) {
    ptr->action(this);
  }

  void broadcast(const section_ptr<Index>&, hypercomm_msg*);

  void receive_value(const entry_port_ptr& port,
                     std::shared_ptr<CkMessage>&& value) {
    auto search = this->entry_ports.find(port);
    if (search == std::end(this->entry_ports)) {
      QdCreate(1);
      port_queue[port].push_back(std::move(value));
    } else {
      CkAssert(search->first && search->first->alive && "entry port must be alive");
      this->try_send(search->second, std::move(value));
      this->try_collect(search);
    }
  }

  using entry_port_iterator = typename decltype(entry_ports)::iterator;

  void try_collect(entry_port_iterator& it) {
    const auto& port = it->first;
    port->alive = port->keep_alive();
    if (!port->alive) {
      this->entry_ports.erase(it);
    }
  }

  void try_collect(const component_id_t& which) {
    this->try_collect(this->components[which]);
  }

  void try_collect(const component_ptr& ptr) {
    if (ptr && ptr->collectible()) {
      const auto& id = ptr->id;
      if (ptr.use_count() != 1) {
        CkError("warning> component %lu replicated\n", id);
      }
      this->components.erase(id);
    }
  }

  void try_send(const component_port_t& port,
                std::shared_ptr<CkMessage>&& value) {
    auto search = components.find(port.first);
    CkAssert((search != components.end()) &&
             "message received for nonexistent component");

    search->second->receive_value(port.second, std::move(value));

    this->try_collect(search->second);
  }

  void resync_port_queue(entry_port_iterator& it) {
    const auto entry_port = it->first;
    auto search = port_queue.find(entry_port);
    if (search != port_queue.end()) {
      auto& buffer = search->second;
      while (entry_port->alive && !buffer.empty()) {
        auto& msg = buffer.front();
        this->try_send(it->second, std::move(msg));
        this->try_collect(it);
        buffer.pop_front();
        QdProcess(1);
      }
      if (buffer.empty()) {
        port_queue.erase(search);
      }
    }
  }

  void open(const entry_port_ptr& ours, const component_port_t& theirs) {
    ours->alive = true;
    auto pair = entry_ports.emplace(ours, theirs);
    CkAssert(pair.second && "entry port must be unique");
    this->resync_port_queue(pair.first);
  }

  void local_contribution(const section_ptr<Index>& which,
                          std::shared_ptr<CkMessage>&& value,
                          const combiner_ptr& fn, const callback_ptr& cb) {
    auto& ident = *(this->identity_for(which));
    auto next = ident.next_reduction();
    auto ustream = ident.upstream();
    auto dstream = ident.downstream();

    const auto& rdcr = this->emplace_component<reducer>(fn, std::move(value));

    for (const auto& up : ustream) {
      auto ours = std::make_shared<reduction_port<Index>>(next, up);
      auto theirs = rdcr->open_in_port();

      this->open(ours, std::make_pair(rdcr->id, theirs));
    }

    if (dstream.empty()) {
      rdcr->open_out_port(cb);
    } else {
      auto collective =
          std::dynamic_pointer_cast<array_proxy>(this->__proxy__());
      CkAssert(collective && "locality must be a valid collective");
      auto theirs = std::make_shared<reduction_port<Index>>(next, ident.mine);
      for (const auto& down : dstream) {
        impl_index_type down_idx;
        down_idx.dimension = 1;
        reinterpret_index<Index>(down_idx) = down;
        rdcr->open_out_port(std::make_shared<forwarding_callback>(
            (*collective)[down_idx], theirs));
      }
    }

    this->activate_component(rdcr);
  }

  const component_ptr& emplace_component(component_ptr&& which) {
    auto placed = this->components.emplace(which->id, std::move(which));
    CkAssert(placed.second && "component id must be unique");
    return placed.first->second;
  }

  template <typename T, typename... Args>
  const component_ptr& emplace_component(Args... args) {
    auto next = this->component_authority++;
    return this->emplace_component(
        std::make_shared<T>(next, std::move(args)...));
  }

  void activate_component(const component_ptr& which) {
    which->alive = true;

    which->resync_status();

    this->try_collect(which);
  }

  const std::shared_ptr<section_identity<Index>>& identity_for(
      const section_ptr<Index>& which) {
    auto search = identities.find(which);
    if (search == identities.end()) {
      auto mine = this->__index__();
      auto iter = identities.emplace(
          which, std::make_shared<section_identity<Index>>(which, mine));
      CkAssert(iter.second && "section should be unique!");
      return (iter.first)->second;
    } else {
      return search->second;
    }
  }
};

template<typename Index>
struct broadcaster: public immediate_action<void(locality_base<Index>*)> {
  using index_type = array_proxy::index_type;

  section_ptr<Index> section;
  std::shared_ptr<hypercomm_msg> msg;

  broadcaster(PUP::reconstruct) {}

  broadcaster(const section_ptr<Index>& _1, std::shared_ptr<hypercomm_msg>&& _2)
  : section(_1), msg(_2) {}

  virtual void action(locality_base<Index>* _1) override {
    auto& locality = const_cast<locality_base<Index>&>(*_1);
    auto collective =
        std::dynamic_pointer_cast<array_proxy>(locality.__proxy__());
    CkAssert(collective && "must be a valid collective");

    const auto& identity = locality.identity_for(this->section);
    auto ustream = identity->upstream();
    for (const auto& up : ustream) {
      auto copy = std::static_pointer_cast<hypercomm_msg>(utilities::copy_message(msg));
      auto next = std::make_shared<broadcaster>(this->section, std::move(copy));
      locality_base<Index>::send_action(collective, up, std::move(next));
    }

    const auto dest = msg->dst;
    locality.receive_value(dest, std::move(msg));
  }

  virtual void __pup__(serdes& s) override {
    s | section;
    s | msg;
  }
};

template<typename Index>
void locality_base<Index>::broadcast(const section_ptr<Index>& section, hypercomm_msg* _msg) {
  auto identity = this->identity_for(section);
  auto root = section->index_at(0);
  auto msg = std::static_pointer_cast<hypercomm_msg>(utilities::wrap_message(_msg));
  auto action = std::make_shared<broadcaster<Index>>(section, std::move(msg));

  using index_type = array_proxy::index_type;
  if (root == this->__index__()) {
    this->receive_action(action);
  } else {
    auto collective =
        std::dynamic_pointer_cast<array_proxy>(this->__proxy__());
    send_action(collective, root, action);
  }
}

#endif
