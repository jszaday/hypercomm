#include "tester.hh"

#include <charm++.h>

#include <cstdlib>
#include <ctime>

void enroll_polymorphs(void) { hypercomm::init_polymorph_registry(); }

/* readonly */ int nElts;
/* readonly */ int nIters;
/* readonly */ int nChares;
/* readonly */ CProxy_main mainProxy;

struct PingMsg : public CMessage_PingMsg {
  char* payload;
};

struct main : public CBase_main {
  CProxy_exchanger exchangers;
  bool warmup;

  main(CkArgMsg* m) : warmup(true) {
    nIters = (m->argc >= 2) ? atoi(m->argv[1]) : 128;
    nElts = (m->argc >= 3) ? atoi(m->argv[2]) : 4096;
    mainProxy = thisProxy;
    nChares = 2;

    CkPrintf("main> pingpong with %d iterations and %dB payload\n", nIters,
             nElts);

    CkCallback cb(CkIndex_main::start(nullptr), thisProxy);
    CProxy_exchanger::ckNew(nChares, cb);
  }

  void start(CkArrayCreatedMsg* msg) {
    new (&exchangers) CProxy_exchanger(msg->aid);

    exchangers[CkArrayIndex1D(0)].run_plain();
  }

  void done(std::string&& str, const double& totalTime) {
    if (!warmup) {
      auto tIter = (nChares == 1) ? (nIters / 2) : nIters;
      CkPrintf("main> roundtrip time for %s was %g us\n", str.c_str(),
               (1e6 * totalTime) / (double)tIter);
    }

    if (str == "plain") {
      exchangers.run_sdag();
    } else if (str == "sdag") {
      exchangers[CkArrayIndex1D(0)].run_intercept();
    } else if (warmup) {
      this->warmup = false;
      exchangers[CkArrayIndex1D(0)].run_plain();
    } else {
      // ensure there is no latent activity
      CkExitAfterQuiescence();
    }
  }
};

class exchanger : public vil<CBase_exchanger, int> {
  exchanger_SDAG_CODE;

 private:
  CProxyElement_exchanger peer;
  double startTime;
  int mine, it;

 public:
  exchanger(void) : mine(this->__index__()) {
    auto peerIdx = conv2idx<CkArrayIndex>((mine + 1) % nChares);
    peer = this->thisProxy[peerIdx];
  }

  void run_intercept(void) {
    this->it = 0;
    auto* msg = new (nElts) PingMsg;
    this->startTime = CkWallTimer();
    UsrToEnv(msg)->setEpIdx(CkIndex_exchanger::recv_intercept(nullptr));
    interceptor::send_async(peer, msg);
  }

  void recv_intercept(CkMessage* msg) {
    if ((mine == 0) && (++this->it == nIters)) {
      auto endTime = CkWallTimer();
      mainProxy.done("intercept", endTime - startTime);
      CkFreeMsg(msg);
    } else {
      UsrToEnv(msg)->setEpIdx(CkIndex_exchanger::recv_intercept(nullptr));
      interceptor::send_async(peer, msg);
    }
  }

  void run_plain(void) {
    this->it = 0;
    auto* msg = new (nElts) PingMsg;
    this->startTime = CkWallTimer();
    peer.recv_plain(msg);
  }

  void recv_plain(CkMessage* msg) {
    if ((mine == 0) && (++this->it == nIters)) {
      auto endTime = CkWallTimer();
      mainProxy.done("plain", endTime - startTime);
      CkFreeMsg(msg);
    } else {
      peer.recv_plain(msg);
    }
  }
};

#define CK_TEMPLATES_ONLY
#include "tester.def.h"
#undef CK_TEMPLATES_ONLY

#include "tester.def.h"
