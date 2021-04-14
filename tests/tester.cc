#include <hypercomm/utilities.hpp>
#include <hypercomm/components.hpp>

using namespace hypercomm::components;

#include "tester.decl.h"

void setup_tester(void) { initialize_module(); }

struct spontaneous : public n_input_component<0> {
  spontaneous(const int& _1) : value(_1) {}

  virtual std::shared_ptr<CkMessage> action(void) override {
    CkPrintf("%lx:th=%p:pe=%d> i'm so spontaneous~!\n", this->id, CthSelf(), CkMyPe());
    // Allocate a marshall message and assign the contents
    // of its buffers to our value
    auto msg = CkAllocateMarshallMsg(sizeof(int));
    auto& val = *(reinterpret_cast<int*>(msg->msgBuf));
    val = value;
    // Then wrap it as a shared pointer and send it
    return hypercomm::utilities::wrap_message(msg);
  }

 private:
  int value;
};

// this component runs inside its own thread, and expects one input
struct dependent : public threaded_component, public n_input_component<1> {
  virtual std::shared_ptr<CkMessage> action(void) override {
    // take ownership of our input value from the buffer
    auto msg = std::move(this->accepted.front());
    this->accepted.clear();
    // retrieve the message's contents
    auto msg_typed = static_cast<CkMarshallMsg*>(msg.get());
    auto& val = *(reinterpret_cast<int*>(msg_typed->msgBuf));
    CkPrintf("%lx:th=%p:pe=%d> i got my value, it's %d~!\n", this->id, CthSelf(), CkMyPe(), val);
    // pass the value along
    return msg;
  }
};

struct selector : public demux_component {
  // a demux component implements the "route" method to selection which
  // of its output ports an incoming message should be sent to
  virtual id_t route(const std::shared_ptr<CkMessage>& msg) const override {
    auto msg_typed = static_cast<CkMarshallMsg*>(msg.get());
    auto& val = *(reinterpret_cast<int*>(msg_typed->msgBuf));
    return this->outgoing[val % (this->outgoing.size())];
  }
};

struct gatherer : public n_input_component<2> {
  virtual std::shared_ptr<CkMessage> action(void) override {
    // sum up all the accepted values (2)
    int sum = 0;
    for (auto& msg : this->accepted) {
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
    // make a gatherer with an identity
    auto g = std::make_shared<gatherer>();
    component::generate_identity(g->id);
    // connect its inputs to the placeholders
    component::connect(a, g);
    component::connect(b, g);
    // then activate it
    component::activate(std::move(g));
  }
};

struct tester : public CBase_tester {
  placeholder test_sequence(const int& value) {
    auto first = std::make_shared<spontaneous>(value);
    component::generate_identity(first->id);

    auto second = std::make_shared<selector>();
    component::generate_identity(second->id);

    auto third = std::make_shared<dependent>();
    component::generate_identity(third->id);

    auto fourth = std::make_shared<dependent>();
    component::generate_identity(fourth->id);

    // connect the input/output ports of the
    // components to one another
    component::connect(first, second);
    component::connect(second, third);
    component::connect(second, fourth);

    placeholder next{};
    if (value % 2 == 0) {
      // a component must have all ports accounted for before
      // activation, so we use placeholders to indicate that
      // it should buffer its output value
      next = third->put_placeholder(OUTPUT);
      component::activate(std::move(fourth));
      component::activate(std::move(third));
      component::activate(std::move(second));
      component::activate(std::move(first));
    } else {
      next = fourth->put_placeholder(OUTPUT);
      component::activate(std::move(first));
      component::activate(std::move(second));
      component::activate(std::move(third));
      component::activate(std::move(fourth));
    }

    return next;
  }

  tester(CkArgMsg* msg) {
    // fire the test sequence with an even/odd
    // number to test both orderings
    auto a = test_sequence(42);
    auto b = test_sequence(21);
    // create a remote chare, and connect it to the
    // sequences' outputs
    CProxy_other::ckNew(a, b, (CkMyPe() + 1) % CkNumPes());
    // await qd then exit
    CkExitAfterQuiescence();
  }
};

#include "tester.def.h"