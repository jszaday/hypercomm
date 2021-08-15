#ifndef __HYPERCOMM_SECTIONS_ROOT_LISTENER_HPP__
#define __HYPERCOMM_SECTIONS_ROOT_LISTENER_HPP__

#include <charm++.h>

namespace hypercomm {
namespace detail {

class root_listener {
  virtual bool on_root_assignment(const CkArrayID&, const CkArrayIndex&) = 0;
};

}
}

#endif
