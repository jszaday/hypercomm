#ifndef __HYPERCOMM_COMPONENTS_COMPONENT_HPP__
#define __HYPERCOMM_COMPONENTS_COMPONENT_HPP__

#include <ck.h>
#include "base.hpp"
#include "inbox.hpp"
#include "outbox.hpp"
#include "../core/typed_value.hpp"

namespace hypercomm {

template <>
struct wrap_<std::tuple<deliverable>, typed_value_ptr> {
  using type = std::tuple<deliverable>;
};

template <typename T>
struct wrap_<std::tuple<typed_value_ptr<T>>, typed_value_ptr> {
  // don't wrap things if they're already wrapped!
  using type = std::tuple<typed_value_ptr<T>>;
};

template <typename T, typename Enable = void>
struct dev_conv_;

template <>
struct dev_conv_<deliverable> {
  static deliverable convert(deliverable&& dev) { return std::move(dev); }
};

template <typename T>
struct dev_conv_<typed_value_ptr<T>> {
  static typed_value_ptr<T> convert(deliverable&& dev) {
    return dev2typed<T>(std::move(dev));
  }
};

template <typename T>
struct invoker_ {
  inline static void invoke(const T& t) { t(); }
};

template <>
struct invoker_<std::nullptr_t> {
  inline static void invoke(const std::nullptr_t&) {}
};

namespace components {
template <typename Inputs, typename Outputs>
struct default_acceptor_;

template <typename Inputs, typename Outputs,
          template <typename, typename> class Acceptor = default_acceptor_>
class component : public base_ {
 public:
  using in_type = typename tuplify_<Inputs>::type;
  using out_type = typename tuplify_<Outputs, true>::type;

  using in_set = typename wrap_<in_type, typed_value_ptr>::type;
  using out_set = typename wrap_<out_type, typed_value_ptr>::type;

  template <std::size_t I>
  using in_elt_t = typename std::tuple_element<I, in_set>::type;

  template <std::size_t O>
  using out_elt_t = typename std::tuple_element<O, out_set>::type;

  using acceptor_type = Acceptor<Inputs, Outputs>;

  friend acceptor_type;

 private:
  static constexpr std::size_t n_inputs_ = std::tuple_size<in_type>::value;
  static constexpr std::size_t n_outputs_ = std::tuple_size<out_type>::value;
  static_assert(n_inputs_ >= 1, "expected at least one input!");

 protected:
  using incoming_type = typename acceptor_type::incoming_type;
  using outgoing_type = outbox_<out_set>;
  using incoming_iterator = typename incoming_type::iterator;

  incoming_type incoming_;
  outgoing_type outgoing_;

 public:
  using accept_array_t = std::array<accept_fn_t, n_inputs_>;
  static accept_array_t acceptors;
#if HYPERCOMM_ERROR_CHECKING
  using in_type_array_t = std::array<std::type_index, n_inputs_>;
  static in_type_array_t in_types;
#endif

  component(std::size_t id_)
      : base_(id_, n_inputs_, n_outputs_, acceptors.data(), in_types.data()) {}

  virtual ~component() { this->incoming_.empty_buffers(); }

 public:
  template <std::size_t O, std::size_t I, typename Input_, typename Output_,
            template <typename, typename> class Acceptor_>
  void output_to(const component<Input_, Output_, Acceptor_>& peer) {
    static_assert(std::is_same<out_elt_t<O>,
                               typename component<Input_, Output_, Acceptor_>::
                                   template in_elt_t<I>>::value,
                  "component types must be compatible");
    this->output_to<O>(peer.id, I);
  }

  template <std::size_t O, typename... Args>
  void output_to(Args&&... args) {
    bool prev =
        (this->outgoing_).template connect_to<O>(std::forward<Args>(args)...);
    CkAssertMsg(!(this->active && prev),
                "component must be offline to change outbound connections");
    if (this->active) {
      (this->outgoing_).template try_flush<O>();
    }
  }

  template <std::size_t O>
  inline bool has_output(void) const {
    return (this->outgoing_).template is_ready<O>();
  }

  virtual out_set action(in_set&) = 0;

  virtual void activate(void) override {
    CkAssertMsg(!this->active, "component already online");
    this->activated = this->active = true;
    incoming_iterator search;
    while (this->active && (search = this->incoming_.find_ready()) !=
                               std::end(this->incoming_)) {
      this->stage_action(search);
    }
  }

  virtual void deactivate(status_ signal) override {
    this->active = false;

    this->incoming_.empty_buffers();
    if (signal == status_::kInvalidation) {
      this->send_invalidations_();
    }

    this->notify_listeners(signal);
  }

  virtual bool collectible(void) const override {
    return this->activated && !this->active && this->outgoing_.empty();
  }

 private:
  template <std::size_t O>
  inline typename std::enable_if<(O == 0)>::type send_invalidation_(void) {}

  template <std::size_t O>
  inline typename std::enable_if<(O == 1)>::type send_invalidation_(void) {
    (this->outgoing_).template invalidate<(O - 1)>();
  }

  template <std::size_t O>
  inline typename std::enable_if<(O > 1)>::type send_invalidation_(void) {
    this->send_invalidation_<(O - 1)>();
    (this->outgoing_).template invalidate<(O - 1)>();
  }

  void send_invalidations_(void) { this->send_invalidation_<n_outputs_>(); }

