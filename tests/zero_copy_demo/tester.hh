#ifndef __TESTER_HH__
#define __TESTER_HH__

#include <hypercomm/core/locality.hpp>
#include <hypercomm/core/persistent_port.hpp>
#include <hypercomm/components/mailbox.hpp>
#include <hypercomm/messaging/messaging.hpp>

#include "tester.decl.h"

using namespace hypercomm;

struct sentinel_callback : public core::callback {
  CthThread th = nullptr;
  std::int64_t gcount = 0;
  std::int64_t lcount = 0;

  sentinel_callback(void) = default;

  virtual void send(core::callback::value_type&&) override {
    this->lcount += 1;
    this->try_resume();
  }

  inline void wait(const std::int64_t& count) {
    this->gcount += count;
    this->th = CthSelf();

    if (!this->try_resume()) {
      CthSuspend();
    }
  }

  bool try_resume(void) {
    auto status = false;

    if (this->lcount == this->gcount) {
      this->lcount = this->gcount = 0;
      if (this->th) {
        if (this->th == CthSelf()) {
          status = true;
        } else {
          CthAwaken(this->th);
        }
        this->th = nullptr;
      }
    }

    return status;
  }

  virtual void __pup__(serdes& s) override { NOT_IMPLEMENTED; }
};

#endif
