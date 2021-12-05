#ifndef __HYPERCOMM_COMPONENTS_IDENTIFIERS_HPP__
#define __HYPERCOMM_COMPONENTS_IDENTIFIERS_HPP__

#include <memory>
#include <cstdint>
#include <utility>

#include "../core/config.hpp"

#if HYPERCOMM_USE_PHMAP
#include <parallel_hashmap/phmap.h>
#else
#include <unordered_map>
#endif

namespace hypercomm {

namespace components {
struct base_;
}

using component_ptr = std::unique_ptr<components::base_>;

using component_id_t = std::uint64_t;
using component_port_t = std::uint64_t;
using reduction_id_t = component_id_t;
using com_port_pair_t = std::pair<component_id_t, component_port_t>;

#if HYPERCOMM_USE_PHMAP
template <typename K, typename V, typename Hash = phmap::Hash<K>,
          typename KeyEqual = phmap::EqualTo<K>>
using hash_map = phmap::flat_hash_map<K, V, Hash, KeyEqual>;
#else
template <typename K, typename V, typename Hash = std::hash<K>,
          typename KeyEqual = std::equal_to<K>>
using hash_map = std::unordered_map<K, V, Hash, KeyEqual>;
#endif

using component_map = hash_map<component_id_t, component_ptr>;

}  // namespace hypercomm

#endif
