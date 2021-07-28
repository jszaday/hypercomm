#ifndef __HYPERCOMM_CORE_COMMON_HPP__
#define __HYPERCOMM_CORE_COMMON_HPP__

#include "future.hpp"
#include "entry_port.hpp"

#include "../components/component.hpp"

namespace hypercomm {

template <typename A, typename Enable = void>
class comproxy;

class connector_;
class destination_;

class generic_locality_;

template<typename BaseIndex, typename Index>
class indexed_locality_;

using entry_port_map = comparable_map<entry_port_ptr, destination_>;
using component_port_t = std::pair<component::id_t, component::port_type>;
using component_map =
    std::unordered_map<component::id_t, std::unique_ptr<component>>;

template <typename Key>
using message_queue = comparable_map<Key, std::deque<component::value_type>>;

message* repack_to_port(const entry_port_ptr& port,
                        component::value_type&& value);

generic_locality_* access_context_(void);

 // TODO eliminate this
callback_ptr local_connector_(const component_id_t&, const component::port_type&);

using proxy_ptr = std::shared_ptr<hypercomm::proxy>;

template <typename Index>
using element_ptr = std::shared_ptr<hypercomm::element_proxy<Index>>;

template <typename Index>
using collective_ptr = std::shared_ptr<hypercomm::collective_proxy<Index>>;

struct future_manager_ {
  virtual future make_future(void) = 0;

  virtual void request_future(const future& f, const callback_ptr& cb) = 0;
};

}

#endif
