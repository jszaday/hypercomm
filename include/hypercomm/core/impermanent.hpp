#ifndef __HYPERCOMM_CORE_IMPERMANENT_HPP__
#define __HYPERCOMM_CORE_IMPERMANENT_HPP__

#include "../serialization/polymorph.hpp"

namespace hypercomm {
class impermanent: public virtual polymorph::trait {
 public:
  bool alive;

  virtual bool keep_alive(void) const { return false; }
};
}

#endif
