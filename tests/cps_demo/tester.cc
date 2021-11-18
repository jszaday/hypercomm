#include "tester.hh"

auto mbox_port = std::make_shared<hypercomm::persistent_port>(0);

void enroll_polymorphs(void) {
  // convenience function for calling ALL initialization
  hypercomm::init_polymorph_registry();

  if (CkMyRank() == 0) {
    hypercomm::enroll<hypercomm::persistent_port>();
  }
}

struct tester_main : public CBase_tester_main {
  tester_main(CkArgMsg*) {
    auto val = hypercomm::make_typed_value<int>(0);
    auto idx = hypercomm::conv2idx<CkArrayIndex>(0);
    auto nElts = CkNumPes() * 8;
    auto max = sizeof(int) * 8 - 1;
    if (nElts > max) nElts = max;
    auto arr =
        (CProxy_accumulator_chare)CProxy_accumulator_chare::ckNew(nElts, nElts);

    // send the initial message to kickstart the process
    hypercomm::interceptor::send_async(arr[idx], mbox_port, std::move(val));

    CkExitAfterQuiescence();
  }
};

static constexpr auto sumOff = 0;
static constexpr auto iOff = sumOff + sizeof(int);
using server_type = state_server_<varstack>;

// 1 input port (string) and 0 outputs
struct accumulator_com
    : public hypercomm::multistate_component<int, std::tuple<>> {
  using parent_t =
      hypercomm::multistate_component<int, std::tuple<>>;
  using in_set_t = typename parent_t::in_set;

  accumulator_com(hypercomm::component_id_t id,
                  const std::shared_ptr<server_type>& server) : parent_t(id) {
    this->set_server(server);
  }

  virtual std::tuple<> action(in_set_t& in_set) override {
    auto* ctx = hypercomm::access_context_();
    auto& val = std::get<0>(in_set)->value();
    auto& stk = this->get_state();

    auto idx = hypercomm::utilities::idx2str(ctx->ckGetArrayIndex());
    CkPrintf("com%d@vil%s> received value %d in iteration %d.\n", this->id,
             idx.c_str(), val, stk->at<int>(iOff));

    stk->at<int>(sumOff) += val;

    return {};
  };
};

struct accumulator_chare : public hypercomm::vil<CBase_accumulator_chare, int> {
  hypercomm::comproxy<hypercomm::mailbox<int>> mbox;
  int nElts;

  accumulator_chare(int n)
      : mbox(this->emplace_component<hypercomm::mailbox<int>>()), nElts(n) {
    this->connect(mbox_port, this->mbox, 0);
    this->activate_component(this->mbox);
    this->accumulate();
  }

  void accumulate(void) {
    auto mine = this->__index__();
    auto nExpected = mine ? mine : 1;
    std::shared_ptr<varstack> top(
      varstack::make_stack(sizeof(int))
    );
    // set up initial stack state
    top->at<int>(sumOff) = mine + 1;
    // create a server for managing state
    auto srv = std::make_shared<server_type>();
    auto com = this->emplace_component<accumulator_com>(srv);
    // setup a listener to propagate sum after all iters finish
    com->add_listener(listener_fn_, 
      new std::shared_ptr<varstack>(top)
    );
    // bring component online to start handling iters
    this->activate_component(com);
    // forall [i] (0:(nExpected - 1),1)
    for (auto i = 0; i < nExpected; i++) {
      // when receive_msg(int sum) =>
      // set value of i in child's stack
      auto* stk = varstack::make_stack(top, sizeof(int));
      stk->at<int>(iOff) = i;
      srv->put_state(i, stk);
      // make a remote request to it
      this->mbox->put_request_to({}, com->id, i);
    }
    // indicate that no more states will be added
    srv->done_inserting();
  }

  static void listener_fn_(const hypercomm::components::base_*,
                           hypercomm::components::status_, void* arg) {
    auto* self = (accumulator_chare*)hypercomm::access_context_();
    auto mine = self->__index__();

    auto* stk = (std::shared_ptr<varstack>*)arg;
    auto& sum = (*stk)->at<int>(sumOff);
    auto val = hypercomm::make_typed_value<int>(sum);

    if (mine == (self->nElts - 1)) {
      CkPrintf("vil%d> all done, total sum is %d.\n", mine, sum);
      CkEnforceMsg(sum == ((1L << self->nElts) - 1));
    } else {
      // send a message to all the remaining chares
      for (auto i = (mine + 1); i < self->nElts; i++) {
        auto idx = hypercomm::conv2idx<CkArrayIndex>(i);
        hypercomm::deliverable dev(val->as_message());
        hypercomm::interceptor::send_async(self->ckGetArrayID(), idx, mbox_port, std::move(dev));
      }
    }

    delete stk;
  }
};

#define CK_TEMPLATES_ONLY
#include "tester.def.h"
#undef CK_TEMPLATES_ONLY

#include "tester.def.h"
