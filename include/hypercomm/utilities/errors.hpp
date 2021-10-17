#ifndef __HYPERCOMM_UTILITIES_ERRORS_HPP__
#define __HYPERCOMM_UTILITIES_ERRORS_HPP__

#include <charm++.h>
#include <cstdarg>

namespace hypercomm {
inline void unreachable [[noreturn]] (const char* msg = nullptr) {
  CkAbort("fatal> unreachable code location, %s.\n", msg ? msg : "unknown");
}

inline void not_implemented [[noreturn]] (const char* msg, ...)
#if defined __GNUC__ || defined __clang__
__attribute__((format(printf, 1, 2)))
#endif
;

inline void not_implemented [[noreturn]] (const char* msg, ...) {
  std::stringstream ss;
  ss << "fatal> feature not implemented, ";
  ss << msg ? msg : "(nil)";
  ss << "." << std::endl;
  auto s = ss.str();
  auto* fmt = s.c_str();

  va_list arg;
  va_start(arg, msg);
  auto len = vsnprintf(NULL, 0, fmt, arg);
  va_end(arg);

  auto out = new char[len];

  va_start(arg, msg);
  vsnprintf(out, len + 1, fmt, arg);
  va_end(arg);

  CkAbort("%s", out);

  delete[] out;
}

inline void not_implemented [[noreturn]] (void) { not_implemented(nullptr); }
}  // namespace hypercomm

#endif
