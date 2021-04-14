#include "manager.hpp"
#include "tester.decl.h"

void setup_tester(void) { initialize(); }

struct spontaneous : public independent_component {
  spontaneous(const int& _1) : value(_1) {}

  virtual std::shared_ptr<CkMessage> action(void) override {
    CkPrintf("%lx:th=%p:pe=%d> i'm so spontaneous~!\n", this->id, CthSelf(), CkMyPe());
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
    auto msg = std::move(this->accepted.front());
    auto msg_typed = static_cast<CkMarshallMsg*>(msg.get());
    auto& val = *(reinterpret_cast<int*>(msg_typed->msgBuf));
    CkPrintf("%lx:th=%p:pe=%d> i got my value, it's %d~!\n", this->id, CthSelf(), CkMyPe(), val);
    this->accepted.clear();
    return msg;
  }
};

struct selector : public demux_component {
  virtual id_t route(const std::shared_ptr<CkMessage>& msg) const override {
    auto msg_typed = static_cast<CkMarshallMsg*>(msg.get());
    auto& val = *(reinterpret_cast<int*>(msg_typed->msgBuf));
    return this->outgoing[val % (this->outgoing.size())];
  }
};

struct gatherer : public component {
  virtual int num_expected(void) const override { return 2; }
  virtual std::shared_ptr<CkMessage> action(void) override {
    int sum = 0;
    for (auto& msg : accepted) {
      auto msg_typed = static_cast<CkMarshallMsg*>(msg.get());
      auto& val = *(reinterpret_cast<int*>(msg_typed->msgBuf));
      sum += val;
    }
    CkPrintf("%lx:th=%p:pe=%d> i got my value, it's %d~!\n", this->id, CthSelf(), CkMyPe(), sum);
    return {};
  }
};

struct other : public CBase_other {
  other(const placeholder& a, const placeholder& b) {
    auto g = std::make_shared<gatherer>();
    set_identity(g->id);

    component::connect(a, g);
    component::connect(b, g);

    emplace_component(std::move(g));
  }
};

struct tester : public CBase_tester {
  placeholder test_sequence_a(const int& value) {
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
    placeholder next{};

    if (value % 2 == 0) {
      next = third->put_placeholder(false);
      emplace_component(std::move(fourth));
      emplace_component(std::move(third));
      emplace_component(std::move(second));
      emplace_component(std::move(first));
    } else {
      next = fourth->put_placeholder(false);
      emplace_component(std::move(first));
      emplace_component(std::move(second));
      emplace_component(std::move(third));
      emplace_component(std::move(fourth));
    }

    return next;
  }

  tester(CkArgMsg* msg) {
    auto a = test_sequence_a(42);
    auto b = test_sequence_a(21);
    CProxy_other::ckNew(a, b, (CkMyPe() + 1) % CkNumPes());
    CkExitAfterQuiescence();
  }
};

#include "tester.def.h"