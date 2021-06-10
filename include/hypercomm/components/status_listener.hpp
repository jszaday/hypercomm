#ifndef __HYPERCOMM_COMPONENTS_STATUS_LISTENER__
#define __HYPERCOMM_COMPONENTS_STATUS_LISTENER__

#include "../serialization/polymorph.hpp"

namespace hypercomm {

struct component;

namespace components {

class status_listener : virtual public polymorph::trait {
 public:
  virtual void on_completion(const component&) = 0;
  virtual void on_invalidation(const component&) = 0;
};
}
}

#endif
