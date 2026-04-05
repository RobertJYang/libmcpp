/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef MC_PROTOCOL_RUNTIME_H
#define MC_PROTOCOL_RUNTIME_H

#include <mc/protocol/context.h>

#include <tuple>
#include <type_traits>
#include <utility>

namespace mc::protocol {

template <typename Spec>
class runtime;

namespace detail {

template <typename RequestType, std::size_t Index>
inline command dispatch_layer(RequestType& request, flow_direction direction);

template <typename RequestType, std::size_t... Indexes>
inline command invoke_stack_impl(RequestType* request, std::size_t index, flow_direction direction,
                                 std::index_sequence<Indexes...>);

template <typename Layer, typename Ctx, typename = void>
struct has_on_push : std::false_type {};

template <typename Layer, typename Ctx>
struct has_on_push<Layer, Ctx,
                   std::void_t<decltype(Layer::on_push(std::declval<Ctx&>(), std::declval<mc::protocol::packet&>()))>>
    : std::true_type {};

template <typename Layer, typename Ctx, typename = void>
struct has_on_pop : std::false_type {};

template <typename Layer, typename Ctx>
struct has_on_pop<Layer, Ctx,
                  std::void_t<decltype(Layer::on_pop(std::declval<Ctx&>(), std::declval<mc::protocol::packet&>()))>>
    : std::true_type {};

template <typename Layer, typename Ctx>
inline command invoke_push(Ctx& ctx, mc::protocol::packet& packet)
{
    if constexpr (has_on_push<Layer, Ctx>::value) {
        return Layer::on_push(ctx, packet);
    }
    return ctx.push_next();
}

template <typename Layer, typename Ctx>
inline command invoke_pop(Ctx& ctx, mc::protocol::packet& packet)
{
    if constexpr (has_on_pop<Layer, Ctx>::value) {
        return Layer::on_pop(ctx, packet);
    }
    return ctx.pop_next();
}

template <typename... Layers>
inline command invoke_stack(void* request_object, std::size_t index, flow_direction direction)
{
    using request_type = request<stack_spec<Layers...>>;
    return invoke_stack_impl(static_cast<request_type*>(request_object), index, direction,
                             std::index_sequence_for<Layers...>{});
}

template <typename RequestType, std::size_t... Indexes>
inline command invoke_stack_impl(RequestType* request, std::size_t index, flow_direction direction,
                                 std::index_sequence<Indexes...>)
{
    using dispatch_fn                          = command (*)(RequestType&, flow_direction);
    static constexpr dispatch_fn dispatchers[] = {&dispatch_layer<RequestType, Indexes>...};
    return dispatchers[index](*request, direction);
}

template <typename RequestType, std::size_t Index>
inline command dispatch_layer(RequestType& request, flow_direction direction)
{
    using spec_type  = typename RequestType::spec_type;
    using layer_type = std::tuple_element_t<Index, typename RequestType::layers_type>;

    context<spec_type, layer_type> ctx(request);

    if (direction == flow_direction::push) {
        return invoke_push<layer_type>(ctx, request.packet());
    }
    return invoke_pop<layer_type>(ctx, request.packet());
}

template <typename... Layers>
inline void* unsafe_layer_ptr_for_stack(void* request_object, std::size_t index) noexcept
{
    using request_type = request<stack_spec<Layers...>>;
    return static_cast<request_type*>(request_object)->unsafe_layer_ptr(index);
}

template <typename Spec>
struct stack_descriptor_holder;

template <typename... Layers>
struct stack_descriptor_holder<stack_spec<Layers...>> {
    static const stack_descriptor& get()
    {
        static const stack_descriptor descriptor = {
            sizeof...(Layers),
            &invoke_stack<Layers...>,
            &unsafe_layer_ptr_for_stack<Layers...>,
        };
        return descriptor;
    }
};

template <typename Spec>
inline const stack_descriptor& stack_descriptor_for()
{
    return stack_descriptor_holder<Spec>::get();
}

} // namespace detail

template <typename... Layers>
class runtime<stack_spec<Layers...>> {
public:
    using spec_type    = stack_spec<Layers...>;
    using request_type = request<spec_type>;

    [[nodiscard]] static execution_state push(request_type& request)
    {
        static_assert(sizeof...(Layers) > 0, "runtime requires at least one layer");
        bind(request);
        return detail::drive_request(request.core(), 0, flow_direction::push);
    }

    template <typename Layer>
    [[nodiscard]] static execution_state push_from(request_type& request)
    {
        bind(request);
        return detail::drive_request(request.core(), detail::layer_index<Layer, Layers...>::value,
                                     flow_direction::push);
    }

    [[nodiscard]] static execution_state pop(request_type& request)
    {
        static_assert(sizeof...(Layers) > 0, "runtime requires at least one layer");
        bind(request);
        return detail::drive_request(request.core(), sizeof...(Layers) - 1, flow_direction::pop);
    }

    template <typename Layer>
    [[nodiscard]] static execution_state pop_from(request_type& request)
    {
        bind(request);
        return detail::drive_request(request.core(), detail::layer_index<Layer, Layers...>::value, flow_direction::pop);
    }

    [[nodiscard]] static execution_state resume(request_type& request)
    {
        bind(request);
        return detail::resume_request(request.core());
    }

private:
    static void bind(request_type& request)
    {
        request.bind_runtime(detail::stack_descriptor_for<spec_type>(), &request);
    }
};

} // namespace mc::protocol

#endif // MC_PROTOCOL_RUNTIME_H
