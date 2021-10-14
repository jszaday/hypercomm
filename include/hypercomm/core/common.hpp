#ifndef __HYPERCOMM_CORE_COMMON_HPP__
#define __HYPERCOMM_CORE_COMMON_HPP__

#include "../messaging/common.hpp"
#include "../components/base.hpp"

#include "callback.hpp"
#include "proxy.hpp"

namespace hypercomm {

template <typename T>
using impl_index_t = typename index_for<T>::type;

template <typename A, typename Enable = void>
class comproxy;

class imprintable_base_;

template <typename Index>
class imprintable;

class identity_base_;

template <typename Index>
class identity;

class entry_port;
class destination;

class generic_locality_;

template <typename Index>
class indexed_locality_;

using entry_port_ptr = std::shared_ptr<entry_port>;
using entry_port_map = comparable_map<entry_port_ptr, destination>;
using component_port_pair = std::pair<component_id_t, component_port_t>;
using component_map =
    std::unordered_map<component_id_t, std::unique_ptr<components::base_>>;

template <typename T>
using mapped_queue = comparable_map<entry_port_ptr, std::deque<T>>;

generic_locality_* access_context_(void);

using proxy_ptr = std::shared_ptr<hypercomm::proxy>;

template <typename Index>
using element_ptr = std::shared_ptr<hypercomm::element_proxy<Index>>;

template <typename Index>
using collective_ptr = std::shared_ptr<hypercomm::collective_proxy<Index>>;

template <typename Action>
CkMessage* pack_action(const Action&);

}  // namespace hypercomm

#include "../serialization/pup.hpp"

#endif
