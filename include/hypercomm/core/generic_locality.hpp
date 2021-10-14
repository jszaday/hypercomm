#ifndef __HYPERCOMM_CORE_GENLOC_HPP__
#define __HYPERCOMM_CORE_GENLOC_HPP__

#include "../messaging/destination.hpp"
#include "../messaging/interceptor.hpp"
#include "../tree_builder/manageable_base.hpp"
#include "zero_copy_value.hpp"

/* TODO consider introducing a simplified connection API that
 *     utilizes "port authorities", aka port id counters, to
 *     remove src/dstPort for trivial, unordered connections
 */


namespace hypercomm {
class generic_locality_ : public manageable_base_ {
 public:
  template <typename A, typename Enable>
  friend class comproxy;
  friend class detail::payload;

  template<typename T>
  using connector = components::connector_<T>;

  entry_port_map entry_ports;
  component_map components;

  mapped_queue<deliverable> port_queue;
  std::vector<component_id_t> invalidations;

  component_id_t component_authority = 0;

  template <typename T>
  using endpoint_queue = endpoint_map<std::deque<T>>;
  endpoint_queue<std::pair<std::shared_ptr<zero_copy_value>, CkNcpyBuffer*>>
      outstanding;

  endpoint_queue<
      std::tuple<std::shared_ptr<void>, std::size_t, CkNcpyBufferPost>>
      buffers;

  using component_type = typename decltype(components)::mapped_type;
  using entry_port_iterator = typename decltype(entry_ports)::iterator;
  using outstanding_iterator = typename decltype(outstanding)::iterator;

  generic_locality_(void) { this->update_context(); }
  virtual ~generic_locality_();

  void update_context(void);

  void receive(const UShort& ep, CkMessage*&&);

  void receive(const entry_port_ptr& ep, value_ptr&&);

  inline void receive(CkMessage* msg) {
    this->receive(UsrToEnv(msg)->getEpIdx(), std::move(msg));
  }

  void loopback(const entry_port_ptr& port, deliverable& value);
  bool has_value(const entry_port_ptr& port) const;

  // template <typename Destination>
  // void open(const entry_port_ptr& ours, const Destination& theirs);
  // void try_send(const destination& dest, value_ptr&& value);
  // void try_send(const component_port_pair& port, value_ptr&& value);

  // template<typename Deliverable, typename... Args>
  // void pass_thru(Deliverable&& msg, Args... args) {
  //   this->pass_thru(msg, connector<Deliverable>(std::forward<Args>(args)...));
  // }

  void receive_value(CkMessage* msg, const value_handler_fn_& fn);

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

  // inline void connect(const component_id_t& src,
  //                     const component_port_t& srcPort,
  //                     const component_id_t& dst,
  //                     const component_port_t& dstPort) {
  //   this->components[src]->update_destination(
  //       srcPort, this->make_connector(dst, dstPort));
  // }

  // inline void connect(const component_id_t& src,
  //                     const component_port_t& srcPort,
  //                     const callback_ptr& cb) {
  //   this->components[src]->update_destination(srcPort, cb);
  // }

  // inline void connect(const entry_port_ptr& srcPort, const component_id_t& dst,
  //                     const component_port_t& dstPort) {
  //   this->components[dst]->add_listener(
  //       &on_status_change, new entry_port_ptr(srcPort),
  //       [](void* value) { delete (entry_port_ptr*)value; });
  //   this->open(srcPort, std::make_pair(dst, dstPort));
  // }

  // callback_ptr make_connector(const component_id_t& com,
  //                             const component_port_t& port);

 protected:
  bool invalidated(const component_id_t& id);

  void pass_thru(const destination& dst, CkMessage* msg);
  void pass_thru(const destination& dst, value_ptr&& val);

  inline void pass_thru(const destination& dst, deliverable& dev) {
    if (dev.kind == deliverable::kMessage) {
      this->pass_thru(dst, dev.release<CkMessage>());
    } else {
      auto* val = dev.release<hyper_value>();
      this->pass_thru(dst, value_ptr(val));
    }
  }

 private:
  outstanding_iterator poll_buffer(CkNcpyBuffer* buffer,
                                   const std::shared_ptr<zero_copy_value>&,
                                   const std::size_t& goal);

  template <typename A>
  A* get_component(const component_id_t& id) {
    auto search = this->components.find(id);
    if (search != std::end(this->components)) {
      return utilities::fast_cast<A>(search->second.get());
    } else {
      return nullptr;
    }
  }

  static void on_status_change(const components::base_*, components::status_ status,
                               void* arg) {
    auto* port = (entry_port_ptr*)arg;
    access_context_()->invalidate_port(*port);
    delete port;
  }
};

// template <typename Destination>
// void generic_locality_::open(const entry_port_ptr& ours,
//                              const Destination& theirs) {
//   ours->alive = true;
//   auto pair = this->entry_ports.emplace(ours, theirs);
// #if CMK_ERROR_CHECKING
//   if (!pair.second) {
//     std::stringstream ss;
//     ss << "[";
//     for (const auto& epp : this->entry_ports) {
//       const auto& other_port = epp.first;
//       if (comparable_comparator<entry_port_ptr>()(ours, other_port)) {
//         ss << "{" << other_port->to_string() << "}, ";
//       } else {
//         ss << other_port->to_string() << ", ";
//       }
//     }
//     ss << "]";

//     CkAbort("fatal> adding non-unique port %s to:\n\t%s\n",
//             ours->to_string().c_str(), ss.str().c_str());
//   }
// #endif
//   this->resync_port_queue(pair.first);
// }

template <void fn(generic_locality_*, const entry_port_ptr&,
                  value_ptr&&)>
void CkIndex_locality_base_::value_handler(CkMessage* msg, CkMigratable* self) {
  ((generic_locality_*)self)->receive_value(msg, fn);
}

}  // namespace hypercomm

#endif
