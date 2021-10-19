#ifndef __HYPERCOMM_CORE_GENLOC_HPP__
#define __HYPERCOMM_CORE_GENLOC_HPP__

#include "../components/component.hpp"
#include "../messaging/interceptor.hpp"
#include "../tree_builder/manageable_base.hpp"

/* TODO consider introducing a simplified connection API that
 *     utilizes "port authorities", aka port id counters, to
 *     remove src/dstPort for trivial, unordered connections
 */

namespace hypercomm {
class generic_locality_ : public manageable_base_ {
 public:
  template <typename A, typename Enable>
  friend class comproxy;
  friend class deliverable;
  friend class CkIndex_locality_base_;

  entry_port_map entry_ports;
  component_map components;

  template <typename T>
  using endpoint_queue = endpoint_map<std::deque<T>>;
  endpoint_queue<std::pair<zero_copy_value*, CkNcpyBuffer*>> outstanding;
  endpoint_queue<
      std::tuple<std::shared_ptr<void>, std::size_t, CkNcpyBufferPost>>
      buffers;

  mapped_queue<deliverable> port_queue;
  std::vector<component_id_t> invalidations;
  component_id_t component_authority = 0;

  using component_type = typename decltype(components)::mapped_type;
  using entry_port_iterator = typename decltype(entry_ports)::iterator;
  using outstanding_iterator = typename decltype(outstanding)::iterator;

  generic_locality_(void) { this->update_context(); }
  virtual ~generic_locality_();

  void update_context(void);

  void receive(deliverable&&);
  void passthru(const destination& dst, deliverable&&);
  void passthru(const com_port_pair_t& ep, deliverable&&);

  void loopback(const entry_port_ptr& port, deliverable&& value);
  bool has_value(const entry_port_ptr& port) const;

  void open(const entry_port_ptr& ours, destination&& theirs);
  template <typename... Args>
  void open(const entry_port_ptr& ours, Args... args) {
    this->open(ours, std::move(destination(std::forward<Args>(args)...)));
  }

  void resync_port_queue(entry_port_iterator& it);
  void invalidate_port(const entry_port_ptr& port);

  void activate_component(const component_id_t& id);
  void invalidate_component(const component_id_t& id);

  template <typename T>
  inline is_valid_endpoint_t<T> manual_mode(const T& ep) {
    this->outstanding[ep];
  }

  template <typename T>
  inline is_valid_endpoint_t<T> post_buffer(
      const T& ep, const std::shared_ptr<void>& buffer, const std::size_t& size,
      const CkNcpyBufferPost& mode = {CK_BUFFER_UNREG, CK_BUFFER_DEREG}) {
    this->post_buffer(endpoint(ep), buffer, size, mode);
  }

  void post_buffer(const endpoint& ep, const std::shared_ptr<void>& buffer,
                   const std::size_t& size, const CkNcpyBufferPost& mode);

  inline void try_collect(const component_id_t& which) {
    this->try_collect(this->components[which]);
  }

  inline void try_collect(const component_type& com) {
    if (com && com->collectible()) {
      this->components.erase(com->id);
    }
  }

  inline void connect(const entry_port_ptr& srcPort, const component_id_t& dst,
                      const component_port_t& dstPort) {
    this->components[dst]->add_listener(
        &on_status_change, new entry_port_ptr(srcPort),
        [](void* value) { delete (entry_port_ptr*)value; });
    this->open(srcPort, std::make_pair(dst, dstPort));
  }

 protected:
  void receive_message(CkMessage* msg);
  void receive_value(CkMessage* msg, const value_handler_fn_& fn);

  bool invalidated(const component_id_t& id);

 private:
  outstanding_iterator poll_buffer(CkNcpyBuffer*, zero_copy_value*,
                                   const std::size_t&);

  template <typename A>
  A* get_component(const component_id_t& id) {
    auto search = this->components.find(id);
    if (search != std::end(this->components)) {
      return utilities::fast_cast<A>(search->second.get());
    } else {
      return nullptr;
    }
  }

  static void on_status_change(const components::base_*,
                               components::status_ status, void* arg) {
    auto* port = (entry_port_ptr*)arg;
    access_context_()->invalidate_port(*port);
    delete port;
  }
};

template <void fn(generic_locality_*, deliverable&&)>
void CkIndex_locality_base_::value_handler(CkMessage* msg, CkMigratable* mig) {
  auto* self = static_cast<generic_locality_*>(mig);
  self->update_context();
  self->receive_value(msg, fn);
}

template <typename... Args>
void passthru_context_(Args&&... args) {
  access_context_()->passthru(std::forward<Args>(args)...);
}

}  // namespace hypercomm

#endif
