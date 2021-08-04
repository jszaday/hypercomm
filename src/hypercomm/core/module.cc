#include <hypercomm/core/module.hpp>
#include <hypercomm/core/locality_map.hpp>

#include <hypercomm/messaging/messaging.decl.h>

namespace hypercomm {

void call_demux_(CkMessage* msg, locality_base_* obj) {
  obj->demux((message*)msg);
}

void call_execute_(CkMessage* msg, locality_base_* obj) { obj->execute(msg); }

const int& CkIndex_locality_base_::idx_demux_CkMessage(void) {
  static int epIdx =
      CkRegisterEp("hypercomm::locality_base_::demux(CkMessage*)",
                   reinterpret_cast<CkCallFnPtr>(call_demux_),
                   CMessage_CkMessage::__idx, CkIndex_locality_base_::__idx, 0);
  return epIdx;
}

const int& CkIndex_locality_base_::idx_execute_CkMessage(void) {
  static int epIdx =
      CkRegisterEp("hypercomm::locality_base_::execute(CkMessage*)",
                   reinterpret_cast<CkCallFnPtr>(call_execute_),
                   CMessage_CkMessage::__idx, CkIndex_locality_base_::__idx, 0);
  return epIdx;
}

int CkIndex_locality_base_::__idx;

namespace core {
void initialize(void) {
  if (CkMyRank() != 0) {
    CkAbort("initialize cannot be called on ranks besides 0");
  }

  auto& __idx = CkIndex_locality_base_::__idx;

  __idx = CkRegisterChare("hypercomm::locality_base_", 0, TypeArray);
  CkRegisterArrayDimensions(__idx, -1);
  CkRegisterBase(__idx, CkIndex_ArrayElement::__idx);

  CkIndex_locality_base_::idx_demux_CkMessage();

  CkIndex_locality_base_::idx_execute_CkMessage();

  // register the messaging module
  _registermessaging();

  // register the locality module
  _registerlocality();
}
}
}

#include <hypercomm/core/locality.def.h>
