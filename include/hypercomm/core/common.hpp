#ifndef __HYPERCOMM_CORE_COMMON_HPP__
#define __HYPERCOMM_CORE_COMMON_HPP__

#include "../messaging/common.hpp"
#include "../components/component.hpp"
#include "callback.hpp"

#define NOT_IMPLEMENTED CkAbort("not yet implemented!")

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
using component_port_t = std::pair<component::id_t, component::port_type>;
using component_map =
    std::unordered_map<component::id_t, std::unique_ptr<component>>;

template <typename T>
using mapped_queue = comparable_map<entry_port_ptr, std::deque<T>>;

message* repack_to_port(const entry_port_ptr& port,
                        component::value_type&& value);

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
