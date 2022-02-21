// toggle this flag to enable PUP'ing:
// #define HYPERCOMM_USE_PUP

#include <hypercomm/tasking/workgroup.hpp>
#include "pgm.decl.h"

using namespace hypercomm;
using namespace hypercomm::tasking;

class relaxation_task : public task<relaxation_task> {
  bool converged;
  bool hasLeft, hasRight;

  double tolerance;

  int nRecvd, nNeighbors, nWorkers;
  int it, maxIters;

public:
  relaxation_task(PUP::reconstruct) {}

  relaxation_task(task_payload &&payload)
      : converged(false), nRecvd(0), tolerance(-1) {
    auto args =
        std::forward_as_tuple(this->nWorkers, this->maxIters, this->tolerance);
    flex::pup_unpack(args, payload.data, std::move(payload.src));

    auto idx = this->index();
    this->hasLeft = idx > 0;
    this->hasRight = (idx + 1) < this->nWorkers;
    this->nNeighbors = (int)this->hasLeft + this->hasRight;

    CkPrintf("task %d of %d created, has %d neighbors.\n", idx, this->nWorkers,
             this->nNeighbors);

    this->converge_loop();
  }

  void converge_loop(void) {
    if (!converged && (it++ < maxIters)) {
      this->nRecvd = 0;

      this->send_ghosts();

      if (this->nNeighbors == 0) {
        this->check_and_compute();
      } else {
        this->suspend<&relaxation_task::receive_ghost>();
      }
    } else {
      this->on_converge();
    }
  }

  void send_ghosts(void) {
    auto idx = this->index();

    if (this->hasLeft) {
      // TODO ( pack ghost data )
      this->send<&relaxation_task::receive_ghost>(idx - 1);
    }

    if (this->hasRight) {
      // TODO ( pack ghost data )
      this->send<&relaxation_task::receive_ghost>(idx + 1);
    }
  }

  void receive_ghost(task_payload &&payload) {
    // TODO ( unpack then copy ghost data )

    if ((++this->nRecvd) >= this->nNeighbors) {
      this->check_and_compute();
    } else {
      this->suspend<&relaxation_task::receive_ghost>();
    }
  }

  void check_and_compute(void) {
    double max_error = this->tolerance * 2;

    CkPrintf("%d> iteration %d\n", this->index(), this->it);

    this->all_reduce<&relaxation_task::receive_max_error>(
        max_error, CkReduction::max_double);

    this->suspend<&relaxation_task::receive_max_error>();
  }

  void receive_max_error(task_payload &&payload) {
    auto &max_error = *((double *)payload.data);
    this->converged = (max_error <= this->tolerance);
    this->converge_loop();
  }

  void on_converge(void) {
    auto idx = this->index();
    auto root = 0;

    // send our contribution to the root
    this->reduce<&relaxation_task::receive_grid>(idx, CkReduction::set, root);

    if (root == idx) {
      this->suspend<&relaxation_task::receive_grid>();
    } else {
      this->terminate();
    }
  }

  void receive_grid(task_payload &&payload) {
    std::set<int> set;
    auto *elt = (CkReduction::setElement *)payload.data;
    // move all the reduction set's entries into the set
    while (elt) {
      set.insert(*((int *)elt->data));
      elt = elt->next();
    }
    // validate that we received all elements
    CkAssert(this->nWorkers == set.size());
    // signal the task is ready to shut down
    this->terminate();
    // then actually shut down!
    CkExit();
  }

  void pup(PUP::er &p) {
    p | this->converged;
    p | this->tolerance;
    p | this->hasLeft;
    p | this->hasRight;
    p | this->nRecvd;
    p | this->nNeighbors;
    p | this->nWorkers;
    p | this->it;
    p | this->maxIters;
  }
};

class pgm : public CBase_pgm {
  workgroup_proxy group;

public:
  pgm(CkArgMsg *msg) {
    auto factor = (msg->argc >= 2) ? atoi(msg->argv[1]) : 4;
    auto n = factor * CkNumPes();
    CkEnforceMsg(n > 0, "must have at least one chare");
    auto maxIters = 100;
    auto tolerance = 0.005;
    group = workgroup_proxy::ckNew(n);
    launch<relaxation_task>(group, n, maxIters, tolerance);
  }
};

#include "pgm.def.h"
#include <hypercomm/tasking/tasking.def.h>
