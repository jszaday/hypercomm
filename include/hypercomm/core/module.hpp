#ifndef __HYPERCOMM_CORE_ENTRY_METHODS_HPP__
#define __HYPERCOMM_CORE_ENTRY_METHODS_HPP__

#include "common.hpp"
#include "../messaging/messaging.hpp"

namespace hypercomm {

class CkIndex_locality_base_ {
 public:
  static int __idx;
  static const int& idx_demux_CkMessage(void);
  static const int& idx_execute_CkMessage(void);
};

// NOTE ( hypercomm claims absolute control over these )
using CProxy_locality_base_ = CProxy_ArrayBase;
using CProxyElement_locality_base_ = CProxyElement_ArrayElement;
using CProxySection_locality_base_ = CProxySection_ArrayElement;

namespace core {
void initialize(void);
}  // namespace core

class locality_base_: public ArrayElement {
 public:
  using base_index_type = CkArrayIndex;

  virtual void demux(hypercomm_msg* msg) = 0;
  virtual void execute(CkMessage* msg) = 0;

  inline const CkArrayIndex& __base_index__(void) const {
    return this->ckGetArrayIndex();
  }

  inline collective_ptr<CkArrayIndex> __proxy__(void) const {
    return std::make_shared<array_proxy>(
        CProxy_ArrayBase(this->ckGetArrayID()));
  }

  inline element_ptr<CkArrayIndex> __element__(void) const {
    return std::make_shared<array_element_proxy>(
        CProxyElement_ArrayElement(this->ckGetArrayID(), this->ckGetArrayIndex()));
  }
};

}  // namespace hypercomm

#endif
