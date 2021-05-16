#ifndef __HYPERCOMM_ENTRY_PORT_HPP__
#define __HYPERCOMM_ENTRY_PORT_HPP__

#include "comparable.hpp"
#include "impermanent.hpp"

namespace hypercomm {
struct entry_port : public virtual polymorph,
                    public virtual comparable,
                    public virtual impermanent {};

using entry_port_ptr = std::shared_ptr<entry_port>;
}

#endif
