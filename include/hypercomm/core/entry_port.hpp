#ifndef __HYPERCOMM_ENTRY_PORT_HPP__
#define __HYPERCOMM_ENTRY_PORT_HPP__

#include "comparable.hpp"

namespace hypercomm {
struct entry_port : public virtual polymorph, public virtual comparable {
  virtual bool keep_alive(void) const = 0;
};

using entry_port_ptr = std::shared_ptr<entry_port>;
}

#endif
