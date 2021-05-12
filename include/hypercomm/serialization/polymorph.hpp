#ifndef __HYPERCOMM_POLYMORPH_HPP__
#define __HYPERCOMM_POLYMORPH_HPP__

#include "serdes.hpp"

namespace hypercomm {

struct polymorph {
  struct trait {};

  virtual ~polymorph() = default;
  virtual void __pup__(serdes& s) = 0;
};

using polymorph_ptr = std::shared_ptr<polymorph>;
}

#endif
