#ifndef __HYPERCOMM_COMPONENTS_COMPROXY_HPP__
#define __HYPERCOMM_COMPONENTS_COMPROXY_HPP__

#include "../core/common.hpp"

namespace hypercomm {

template <typename A>
class comproxy<
    A, typename std::enable_if<std::is_base_of<component, A>::value>::type> {
  using id_t = component_id_t;
  id_t id;

 public:
  using type = A;

  comproxy(void) : id(std::numeric_limits<id_t>::max()) {}
  comproxy(const id_t& id) : id(id) {}
  comproxy(const comproxy<A>& proxy) : id(proxy->id) {}

  A* operator->(void) const noexcept {
    return access_context_()->get_component<A>(this->id);
  }

  operator id_t(void) const noexcept { return this->id; }

  operator bool(void) const noexcept {
    return (this->id != std::numeric_limits<id_t>::max());
  }
};
}  // namespace hypercomm

#endif
