#ifndef __HYPERCOMM_UTILITIES_BACKSTAGE_PASS_HPP__
#define __HYPERCOMM_UTILITIES_BACKSTAGE_PASS_HPP__

namespace hypercomm {

namespace detail {
template <typename type, type value, typename tag>
class access_bypass {
  friend type get(tag) { return value; }
};

struct backstage_pass {};
}
}

#endif
