#ifndef __HYPERCOMM_CORE_CONFIG_HPP__
#define __HYPERCOMM_CORE_CONFIG_HPP__

#ifndef HYPERCOMM_PORT_SIZE
#define HYPERCOMM_PORT_SIZE 48
#endif

#include <cstdint>

namespace hypercomm {
  constexpr std::size_t kMinPortSize = HYPERCOMM_PORT_SIZE;
}

#endif
