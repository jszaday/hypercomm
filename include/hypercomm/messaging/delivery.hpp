#ifndef __HYPERCOMM_MESSAGING_DELIVERY_HPP__
#define __HYPERCOMM_MESSAGING_DELIVERY_HPP__

#include "../core/proxy.hpp"
#include "messaging.hpp"

namespace hypercomm {

template <typename Index>
void deliver(const element_proxy<Index>&, message* msg);

}

#endif
