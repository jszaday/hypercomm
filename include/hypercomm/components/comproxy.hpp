#ifndef __HYPERCOMM_COMPONENTS_COMPROXY_HPP__
#define __HYPERCOMM_COMPONENTS_COMPROXY_HPP__

#include "../core/common.hpp"

namespace hypercomm {

template <typename A, typename Enable = void>
class comproxy;

template <typename A>
class comproxy<A, typename std::enable_if<
                      std::is_base_of<components::base_, A>::value>::type> {
  static constexpr auto nil_id_ = std::numeric_limits<component_id_t>::max();
  component_id_t id_;

 public:
  using type = A;

  comproxy(void) : id_(nil_id_) {}
  comproxy(const component_id_t& id) : id_(id) {}
  comproxy(const comproxy<A>& proxy) : id_(proxy.id_) {}
  comproxy(comproxy<A>&& proxy) : id_(proxy.id_) { proxy.id_ = nil_id_; }

  comproxy<A>& operator=(const comproxy<A>& other) {
    if (this != &other) {
      this->id_ = other.id_;
    }
    return *this;
  }

  A* operator->(void) const noexcept {
    return access_context_()->get_component<A>(this->id_);
  }

  A& operator*(void) const noexcept { return *(this->operator->()); }

  operator component_id_t(void) const noexcept { return this->id_; }

  operator bool(void) const noexcept { return (this->id_ != nil_id_); }
};
}  // namespace hypercomm

#endif
