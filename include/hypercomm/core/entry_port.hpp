#ifndef __HYPERCOMM_ENTRY_PORT_HPP__
#define __HYPERCOMM_ENTRY_PORT_HPP__

#include "impermanent.hpp"

namespace hypercomm {

struct entry_port : public virtual polymorph,
                    public virtual comparable,
                    public virtual impermanent,
                    public virtual_enable_shared_from_this<entry_port> {
  virtual std::string to_string(void) const = 0;
};
}  // namespace hypercomm

#endif
