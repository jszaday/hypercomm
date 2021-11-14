#ifndef __HYPERCOMM_CORE_ENTRY_METHODS_HPP__
#define __HYPERCOMM_CORE_ENTRY_METHODS_HPP__

#include "../core/common.hpp"
#include "../messaging/messaging.hpp"
#include "../tree_builder/manageable_base.hpp"

namespace hypercomm {

class deliverable;
class locality_base_;
class generic_locality_;

using value_handler_fn_ = void (*)(generic_locality_*, deliverable&&);

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

  template <void fn(generic_locality_*, deliverable&&)>
  static int register_value_handler(const char* name, const int& flags = 0) {
    auto epIdx = CkRegisterEp(name, (CkCallFnPtr)&value_handler<fn>,
                              CMessage_CkMessage::__idx, __idx, flags);
    put_value_handler(epIdx, fn);
    return epIdx;
  }

  template <void fn(generic_locality_*, deliverable&&)>
  static void value_handler(CkMessage* msg, CkMigratable* base);
};

// NOTE ( hypercomm claims absolute control over these )
using CProxy_locality_base_ = CProxy_ArrayBase;
using CProxySection_locality_base_ = CProxySection_ArrayElement;

struct CProxyElement_locality_base_ : public CProxyElement_ArrayElement {
  explicit CProxyElement_locality_base_(void) : CProxyElement_ArrayElement() {}

  explicit CProxyElement_locality_base_(const ArrayElement* elt)
      : CProxyElement_ArrayElement(elt) {}

  explicit CProxyElement_locality_base_(const CkArrayID& aid,
                                        const CkArrayIndex& idx,
                                        CK_DELCTOR_PARAM)
      : CProxyElement_ArrayElement(aid, idx, CK_DELCTOR_ARGS) {}

  CProxyElement_locality_base_(const CkArrayID& aid, const CkArrayIndex& idx)
      : CProxyElement_ArrayElement(aid, idx) {}

  CProxyElement_locality_base_(const CProxyElement_locality_base_& other)
      : CProxyElement_locality_base_(other.ckGetArrayID(), other.ckGetIndex()) {
  }

  inline hash_code hash(void) const {
    return hash_combine(utilities::hash<CkGroupID>()(this->ckGetArrayID()),
                        utilities::hash<CkArrayIndex>()(this->ckGetIndex()));
  }

  inline bool operator==(const CProxyElement_locality_base_& other) {
    return (this->ckGetArrayID() == other.ckGetArrayID()) &&
           (this->ckGetIndex() == other.ckGetIndex());
  }

  inline operator bool(void) const { return !(this->ckGetArrayID().isZero()); }

  inline operator std::string(void) const {
    std::stringstream ss;
    ss << "vil(";
    ss << "aid=" << ((CkGroupID)this->ckGetArrayID()).idx << ",";
    ss << "idx=" << utilities::idx2str(this->ckGetIndex());
    ss << ")";
    return ss.str();
  }
};

namespace core {
void initialize(void);
}  // namespace core
}  // namespace hypercomm

#endif
