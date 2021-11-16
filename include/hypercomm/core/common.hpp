#ifndef __HYPERCOMM_CORE_COMMON_HPP__
#define __HYPERCOMM_CORE_COMMON_HPP__

#include "../messaging/common.hpp"
#include "../components/identifiers.hpp"

#include "proxy.hpp"

namespace hypercomm {

template <typename T>
using impl_index_t = typename index_for<T>::type;

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

class deliverable;

class hyper_value;
using value_ptr = std::unique_ptr<hyper_value>;

template <typename T>
class typed_value;

template <typename T>
using typed_value_ptr = std::unique_ptr<typed_value<T>>;

inline bool passthru_context_(const destination&, deliverable&);

template <typename T>
inline void try_return(typed_value_ptr<T>&& value);

void try_return(deliverable&& dev);

struct future;
using future_id_t = std::uint32_t;

void send2future(const future& f, deliverable&& dev);
}  // namespace hypercomm

#include "../serialization/pup.hpp"

#endif
