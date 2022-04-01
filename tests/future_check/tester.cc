#include <hypercomm/core/locality.hpp>
#include <hypercomm/core/resuming_callback.hpp>
#include <hypercomm/components/link.hpp>

#include "tester.decl.h"

using namespace hypercomm;

const auto setup_environment = core::initialize;

struct plus: public immediate_action<void(int, int)> {
  plus(void) {}
  plus(tags::reconstruct) {}

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

  void microcheck(void) {
    auto hi = hypercomm::link(32);
    auto mi = hypercomm::link(hi, 64);
    auto lo = hypercomm::link(mi, 128);

    CkEnforce(lo->get<0>() == 32);
    CkEnforce(lo->get<1>() == 64);
    CkEnforce(lo->get<2>() == 128);
    
    CkEnforce(mi == lo->unwind());
    CkEnforce(hi == mi->unwind());
    CkEnforce(!hi->unwind());

    auto lolo = lo->clone();
    lolo->get<2>() += 1;
    CkEnforce(lo->get<2>() == (lolo->get<2>() - 1));
    CkEnforce(mi == lolo->unwind());
  }

  void run(void) {
    auto f = this->make_future();
    auto g = this->make_future();

    using test_type = std::shared_ptr<int>;
    test_type t(new test_type::element_type(42));
    auto value = make_typed_value<std::tuple<test_type, test_type>>(t, t);
    f.set(deliverable(value->as_message()));

    do {
      CthYield();
      this->update_context();
    } while (!f.ready());

    auto list = { f, g };
    auto pair = wait_any(std::begin(list), std::end(list));
    CkEnforce(f.equals(*pair.second));

    value = dev2typed<std::tuple<test_type, test_type>>(std::move(pair.first));
    CkEnforce((bool)value);
    CkEnforce(t != std::get<0>(value->value()));
    CkEnforce(std::get<0>(value->value()) == std::get<1>(value->value()));

    this->microcheck();

    this->contribute(CkCallback(CkCallback::ckExit));
  }
};

#include "tester.def.h"
