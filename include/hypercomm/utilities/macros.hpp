#ifndef __HYPERCOMM_UTILITIES_MACROS_HPP__
#define __HYPERCOMM_UTILITIES_MACROS_HPP__

// register a handler fn with the rts on a per-pe basis
#define CmiAutoRegister(fn)                                     \
  ([](void) -> const int& {                                     \
    CpvStaticDeclare(int, handler_);                            \
    if (!CpvInitialized(handler_) || !CpvAccess(handler_)) {    \
      CpvInitialize(int, handler_);                             \
      CpvAccess(handler_) = CmiRegisterHandler((CmiHandler)fn); \
    }                                                           \
    return CpvAccess(handler_);                                 \
  })()

#endif
