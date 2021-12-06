#include <algorithm>
#include "tester.hh"

CProxy_test_main mainProxy;

void enroll_polymorphs(void) {
  // convenience function for calling ALL initialization
  hypercomm::init_polymorph_registry();

  if (CkMyRank() == 0) {
    hypercomm::enroll<hypercomm::persistent_port>();
  }
}

enum test_phase_ : std::size_t {
  kSdag = 0,
  kConventional = 1,
  kMultistate = 2,
  kDone = 3
};

struct test_main : public CBase_test_main {
  test_main_SDAG_CODE;

  CProxy_test_chare testProxy;

  int rep;
  int nElts, nReps, nSkip;
  double startTime, totalTime;

  test_phase_ phase;

  test_main(CkArgMsg* msg) {
    auto val = hypercomm::make_typed_value<int>(0);
    auto idx = hypercomm::conv2idx<CkArrayIndex>(0);
    nElts = CkNumPes() * 8;
    nReps = (msg->argc >= 2) ? atoi(msg->argv[1]) : 513;
    nSkip = nReps / 10 + 1;
    mainProxy = thisProxy;
    testProxy = CProxy_test_chare::ckNew(nElts, nElts);
    std::string mode(msg->argv[std::min(msg->argc - 1, 2)]);
    if (mode == "multi") {
      this->phase = kMultistate;
    } else {
      this->phase = kSdag;
    }
    thisProxy.run();
  }

  inline void action(void) {
    switch (this->phase) {
      case kSdag:
        testProxy.run_sdag();
        break;
      case kConventional:
        testProxy.run_conventional();
        break;
      case kMultistate:
        testProxy.run_multistate();
        break;
      default:
        break;
    }
  }

  inline const char* current_phase(void) {
    switch (this->phase) {
      case kSdag:
        return "sdag";
      case kConventional:
        return "component-per-iter";
      case kMultistate:
        return "multistate-component";
      default:
        return "???";
    }
  }

  inline void next_phase(void) {
    reinterpret_cast<std::size_t&>(this->phase)++;

    if (this->phase == kDone) {
      for (auto i = 0; i < nElts; i++) {
        auto idx = hypercomm::conv2idx<CkArrayIndex>(i);
        testProxy[idx].ckDestroy();
      }

      CkExitAfterQuiescence();
    } else {
      thisProxy.run();
    }
  }
};

struct test_chare : public hypercomm::vil<CBase_test_chare, int> {
  test_chare_SDAG_CODE;

  hypercomm::comproxy<hypercomm::mailbox<int>> mbox;

  int i, j;
  int n, nRecvd;
  CkCallback cb;

  test_chare(int n_)
      : n(n_),
        mbox(this->emplace_component<hypercomm::mailbox<int>>()),
        cb(CkIndex_test_main::on_completion(nullptr), mainProxy) {
    this->activate_component(mbox);
  }

  ~test_chare() { CkAssert(this->components.size() == 1); }

  void run_multistate(void) {
    this->update_context();
    this->nRecvd = 0;

    auto srv = std::make_shared<server_type>();
    auto com = this->emplace_component<test_multistate_component>(srv);

    srv->reserve_states(n * n);
    com->add_listener(on_completion, (void*)kMultistate);
    this->activate_component(com);

    for (auto i = 0; i < n; i++) {
      thisProxy.recv_value(pack_value(i));

      for (auto j = 0; j < n; j++) {
        auto* stk = new hypercomm::typed_microstack<int, int>(nullptr, i, j);
        auto it = i * n + j;
        srv->put_state(it, stk);
        mbox->put_request_to({}, com, it);
      }
    }

    srv->done_inserting();
  }

  void run_conventional(void) {
    this->update_context();
    this->nRecvd = 0;

    for (auto i = 0; i < n; i++) {
      thisProxy.recv_value(pack_value(i));

      for (auto j = 0; j < n; j++) {
        auto com = this->emplace_component<test_component>();
        com->add_listener(on_completion, (void*)kConventional);
        this->activate_component(com);
        mbox->put_request_to({}, com, 0);
      }
    }
  }

  static void on_completion(const hypercomm::components::base_* com,
                            hypercomm::components::status_ status, void* arg) {
    auto* self = (test_chare*)hypercomm::access_context_();
    auto multi = arg == (void*)kMultistate;
    if (multi || (++(self->nRecvd) == (self->n * self->n))) {
      self->contribute(self->cb);
    }
  }

  void recv_value(CkMessage* msg) {
    this->update_context();
    hypercomm::deliverable dev(msg);
    auto status = mbox->accept(0, dev);
    CkEnforce(status);
  }

  inline CkMessage* pack_value(int i) { return hypercomm::pack(i); }
};

#define CK_TEMPLATES_ONLY
#include "tester.def.h"
#undef CK_TEMPLATES_ONLY

#include "tester.def.h"
