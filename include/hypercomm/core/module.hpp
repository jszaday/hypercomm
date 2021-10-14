#ifndef __HYPERCOMM_CORE_ENTRY_METHODS_HPP__
#define __HYPERCOMM_CORE_ENTRY_METHODS_HPP__

#include "common.hpp"
#include "../messaging/messaging.hpp"
#include "../tree_builder/manageable_base.hpp"

namespace hypercomm {

class locality_base_;
class generic_locality_;

using value_handler_fn_ = void (*)(generic_locality_*, const entry_port_ptr&,
                                   value_ptr&&);

class CkIndex_locality_base_ {
 public:
  static int __idx;
  static const int& idx_demux_CkMessage(void);
  static const int& idx_execute_CkMessage(void);
  static const int& idx_replace_downstream_CkMessage(void);

  static value_handler_fn_ get_value_handler(const int& epIdx);
  static void put_value_handler(const int& epIdx, const value_handler_fn_& fn);

  inline static value_handler_fn_ default_value_handler(void) {
    return get_value_handler(idx_demux_CkMessage());
  }

  template <void fn(generic_locality_*, const entry_port_ptr&,
                    value_ptr&&)>
  static int register_value_handler(const char* name, const int& flags = 0) {
    auto epIdx = CkRegisterEp(name, (CkCallFnPtr)&value_handler<fn>,
                              CMessage_CkMessage::__idx, __idx, flags);
    put_value_handler(epIdx, fn);
    return epIdx;
  }

  template <void fn(generic_locality_*, const entry_port_ptr&,
                    value_ptr&&)>
  static void value_handler(CkMessage* msg, CkMigratable* base);
};

// NOTE ( hypercomm claims absolute control over these )
using CProxy_locality_base_ = CProxy_ArrayBase;
using CProxyElement_locality_base_ = CProxyElement_ArrayElement;
using CProxySection_locality_base_ = CProxySection_ArrayElement;

namespace core {
void initialize(void);
}  // namespace core
}  // namespace hypercomm

#endif
