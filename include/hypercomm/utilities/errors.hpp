#ifndef __HYPERCOMM_UTILITIES_ERRORS_HPP__
#define __HYPERCOMM_UTILITIES_ERRORS_HPP__

#include <charm++.h>

namespace hypercomm {
inline void unreachable [[noreturn]] (const char* msg = nullptr) {
  CkAbort("fatal> unreachable code location, %s.\n", msg ? msg : "unknown");
}

inline void not_implemented [[noreturn]] (const char* msg = nullptr) {
  CkAbort("fatal> feature not implemented, %s.\n", msg ? msg : "unknown");
}
}  // namespace hypercomm

#endif
