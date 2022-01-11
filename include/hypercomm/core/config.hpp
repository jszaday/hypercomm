#ifndef __HYPERCOMM_CORE_CONFIG_HPP__
#define __HYPERCOMM_CORE_CONFIG_HPP__

#ifndef HYPERCOMM_PORT_SIZE
#define HYPERCOMM_PORT_SIZE 48
#endif

#ifndef HYPERCOMM_STACK_SIZE
#define HYPERCOMM_STACK_SIZE 2
#endif

#ifndef HYPERCOMM_USE_PHMAP
#define HYPERCOMM_USE_PHMAP 0
#endif

#define HYPERCOMM_STRICT_MODE 0
#define HYPERCOMM_ERROR_CHECKING 1

#include <cstdint>
#include <pup.h>

#if HYPERCOMM_USE_PHMAP
#include <parallel_hashmap/phmap.h>
#else
#include <unordered_map>
#endif

namespace hypercomm {
constexpr std::size_t kStackSize = HYPERCOMM_STACK_SIZE;
constexpr std::size_t kMinPortSize = HYPERCOMM_PORT_SIZE;
constexpr std::size_t kZeroCopySize = 256 * 1024;

namespace tags {
using reconstruct = PUP::reconstruct;
}

#if HYPERCOMM_USE_PHMAP
template <typename K, typename V, typename Hash = phmap::Hash<K>,
          typename KeyEqual = phmap::EqualTo<K>>
using hash_map = phmap::flat_hash_map<K, V, Hash, KeyEqual>;
#else
template <typename K, typename V, typename Hash = std::hash<K>,
          typename KeyEqual = std::equal_to<K>>
using hash_map = std::unordered_map<K, V, Hash, KeyEqual>;
#endif
}  // namespace hypercomm

#endif
