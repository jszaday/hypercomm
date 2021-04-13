#include "manager.hpp"
#include "tester.decl.h"

void setup_tester(void) {
    initialize();
}

struct spontaneous_component: public component {
  virtual std::shared_ptr<CkMessage> action(void) override {
      CkPrintf("%lu> i'm so spontaneous~!\n", this->id);

      return {};
  }

  virtual int num_expected(void) const override { return 0; }
};

struct chained_component: public monovalue_component {
  virtual std::shared_ptr<CkMessage> action(void) override {
        CkPrintf("%lu> i got my value~!\n", this->id);
    
        return {};
  }
};

struct tester: public CBase_tester {
    tester(CkArgMsg* msg) {
        auto first = std::make_shared<spontaneous_component>();
        set_identity(first->id);
        auto second = std::make_shared<chained_component>();
        set_identity(second->id);
        component::connect(first, second);
        emplace_component(std::move(first));
        emplace_component(std::move(second));
    }
};

#include "tester.def.h"