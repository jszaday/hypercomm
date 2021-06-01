#ifndef __HYPERCOMM_CORE_HPP__
#define __HYPERCOMM_CORE_HPP__

#include "core/proxy.hpp"
#include "core/callback.hpp"
#include "core/comparable.hpp"
#include "core/immediate.hpp"
#include "core/forwarding_callback.hpp"

namespace hypercomm {
using callback = core::callback;
using callback_ptr = std::shared_ptr<callback>;

using combiner = core::combiner;
using combiner_ptr = std::shared_ptr<combiner>;
}

#endif
