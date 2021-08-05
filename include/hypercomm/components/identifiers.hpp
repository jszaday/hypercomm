#ifndef __HYPERCOMM_COMPONENTS_IDENTIFIERS_HPP__
#define __HYPERCOMM_COMPONENTS_IDENTIFIERS_HPP__

#include <cstdint>
#include <pup.h>

namespace hypercomm {

using component_id_t = std::uint64_t;
using reduction_id_t = component_id_t;

namespace components {
using port_id_t = std::uint64_t;

enum port_direction : bool { INPUT = true, OUTPUT = false };

struct port_handle {
  component_id_t com;
  port_id_t port;
  port_direction dir;

  inline bool is_output(void) const { return dir == OUTPUT; }
  inline bool is_input(void) const { return dir == INPUT; }
};
}  // namespace components
}  // namespace hypercomm

PUPbytes(hypercomm::components::port_direction);
PUPbytes(hypercomm::components::port_handle);

#endif
