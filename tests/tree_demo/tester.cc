#include <hypercomm/core/locality.hpp>
#include <hypercomm/core/typed_value.hpp>
#include <hypercomm/core/inter_callback.hpp>
#include <hypercomm/tree_builder/tree_builder.hpp>

#include "tester.decl.h"

constexpr int kMultiplier = 2;

/* readonly */ int numElements;
/* readonly */ CProxy_Main mainProxy;
/* readonly */ CProxy_tree_builder locProxy;

namespace ck {
  inline const std::shared_ptr<imprintable<int>>& span_all(void) {
    return managed_imprintable<int>::instance();
  }
}

void enroll_polymorphs(void) {
  hypercomm::tree_builder::initialize();
  hypercomm::init_polymorph_registry();

  if (CkMyRank() == 0) {
    hypercomm::enroll<managed_imprintable<int>>();
    hypercomm::enroll<reduction_port<int>>();
  }
}

template <typename T>
class adder : public core::combiner {
  virtual combiner::return_type send(combiner::argument_type&& args) override {
    if (args.empty()) {
      return {};
    } else {
      auto accum = value2typed<T>(std::move(args.back()));
      args.pop_back();
      for (auto& arg : args) {
        auto typed = value2typed<T>(std::move(arg));
        accum->value() += typed->value();
      }
      return accum;
    }
  }

  virtual void __pup__(hypercomm::serdes& s) override {}
};

class Test : public manageable<vil<CBase_Test, int>> {
 public:
  Test(void) = default;

  Test(association_ptr_&& ptr, const reduction_id_t& seed)
      : manageable(std::forward<association_ptr_>(ptr), seed) {
    auto& mine = this->__index__();
    CkAssertMsg(mine % 2 != 0, "expected an odd index");
    this->make_contribution();
  }

  void make_contribution(void) {
    this->update_context();

    auto& mine = this->__index__();
    if (mine % 2 == 0) {
      auto next = conv2idx<CkArrayIndex>(mine + numElements + 1);
      auto child = locProxy.ckLocalBranch()->create_child(this, next);
      thisProxy[next].insert(child.first, std::get<1>(child.second));
    } else if (mine < numElements) {
      thisProxy[conv2idx<CkArrayIndex>(mine)].ckDestroy();
      return;
    }

    auto val = std::make_shared<typed_value<int>>(mine);
    auto fn = std::make_shared<adder<int>>();
    auto cb = CkCallback(CkIndex_Main::done(nullptr), mainProxy);
    auto icb = std::make_shared<inter_callback>(cb);
    this->local_contribution(ck::span_all(), std::move(val), fn, icb);
  }
};

class Main : public CBase_Main {
  CProxy_Test testProxy;

 public:
  Main(CkArgMsg* msg) {
    int mult =
        (msg->argc >= 2) ? atoi(msg->argv[1])
                         : kMultiplier;
    mainProxy = thisProxy;
    testProxy = CProxy_Test::ckNew();
    locProxy = CProxy_tree_builder::ckNew();
    numElements = mult * CkNumPes();

    // each array requires its own completion detector for static
    // insertions, ensuring the spanning tree is ready before completion
    locProxy.reg_array(CProxy_CompletionDetector::ckNew(), testProxy,
                       CkCallback(CkIndex_Main::run(), thisProxy));
  }

  void run(void) {
    // initiate a phase of static insertion, suspending the thread
    // and resuming when we can begin. insertions are allowed when
    // all nodes have called "done_inserting"
    locProxy.begin_inserting(
        testProxy, CkCallbackResumeThread(),
        CkCallback(CkIndex_Test::make_contribution(), testProxy));

    for (auto i = 0; i < numElements; i += 1) {
      testProxy[conv2idx<CkArrayIndex>(i)].insert();
    }

    locProxy.done_inserting(testProxy);
  }

  inline int expected(void) const {
    auto sum = 0;
    for (auto i = 0; i < numElements; i += 1) {
      if (i % 2 == 0) {
        sum += 2 * i + numElements + 1;
      }
    }
    return sum;
  }

  void done(CkMessage* msg) {
    auto value = std::make_shared<typed_value<int>>(msg);
    auto expected = this->expected();

    if (value->value() != expected) {
      CkAbort("fatal> got value %d from reduction (expected %d).\n",
              value->value(), expected);
    } else {
      CkPrintf("info> got value %d from reduction.\n", value->value());
    }

    CkExit();
  }
};

#include "tester.def.h"
