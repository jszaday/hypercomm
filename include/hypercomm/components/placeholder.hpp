#ifndef __HYPERCOMM_COMPONENTS_PLACEHOLDER_HPP__
#define __HYPERCOMM_COMPONENTS_PLACEHOLDER_HPP__

#include "component.hpp"

namespace hypercomm {
namespace components {
struct placeholder {
  component::id_t source;
  component::id_t port;
  bool input;

  inline bool is_output(void) const { return !input; }
  inline const bool& is_input(void) const { return input; }
};
}
}

PUPbytes(hypercomm::components::placeholder);

#endif
