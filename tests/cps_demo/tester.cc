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

// 1 input port (int) and 0 outputs
struct accumulator_com
    : public hypercomm::multistate_component<int, std::tuple<>> {
  using parent_t = hypercomm::multistate_component<int, std::tuple<>>;
  using in_set_t = typename parent_t::in_set;

  accumulator_com(hypercomm::component_id_t id,
                  const std::shared_ptr<server_type>& server)
      : parent_t(id) {
    this->set_server(server);
  }

  virtual std::tuple<> action(in_set_t& in_set) override {
    auto* ctx = hypercomm::access_context_();
    auto& val = std::get<0>(in_set)->value();
    auto& stk = this->get_state();

    auto idx = hypercomm::utilities::idx2str(ctx->ckGetArrayIndex());
    CkPrintf("com%lu@vil%s> received value %d in iteration %d.\n", this->id,
             idx.c_str(), val, stk->at<int>(iOff));

    stk->at<int>(sumOff) += val;

    return {};
  };
};

struct accumulator_chare : public hypercomm::vil<CBase_accumulator_chare, int> {
  using pair_type =
      std::pair<hypercomm::component_id_t, std::shared_ptr<server_type>>;

  hypercomm::comproxy<hypercomm::mailbox<int>> mbox;
  std::shared_ptr<screener<int>> rfilt;

  int nElts;
  bool inv;

  accumulator_chare(int n)
      : mbox(this->emplace_component<hypercomm::mailbox<int>>()),
        nElts(n),
        inv(false) {
    this->connect(mbox_port, this->mbox, 0);
    this->activate_component(this->mbox);
    this->accumulate();
  }

  ~accumulator_chare() {
    // a component should've been inv'd
    CkEnforce(this->inv);
    // only the mailbox should be left
    auto nCom = this->components.size();
    if (nCom != 1) {
      CkAbort("vil%d> expected one component, instead got %lu.\n",
              this->__index__(), nCom);
    }
  }

  void accumulate(void) {
    auto mine = this->__index__();
    auto nExpected = mine ? mine : 1;
    std::shared_ptr<hypercomm::varstack> top(
        hypercomm::varstack::make_stack(sizeof(int)));
    // set up initial stack state
    top->at<int>(sumOff) = mine + 1;
    // create a server for managing state
    auto srv = std::make_shared<server_type>();
    auto com = this->emplace_component<accumulator_com>(srv);
    // setup a listener to propagate sum after all iters finish
    com->add_listener(listener_fn_,
                      new std::shared_ptr<hypercomm::varstack>(top));
    // bring component online to start handling iters
    this->activate_component(com);
    // forall [i] (0:(nExpected - 1),1)
    for (auto i = 0; i < nExpected; i++) {
      // when receive_msg(int sum) =>
      // set value of i in child's stack
      auto* stk = hypercomm::varstack::make_stack(top, sizeof(int));
      stk->at<int>(iOff) = i;
      srv->put_state(i, stk);
      // make a remote request to it
      this->mbox->put_request_to({}, com->id, i);
    }
    // indicate that no more states will be added
    srv->done_inserting();
  }

  /* loosely speaking... this implements
   * an sdag-like flow:
   *
   *    val init = true;
   *    await any {
   *        when receive(_ == left) => {
   *            init = false;
   *            send to left;
   *        }
   *        when receive(_ == right) => {
   *            ck::abort(...);
   *        }
   *    }
   *    when receive(_ == right) => {
   *        ck::assert(!init);
   *        send to right;
   *    }
   */