  template <std::size_t I>
  void on_invalidation_(void) {
    this->deactivate(kInvalidation);
  }

  template <std::size_t I>
  inline static typename std::enable_if<(I == 0) && (n_inputs_ == 1)>::type
  direct_stage(component<Inputs, Outputs, Acceptor>* self, in_elt_t<I>&& val) {
    CkAssertMsg((bool)val, "not equipped for invalidations!");
    in_set set(std::move(val));
    if (!self->stage_action(set, nullptr)) {
      // this should be consistent with "find_ready"
      // and the opposite of "find_gap"
      self->incoming_.emplace_front(std::move(set));
    }
  }

  template <std::size_t I>
  inline static typename std::enable_if<(I >= 0) && (n_inputs_ > 1)>::type
  direct_stage(component<Inputs, Outputs, Acceptor>* self, in_elt_t<I>&& val) {
    CkAbort("-- unreachable --");
  }

  inline void stage_action(const incoming_iterator& search) {
    this->stage_action(*search, [&](void) { this->incoming_.erase(search); });
  }

  // returns true if the set was consumed
  template <typename Fn>
  inline bool stage_action(in_set& set, const Fn& cleanup) {
    if (this->active) {
      try {
        auto res = this->action(set);
        invoker_<Fn>::invoke(cleanup);
        this->outgoing_.unspool(res);
        if (!this->persistent) {
          this->deactivate(kCompletion);
        }
      } catch (suspension_& s) {
        if (this->id != s.com) {
          throw s;
        } else {
          invoker_<Fn>::invoke(cleanup);
        }
      }
      return true;
    } else {
      return false;
    }
  }

  template <std::size_t I, typename Array>
  inline static typename std::enable_if<(I == 0)>::type make_acceptors_(
      Array& arr) {
    using elt_t = typename Array::value_type;
    new (&arr[I]) elt_t(acceptor_type::template accept<I>);
  }

  template <std::size_t I, typename Array>
  inline static typename std::enable_if<(I >= 1)>::type make_acceptors_(
      Array& arr) {
    using elt_t = typename Array::value_type;
    new (&arr[I]) elt_t(acceptor_type::template accept<I>);
    make_acceptors_<(I - 1), Array>(arr);
  }

  template <typename Array>
  inline static Array make_acceptors_(void) {
    using type = Array;
    using storage_type =
        typename std::aligned_storage<sizeof(type), alignof(type)>::type;
    storage_type storage;
    auto* arr = reinterpret_cast<type*>(&storage);
    make_acceptors_<n_inputs_ - 1, type>(*arr);
    return *arr;
  }
};

template <typename Inputs, typename Outputs>
struct default_acceptor_ {
  using component_type = component<Inputs, Outputs, default_acceptor_>;
  using incoming_type = inbox_<typename component_type::in_set>;

  template <std::size_t I>
  using in_elt_t = typename component_type::template in_elt_t<I>;

  static constexpr auto n_inputs_ = component_type::n_inputs_;

 private:
  template <std::size_t I>
  inline static void accept_(component_type* self, in_elt_t<I>&& val) {
    if (!(val || self->permissive)) {
      self->template on_invalidation_<I>();
    } else if (n_inputs_ == 1) {
      // bypass seeking a partial set for single-input coms
      component_type::direct_stage<I>(self, std::move(val));
    } else {
      // look for a set missing this value
      auto search = self->incoming_.template find_gap<I>();
      const auto gapless = search == std::end(self->incoming_);
      // if we couldn't find one:
      if (gapless) {
        // create a new set
        self->incoming_.emplace_front();
        // then update the search iterator
        search = std::begin(self->incoming_);
      }
      // update the value in the set
      std::get<I>(*search) = std::move(val);
      // check whether it's ready, and stage it if so!
      if (!gapless && incoming_type::is_ready(*search)) {
        self->stage_action(search);
      }
    }
  }

 public:
  // TODO ( reject values if we've been deactivated! )
  template <std::size_t I>
  static bool accept(base_* base, component_port_t port, deliverable& dev) {
    CkAssertMsg(port < n_inputs_, "port out of range!");
    auto* self = static_cast<component_type*>(base);
    accept_<I>(self, dev_conv_<in_elt_t<I>>::convert(std::move(dev)));
    return true;
  }
};

#if HYPERCOMM_ERROR_CHECKING
template <typename Inputs, typename Outputs,
          template <typename, typename> class Acceptor>
typename component<Inputs, Outputs, Acceptor>::in_type_array_t
    component<Inputs, Outputs, Acceptor>::in_types =
        make_type_list_<component<Inputs, Outputs, Acceptor>::in_type,
                        component<Inputs, Outputs, Acceptor>::n_inputs_>();
#endif

template <typename Inputs, typename Outputs,
          template <typename, typename> class Acceptor>
typename component<Inputs, Outputs, Acceptor>::accept_array_t
    component<Inputs, Outputs, Acceptor>::acceptors =
        component<Inputs, Outputs, Acceptor>::make_acceptors_<accept_array_t>();

inline bool components::base_::accept(std::size_t port, deliverable& dev) {
#if HYPERCOMM_STRICT_MODE
  CkAssertMsg((bool)dev.endpoint(), "received unreturnable value!");
#endif
  return (this->acceptors[(port % this->n_inputs)])(this, port, dev);
}
}  // namespace components
template <typename Input, typename Output>
using component = components::component<Input, Output>;
}  // namespace hypercomm

#endif
