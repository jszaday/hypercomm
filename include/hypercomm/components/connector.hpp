#ifndef __HYPERCOMM_COMPONENTS_CONNECTOR_HPP__
#define __HYPERCOMM_COMPONENTS_CONNECTOR_HPP__

#include <cstdint>
#include "../serialization/construction.hpp"

namespace hypercomm {
namespace components {
template <typename T>
struct connector_ {
  enum options_ { kInvalid, kWire };

  options_ which_;

  union u_state_ {
    struct s_wire_ {
      std::size_t com;
      std::size_t port;

      s_wire_(std::size_t com_, std::size_t port_) : com(com_), port(port_) {}
    } wire_;

    u_state_(tags::no_init) {}

    u_state_(std::size_t com, std::size_t port) : wire_(com, port) {}
  } state_;

  connector_(void) : which_(kInvalid), state_(tags::no_init()) {}

  connector_(std::size_t com, std::size_t port)
      : which_(kWire), state_(com, port) {}

  inline bool ready(void) const { return this->which_ != kInvalid; }

  inline void relay(T&& value) {
    switch (this->which_) {
      case kWire:
        // access_context_()->accept((this->state_).wire_.com,
        //                           (this->state_).wire_.port,
        //                           cast_value(std::move(value)));
        break;
      case kInvalid:
        CkAbort("delivery to invalid connector!");
        break;
    }
  }
};
}  // namespace components
}  // namespace hypercomm

#endif
