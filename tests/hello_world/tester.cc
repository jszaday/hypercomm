#include "tester.hh"

void enroll_polymorphs(void) {
  // convenience function for calling ALL initialization
  hypercomm::init_polymorph_registry();

  if (CkMyRank() == 0) {
    hypercomm::enroll<hypercomm::persistent_port>();
  }
}

struct hello_main : public CBase_hello_main {
  hello_main(CkArgMsg*) {
    CProxy_hello_chare::ckNew(CkNumPes() * 4, CkNumPes() * 4);
    CkExitAfterQuiescence();
  }
};

// 1 input port (string) and 0 outputs
struct hello_com : public hypercomm::component<std::string, std::tuple<>> {
  using parent_t = hypercomm::component<std::string, std::tuple<>>;

  hello_com(hypercomm::component_id_t id) : parent_t(id) {}

  using in_set_t = typename parent_t::in_set;

  virtual std::tuple<> action(in_set_t& in_set) override {
    auto val = std::move(std::get<0>(in_set));
    std::string& msg = val->value();

    CkPrintf("com%llu> got message: %s\n", this->id, msg.c_str());

    return {};
  };
};

struct hello_chare : public hypercomm::vil<CBase_hello_chare, int> {
  hello_chare(int n) {
    // this manually tracks the current chare that's executing
    // eliminated by: https://github.com/UIUC-PPL/charm/pull/3426
    // TODO ( add ifdef to detect that pr )
    this->update_context();

    auto mine = this->__index__();

    // every chare opens a port corresponding to its index
    auto my_port = std::make_shared<hypercomm::persistent_port>(mine);
    auto com = this->emplace_component<hello_com>();
    this->connect(my_port, com, 0);

    // activate the component so it can do things
    this->activate_component(com);

    // prepare a message~
    auto to_send = hypercomm::make_typed_value<std::string>(
        "hello from " + std::to_string(mine));

    // every chare sends a message to its left neighbor's port
    auto left = hypercomm::make_proxy(
        this->thisProxy[hypercomm::conv2idx<CkArrayIndex>((mine + n - 1) % n)]);
    auto their_port =
        std::make_shared<hypercomm::persistent_port>((mine + n - 1) % n);

    // send the message to our neighbor's port
    hypercomm::send2port<CkArrayIndex>(left, their_port, std::move(to_send));
  }
};

#define CK_TEMPLATES_ONLY
#include "tester.def.h"
#undef CK_TEMPLATES_ONLY

#include "tester.def.h"
