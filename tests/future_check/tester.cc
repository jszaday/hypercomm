#include <hypercomm/core/locality.hpp>
#include <hypercomm/core/resuming_callback.hpp>

#include "tester.decl.h"

using namespace hypercomm;

const auto setup_environment = core::initialize;

struct plus: public immediate_action<void(int, int)> {
  plus(void) {}
  plus(PUP::reconstruct) {}

  virtual void action(int i, int j) override {
    CkPrintf("%d + %d = %d\n", i, j, i + j);
  }

  virtual void __pup__(hypercomm::serdes&) override {}
};

struct main : public CBase_main {
  main(CkArgMsg* m) { this->check_action(); }

  void check_action(void) {
    auto p = std::make_shared<plus>();
    auto cb = std::static_pointer_cast<callback>(std::move(p));
    auto t = make_typed_value<std::tuple<int, int>>(20, 22);

    CkPrintf("checking typed callbacks: ");
    cb->send(deliverable(std::move(t)));

    make_grouplike<CProxy_locality>().run();
  }
};

struct locality : public vil<CBase_locality, int> {
  locality(void) = default;

  void run(void) {
    auto f = this->make_future();
    auto g = this->make_future();

    using test_type = std::shared_ptr<int>;
    test_type t(new test_type::element_type(42));
    auto value = make_typed_value<std::tuple<test_type, test_type>>(t, t);
    f.set(msg2value(value->release()));

    do {
      CthYield();
      this->update_context();
    } while (!f.ready());

    auto list = { f, g };
    auto pair = wait_any(std::begin(list), std::end(list));
    CkEnforce(f.equals(*pair.second));

    value = value2typed<std::tuple<test_type, test_type>>(std::move(pair.first));
    CkEnforce(t != std::get<0>(value->value()));
    CkEnforce(std::get<0>(value->value()) == std::get<1>(value->value()));

    this->contribute(CkCallback(CkCallback::ckExit));
  }
};

#include "tester.def.h"