  void accumulate_two(void) {
    // update context on entering EP
    this->update_context();
    // make left/right determinations
    auto mine = this->__index__();
    auto left = (mine + nElts - 1) % nElts;
    auto right = (mine + 1) % nElts;
    this->rfilt = std::make_shared<screener<int>>(right);
    // create two multistate components and a server
    auto srv1 = std::make_shared<server_type>();
    auto com1 = this->emplace_component<accumulator_two_com>(srv1);
    auto com2 = this->emplace_component<accumulator_two_com>(srv1);
    // create a stack for receiving values
    auto* stk = hypercomm::varstack::make_stack(sizeof(int) * 2 + sizeof(bool));
    stk->at<int>(0) = mine;
    stk->at<int>(sizeof(int)) = nElts;
    stk->at<bool>(sizeof(int) * 2) = true;
    srv1->put_state(0, stk);
    srv1->done_inserting();  // IMPORTANT!
    // ensure the component is invalidated
    auto* arg = &(this->inv);
    com2->add_listener(on_invalidation_, arg);
    auto lfilt = std::make_shared<screener<int>>(left);
    // create a secondary server to receive continuations
    // ( create the continuation BEFORE its state can be consumed! )
    // ( otherwise, there will be no trace of it! )
    auto srv2 = std::make_shared<server_type>();
    auto com3 = this->emplace_component<accumulator_two_com>(srv2);
    srv1->put_continuation(0, on_continuation_, new pair_type(com3->id, srv2));
    // THEN activate the components ( no possibility of consumption 'til now )
    this->activate_component(com1);
    this->activate_component(com2);
    // the request to rfilt goes first since
    // there's a SLIM chance that it'll get inv'd
    // if the mbox has a value ready for com1
    this->mbox->put_request_to(this->rfilt, com2->id, 0);
    this->mbox->put_request_to(lfilt, com1->id, 0);
    // kickoff the sequence
    if (mine == 0) {
      auto val = hypercomm::make_typed_value<int>(mine);
      hypercomm::interceptor::send_async(
          this->ckGetArrayID(), hypercomm::conv2idx<CkArrayIndex>(right),
          mbox_port, std::move(val));
    }
  }

  static void on_continuation_(void* arg,
                               typename server_type::state_type&& state) {
    auto* self = (accumulator_chare*)hypercomm::access_context_();
    auto* pair = (pair_type*)arg;
    // pass the state along to the next server
    pair->second->put_state(std::move(state));
    pair->second->done_inserting();
    // then activate the component
    self->activate_component(pair->first);
    // then make a value request
    self->mbox->put_request_to(std::move(self->rfilt), pair->first, 0);
    delete pair;
  }

  static void on_invalidation_(const hypercomm::components::base_* com,
                               hypercomm::components::status_ status,
                               void* arg) {
    auto& inv = *((bool*)arg);
    inv = inv || (status == hypercomm::components::kInvalidation);
  }

  static void listener_fn_(const hypercomm::components::base_*,
                           hypercomm::components::status_, void* arg) {
    auto* self = (accumulator_chare*)hypercomm::access_context_();
    auto mine = self->__index__();

    auto* stk = (std::shared_ptr<hypercomm::varstack>*)arg;
    auto& sum = (*stk)->at<int>(sumOff);
    auto val = hypercomm::make_typed_value<int>(sum);

    if (mine == (self->nElts - 1)) {
      CkPrintf("vil%d> all done, total sum is %d.\n", mine, sum);
      CkEnforceMsg(sum == ((1L << self->nElts) - 1));
      self->thisProxy.accumulate_two();
    } else {
      // send a message to all the remaining chares
      for (auto i = (mine + 1); i < self->nElts; i++) {
        auto idx = hypercomm::conv2idx<CkArrayIndex>(i);
        hypercomm::deliverable dev(val->as_message());
        hypercomm::interceptor::send_async(self->ckGetArrayID(), idx, mbox_port,
                                           std::move(dev));
      }
    }

    delete stk;
  }
};

std::tuple<> accumulator_two_com::action(
    accumulator_two_com::in_set_t& in_set) {
  auto* ctx = (accumulator_chare*)hypercomm::access_context_();
  auto& aid = ctx->ckGetArrayID();
  auto& tru = std::get<0>(in_set);
  auto& val = tru->value();
  auto& stk = this->get_state();
  auto& mine = stk->at<int>(0);
  auto& nElts = stk->at<int>(sizeof(int));
  auto& init = stk->at<bool>(sizeof(int) * 2);

  auto left = (mine + nElts - 1) % nElts;
  auto right = (mine + 1) % nElts;

  CkPrintf("com%lu@vil%d> received value: %d\n", this->id, mine, val);

  int dst;
  if (init) {
    CkEnforce(val == left);
    init = false;
    dst = (mine == 0) ? left : right;
  } else {
    CkEnforce(val == right);
    dst = (mine == 0) ? -1 : left;
    // send ck destroy as message so it's async
    // ( destructor isn't called otherwise )
    ctx->thisProxy[ctx->thisIndexMax].ckDestroy();
  }

  if (dst >= 0) {
    val = mine;
    hypercomm::interceptor::send_async(
        aid, hypercomm::conv2idx<CkArrayIndex>(dst), mbox_port, std::move(tru));
  }

  return {};
}

#define CK_TEMPLATES_ONLY
#include "tester.def.h"
#undef CK_TEMPLATES_ONLY

#include "tester.def.h"
