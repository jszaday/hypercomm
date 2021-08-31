#include <hypercomm/core/locality.hpp>
#include <hypercomm/core/resuming_callback.hpp>

#include "tester.decl.h"

using namespace hypercomm;

const auto setup_environment = core::initialize;

struct main : public CBase_main {
  main(CkArgMsg* m) { make_grouplike<CProxy_locality>().run(); }
};

struct locality : public vil<CBase_locality, int> {
  locality(void) = default;

  void run(void) {
    auto f = this->make_future();
    auto value = hypercomm::make_unit_value();
    f.set(std::move(value));
    while (!f.ready()) CthYield();
    this->contribute(CkCallback(CkCallback::ckExit));
  }
};

#include "tester.def.h"
