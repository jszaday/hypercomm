#ifndef __HYPERCOMM_COMPONENTS_IDENTIFIERS_HPP__
#define __HYPERCOMM_COMPONENTS_IDENTIFIERS_HPP__

#include <memory>
#include <cstdint>
#include <utility>

#include "../core/config.hpp"

namespace hypercomm {

namespace components {
struct base_;
}

using component_ptr = std::unique_ptr<components::base_>;

using component_id_t = std::uint64_t;
using component_port_t = std::uint64_t;
using reduction_id_t = component_id_t;
using com_port_pair_t = std::pair<component_id_t, component_port_t>;

using component_map = hash_map<component_id_t, component_ptr>;

}  // namespace hypercomm

#endif
