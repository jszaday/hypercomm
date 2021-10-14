#ifndef __HYPERCOMM_CORE_CONFIG_HPP__
#define __HYPERCOMM_CORE_CONFIG_HPP__

#ifndef HYPERCOMM_PORT_SIZE
#define HYPERCOMM_PORT_SIZE 48
#endif

#ifndef HYPERCOMM_ERROR_CHECKING
#define HYPERCOMM_ERROR_CHECKING CMK_ERROR_CHECKING
#endif

#include <cstdint>

namespace hypercomm {
constexpr std::size_t kMinPortSize = HYPERCOMM_PORT_SIZE;
constexpr std::size_t kZeroCopySize = 256 * 1024;
}  // namespace hypercomm

#endif
