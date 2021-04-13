#include "manager.hpp"
#include "tester.decl.h"

void setup_tester(void) { initialize(); }

struct spontaneous : public independent_component {
  spontaneous(const int& _1) : value(_1) {}

  virtual std::shared_ptr<CkMessage> action(void) override {
    CkPrintf("%lu@%p> i'm so spontaneous~!\n", this->id, CthSelf());
    auto msg = CkAllocateMarshallMsg(sizeof(int));
    auto& val = *(reinterpret_cast<int*>(msg->msgBuf));
    val = value;
    return std::shared_ptr<CkMessage>(msg,
                                      [](CkMessage* msg) { CkFreeMsg(msg); });
  }

 private:
  int value;
};

struct dependent : public monovalue_component, public threaded_component {
  virtual std::shared_ptr<CkMessage> action(void) override {
    auto msg = std::move(this->buffer.front());
    auto msg_typed = static_cast<CkMarshallMsg*>(msg.get());
    auto& val = *(reinterpret_cast<int*>(msg_typed->msgBuf));
    CkPrintf("%lu@%p> i got my value, it's %d~!\n", this->id, CthSelf(), val);
    this->buffer.clear();
    return msg;
  }
};

struct selector : public demux_component {
  virtual id_t route(const std::shared_ptr<CkMessage>& msg) const {
    auto msg_typed = static_cast<CkMarshallMsg*>(msg.get());
    auto& val = *(reinterpret_cast<int*>(msg_typed->msgBuf));
    return this->outgoing[val % (this->outgoing.size())];
  }
};

struct tester : public CBase_tester {
  void test_sequence_a(const int& value) {
    auto first = std::make_shared<spontaneous>(value);
    set_identity(first->id);
    auto second = std::make_shared<selector>();
    set_identity(second->id);

    auto third = std::make_shared<dependent>();
    set_identity(third->id);
    auto fourth = std::make_shared<dependent>();
    set_identity(fourth->id);

    component::connect(first, second);
    component::connect(second, third);
    component::connect(second, fourth);

    emplace_component(std::move(fourth));
    emplace_component(std::move(third));
    emplace_component(std::move(second));
    emplace_component(std::move(first));
  }

  tester(CkArgMsg* msg) {
    test_sequence_a(42);
    test_sequence_a(21);
    CkExitAfterQuiescence();
  }
};

#include "tester.def.h"