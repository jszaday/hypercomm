#ifndef __HYPERCOMM_CORE_CONFIG_HPP__
#define __HYPERCOMM_CORE_CONFIG_HPP__

#ifndef HYPERCOMM_PORT_SIZE
#define HYPERCOMM_PORT_SIZE 48
#endif

#ifndef HYPERCOMM_STACK_SIZE
#define HYPERCOMM_STACK_SIZE 2
#endif

#define HYPERCOMM_USE_PHMAP 1
#define HYPERCOMM_STRICT_MODE 0
#define HYPERCOMM_ERROR_CHECKING 1

#include <cstdint>

namespace hypercomm {
constexpr std::size_t kStackSize = HYPERCOMM_STACK_SIZE;
constexpr std::size_t kMinPortSize = HYPERCOMM_PORT_SIZE;
constexpr std::size_t kZeroCopySize = 256 * 1024;
}  // namespace hypercomm

#endif
