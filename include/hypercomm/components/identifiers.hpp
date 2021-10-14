#ifndef __HYPERCOMM_COMPONENTS_IDENTIFIERS_HPP__
#define __HYPERCOMM_COMPONENTS_IDENTIFIERS_HPP__

#include <cstdint>
#include <pup.h>

namespace hypercomm {

using component_id_t = std::uint64_t;
using component_port_t = std::uint64_t;
using reduction_id_t = component_id_t;

}  // namespace hypercomm

#endif
