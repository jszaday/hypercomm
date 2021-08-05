#ifndef __HYPERCOMM_MESSAGING_DELIVERY_HPP__
#define __HYPERCOMM_MESSAGING_DELIVERY_HPP__

#include "messaging.hpp"
#include "../core/proxy.hpp"

namespace hypercomm {

template <typename Index>
void deliver(const element_proxy<Index>&, message* msg);

}

#endif
