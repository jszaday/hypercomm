#ifndef __HYPERCOMM_COMPONENTS_MANAGER_HPP__
#define __HYPERCOMM_COMPONENTS_MANAGER_HPP__

#include "component.hpp"

#include <deque>

namespace hypercomm {
namespace components {
struct manager {
  manager(void) = default;

  using component_t = std::shared_ptr<component>;
  using buffer_t = std::deque<std::pair<component::id_t, component::value_t>>;

  void recv_value(const component::id_t& from, const component::id_t& to,
                  component::value_t&& msg);

  void recv_invalidation(const component::id_t& from,
                         const component::id_t& to);

  void recv_connection(const placeholder& p, const component::id_t& id);

  void emplace(component_t&& which);

  void try_action(component_t& ptr);
  void try_action(const component::id_t& which);

  void try_collect(component_t& ptr);
  void try_collect(const component::id_t& which);

  static void trigger_action(component*);
  static CthThread launch_action(component*);

  std::map<component::id_t, component_t> components;
  std::map<component::id_t, buffer_t> buffers;

  using manager_t = std::unique_ptr<manager>;
  static manager_t& local(void);
};

}
}

#endif
