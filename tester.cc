#include "manager.hpp"
#include "tester.decl.h"

void setup_tester(void) {
    initialize();
}

struct spontaneous: public independent_component {
  virtual std::shared_ptr<CkMessage> action(void) override {
      CkPrintf("%lu@%p> i'm so spontaneous~!\n", this->id, CthSelf());
      auto msg = CkAllocateMarshallMsg(sizeof(int));
      auto& val = *(reinterpret_cast<int*>(msg->msgBuf));
      val = 42;
      return std::shared_ptr<CkMessage>(msg, [](CkMessage* msg) {
        CkFreeMsg(msg);
      });
  }
};

struct dependent: public monovalue_component, public threaded_component {
  virtual std::shared_ptr<CkMessage> action(void) override {
    auto msg = this->buffer.front();
    auto msg_typed = static_cast<CkMarshallMsg*>(msg.get());
    auto& val = *(reinterpret_cast<int*>(msg_typed->msgBuf));
    CkPrintf("%lu@%p> i got my value, it's %d~!\n", this->id, CthSelf(), val);
    return {};
  }
};

struct tester: public CBase_tester {
    tester(CkArgMsg* msg) {
        auto first = std::make_shared<spontaneous>();
        set_identity(first->id);
        auto second = std::make_shared<dependent>();
        set_identity(second->id);
        component::connect(first, second);
        emplace_component(std::move(first));
        emplace_component(std::move(second));
        CkExitAfterQuiescence();
    }
};

#include "tester.def.h"