#ifndef __HYPERCOMM_ENTRY_PORT_HPP__
#define __HYPERCOMM_ENTRY_PORT_HPP__

#include "../components/status_listener.hpp"

#include "value.hpp"
#include "comparable.hpp"
#include "impermanent.hpp"

namespace hypercomm {

struct entry_port : public virtual polymorph,
                    public virtual comparable,
                    public virtual impermanent,
                    public virtual value_source,
                    public virtual components::status_listener,
                    public virtual_enable_shared_from_this<entry_port> {
  virtual std::string to_string(void) const = 0;

  // implemented in generic_locality.hpp
  virtual void on_completion(const component&) override;
  virtual void on_invalidation(const component&) override;
  virtual void take_back(std::shared_ptr<hyper_value>&& value) override;
};

using entry_port_ptr = std::shared_ptr<entry_port>;
}  // namespace hypercomm

#endif
