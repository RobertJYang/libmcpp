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

#include <mc/exception.h>
#include <mc/future.h>
#include <mc/protocol/context.h>

#include <tuple>
#include <type_traits>
#include <utility>

namespace mc::proto {

template <typename Spec>
class runtime;

namespace detail {

template <typename RequestType, typename ProtocolTuple, std::size_t Index>
inline command dispatch_protocol(RequestType& request, ProtocolTuple& protocols, flow_direction direction);

template <typename RequestType, typename ProtocolTuple, std::size_t... Indexes>
inline command invoke_stack_impl(RequestType* request, ProtocolTuple* protocols, std::size_t index,
                                 flow_direction direction, std::index_sequence<Indexes...>);

template <typename ProtocolTuple, std::size_t... Indexes>
inline void link_protocol_parents_impl(ProtocolTuple& protocols, std::index_sequence<Indexes...>);

template <std::size_t Index, typename ProtocolTuple>
inline void link_protocol_parent(ProtocolTuple& protocols);

MC_API void finalize_async_result(request_core& core, execution_state state);

template <typename Protocol, typename = void>
struct has_request_view_type : std::false_type {};

template <typename Protocol>
struct has_request_view_type<Protocol, std::void_t<typename Protocol::request_type>> : std::true_type {};

template <typename Protocol, typename = void>
struct has_member_view_on_push : std::false_type {};

template <typename Protocol>
struct has_member_view_on_push<
    Protocol, std::void_t<typename Protocol::request_type, decltype(std::declval<Protocol&>().on_push(
                                                               std::declval<typename Protocol::request_type&>()))>>
    : std::true_type {};

template <typename Protocol, typename = void>
struct has_static_view_on_push : std::false_type {};

template <typename Protocol>
struct has_static_view_on_push<
    Protocol, std::void_t<typename Protocol::request_type,
                          decltype(Protocol::on_push(std::declval<typename Protocol::request_type&>()))>>
    : std::true_type {};

template <typename Protocol, typename = void>
struct has_member_view_on_pop : std::false_type {};

template <typename Protocol>
struct has_member_view_on_pop<
    Protocol, std::void_t<typename Protocol::request_type,
                          decltype(std::declval<Protocol&>().on_pop(std::declval<typename Protocol::request_type&>()))>>
    : std::true_type {};

template <typename Protocol, typename = void>
struct has_static_view_on_pop : std::false_type {};

template <typename Protocol>
struct has_static_view_on_pop<Protocol,
                              std::void_t<typename Protocol::request_type,
                                          decltype(Protocol::on_pop(std::declval<typename Protocol::request_type&>()))>>
    : std::true_type {};

template <typename Protocol, typename Ctx, typename = void>
struct has_member_request_on_push : std::false_type {};

template <typename Protocol, typename RequestType>
struct has_member_request_on_push<
    Protocol, RequestType, std::void_t<decltype(std::declval<Protocol&>().on_push(std::declval<RequestType&>()))>>
    : std::true_type {};

template <typename Protocol, typename Ctx, typename = void>
struct has_static_request_on_push : std::false_type {};

template <typename Protocol, typename RequestType>
struct has_static_request_on_push<Protocol, RequestType,
                                  std::void_t<decltype(Protocol::on_push(std::declval<RequestType&>()))>>
    : std::true_type {};

template <typename Protocol, typename Ctx, typename = void>
struct has_member_on_push : std::false_type {};

template <typename Protocol, typename Ctx>
struct has_member_on_push<
    Protocol, Ctx,
    std::void_t<decltype(std::declval<Protocol&>().on_push(std::declval<Ctx&>(), std::declval<mc::proto::packet&>()))>>
    : std::true_type {};

template <typename Protocol, typename Ctx, typename = void>
struct has_static_on_push : std::false_type {};

template <typename Protocol, typename Ctx>
struct has_static_on_push<
    Protocol, Ctx, std::void_t<decltype(Protocol::on_push(std::declval<Ctx&>(), std::declval<mc::proto::packet&>()))>>
    : std::true_type {};

template <typename Protocol, typename Ctx, typename = void>
struct has_member_request_on_pop : std::false_type {};

template <typename Protocol, typename RequestType>
struct has_member_request_on_pop<Protocol, RequestType,
                                 std::void_t<decltype(std::declval<Protocol&>().on_pop(std::declval<RequestType&>()))>>
    : std::true_type {};

template <typename Protocol, typename Ctx, typename = void>
struct has_static_request_on_pop : std::false_type {};

template <typename Protocol, typename RequestType>
struct has_static_request_on_pop<Protocol, RequestType,
                                 std::void_t<decltype(Protocol::on_pop(std::declval<RequestType&>()))>>
    : std::true_type {};

template <typename Protocol, typename Ctx, typename = void>
struct has_member_on_pop : std::false_type {};

template <typename Protocol, typename Ctx>
struct has_member_on_pop<
    Protocol, Ctx,
    std::void_t<decltype(std::declval<Protocol&>().on_pop(std::declval<Ctx&>(), std::declval<mc::proto::packet&>()))>>
    : std::true_type {};

template <typename Protocol, typename Ctx, typename = void>
struct has_static_on_pop : std::false_type {};

template <typename Protocol, typename Ctx>
struct has_static_on_pop<
    Protocol, Ctx, std::void_t<decltype(Protocol::on_pop(std::declval<Ctx&>(), std::declval<mc::proto::packet&>()))>>
    : std::true_type {};

template <typename Protocol, typename RequestType>
inline command invoke_push(Protocol& proto, RequestType& request)
{
    if constexpr (has_member_request_on_push<Protocol, RequestType>::value) {
        return proto.on_push(request);
    } else if constexpr (has_static_request_on_push<Protocol, RequestType>::value) {
        return Protocol::on_push(request);
    }
    return request.push_next();
}

template <typename Protocol>
inline command invoke_push_view(Protocol& proto, typename Protocol::request_type& request)
{
    if constexpr (has_member_view_on_push<Protocol>::value) {
        return proto.on_push(request);
    } else if constexpr (has_static_view_on_push<Protocol>::value) {
        return Protocol::on_push(request);
    }
    return request.push_next();
}

template <typename Protocol, typename Ctx>
inline command invoke_push_legacy(Protocol& proto, Ctx& ctx, mc::proto::buffer& buffer)
{
    if constexpr (has_member_on_push<Protocol, Ctx>::value) {
        return proto.on_push(ctx, buffer);
    } else if constexpr (has_static_on_push<Protocol, Ctx>::value) {
        return Protocol::on_push(ctx, buffer);
    }
    return ctx.push_next();
}

template <typename Protocol, typename RequestType>
inline command invoke_pop(Protocol& proto, RequestType& request)
{
    if constexpr (has_member_request_on_pop<Protocol, RequestType>::value) {
        return proto.on_pop(request);
    } else if constexpr (has_static_request_on_pop<Protocol, RequestType>::value) {
        return Protocol::on_pop(request);
    }
    return request.pop_next();
}

template <typename Protocol>
inline command invoke_pop_view(Protocol& proto, typename Protocol::request_type& request)
{
    if constexpr (has_member_view_on_pop<Protocol>::value) {
        return proto.on_pop(request);
    } else if constexpr (has_static_view_on_pop<Protocol>::value) {
        return Protocol::on_pop(request);
    }
    return request.pop_next();
}

template <typename RequestType>
inline request_view_base make_request_view_base(RequestType& request)
{
    using spec_type = typename RequestType::spec_type;
    return request_view_base(request.unsafe_core(), &prepare_push_view_buffer<spec_type>,
                             &prepare_pop_view_buffer<spec_type>);
}

template <typename Protocol, typename RequestType>
inline auto make_typed_request_view(RequestType& request)
{
    return typename Protocol::request_type(make_request_view_base(request), request.template packet<Protocol>(),
                                           request.template context<Protocol>());
}

template <typename Protocol, typename Ctx>
inline command invoke_pop_legacy(Protocol& proto, Ctx& ctx, mc::proto::buffer& buffer)
{
    if constexpr (has_member_on_pop<Protocol, Ctx>::value) {
        return proto.on_pop(ctx, buffer);
    } else if constexpr (has_static_on_pop<Protocol, Ctx>::value) {
        return Protocol::on_pop(ctx, buffer);
    }
    return ctx.pop_next();
}

template <typename... Protocols>
inline command invoke_stack(void* request_object, void* runtime_object, std::size_t index, flow_direction direction)
{
    using request_type        = request<stack_spec<Protocols...>>;
    using protocol_tuple_type = std::tuple<Protocols...>;
    return invoke_stack_impl(static_cast<request_type*>(request_object),
                             static_cast<protocol_tuple_type*>(runtime_object), index, direction,
                             std::index_sequence_for<Protocols...>{});
}

template <typename RequestType, typename ProtocolTuple, std::size_t... Indexes>
inline command invoke_stack_impl(RequestType* request, ProtocolTuple* protocols, std::size_t index,
                                 flow_direction direction, std::index_sequence<Indexes...>)
{
    using dispatch_fn                          = command (*)(RequestType&, ProtocolTuple&, flow_direction);
    static constexpr dispatch_fn dispatchers[] = {&dispatch_protocol<RequestType, ProtocolTuple, Indexes>...};
    return dispatchers[index](*request, *protocols, direction);
}

template <typename ProtocolTuple, std::size_t... Indexes>
inline void link_protocol_parents_impl(ProtocolTuple& protocols, std::index_sequence<Indexes...>)
{
    (link_protocol_parent<Indexes>(protocols), ...);
}

template <std::size_t Index, typename ProtocolTuple>
inline void link_protocol_parent(ProtocolTuple& protocols)
{
    auto& current = std::get<Index>(protocols);
    current.set_layer_index(Index);
    if constexpr (Index == 0) {
        current.set_parent(nullptr);
    } else {
        current.set_parent(static_cast<mc::proto::protocol*>(&std::get<Index - 1>(protocols)));
    }
}

template <typename... Protocols>
inline void link_protocol_parents(std::tuple<Protocols...>& protocols)
{
    link_protocol_parents_impl(protocols, std::index_sequence_for<Protocols...>{});
}

template <typename RequestType, typename ProtocolTuple, std::size_t Index>
inline command dispatch_protocol(RequestType& request, ProtocolTuple& protocols, flow_direction direction)
{
    using spec_type     = typename RequestType::spec_type;
    using protocol_type = std::tuple_element_t<Index, typename RequestType::protocols_type>;

    auto& proto = std::get<Index>(protocols);

    if (direction == flow_direction::push) {
        if constexpr (has_member_view_on_push<protocol_type>::value || has_static_view_on_push<protocol_type>::value) {
            auto typed_request = make_typed_request_view<protocol_type>(request);
            return invoke_push_view(proto, typed_request);
        }
        if constexpr (has_member_request_on_push<protocol_type, RequestType>::value ||
                      has_static_request_on_push<protocol_type, RequestType>::value) {
            return invoke_push(proto, request);
        }

        runtime_context<spec_type, protocol_type> ctx(request);
        return invoke_push_legacy(proto, ctx, request.buffer());
    }

    if constexpr (has_member_view_on_pop<protocol_type>::value || has_static_view_on_pop<protocol_type>::value) {
        auto typed_request = make_typed_request_view<protocol_type>(request);
        return invoke_pop_view(proto, typed_request);
    }
    if constexpr (has_member_request_on_pop<protocol_type, RequestType>::value ||
                  has_static_request_on_pop<protocol_type, RequestType>::value) {
        return invoke_pop(proto, request);
    }

    runtime_context<spec_type, protocol_type> ctx(request);
    return invoke_pop_legacy(proto, ctx, request.buffer());
}

template <typename... Protocols>
inline void* unsafe_layer_ptr_for_stack(void* request_object, std::size_t index) noexcept
{
    using request_type = request<stack_spec<Protocols...>>;
    return static_cast<request_type*>(request_object)->unsafe_layer_ptr(index);
}

template <typename... Protocols>
inline void* unsafe_protocol_ptr_for_stack(void* runtime_object, std::size_t index) noexcept
{
    using protocol_tuple_type = std::tuple<Protocols...>;
    return detail::tuple_ptr_at(*static_cast<protocol_tuple_type*>(runtime_object), index);
}

template <typename Spec>
struct stack_descriptor_holder;

template <typename... Protocols>
struct stack_descriptor_holder<stack_spec<Protocols...>> {
    static const stack_descriptor& get()
    {
        static const stack_descriptor descriptor = {
            sizeof...(Protocols),
            &invoke_stack<Protocols...>,
            &unsafe_layer_ptr_for_stack<Protocols...>,
            &unsafe_protocol_ptr_for_stack<Protocols...>,
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

template <typename... Protocols>
class runtime<stack_spec<Protocols...>> {
public:
    using spec_type           = stack_spec<Protocols...>;
    using request_type        = request<spec_type>;
    using protocol_tuple_type = std::tuple<Protocols...>;

    [[nodiscard]] static execution_state push(request_type& request)
    {
        return push(request, request.owned_protocols());
    }

    [[nodiscard]] static execution_state push(request_type& request, protocol_tuple_type& protocols)
    {
        static_assert(sizeof...(Protocols) > 0, "runtime requires at least one protocol");
        bind(request, protocols);
        return detail::drive_request(request.core(), 0, flow_direction::push);
    }

    template <typename Protocol>
    [[nodiscard]] static execution_state push_from(request_type& request)
    {
        return push_from<Protocol>(request, request.owned_protocols());
    }

    template <typename Protocol>
    [[nodiscard]] static execution_state push_from(request_type& request, protocol_tuple_type& protocols)
    {
        bind(request, protocols);
        return detail::drive_request(request.core(), detail::protocol_index<Protocol, Protocols...>::value,
                                     flow_direction::push);
    }

    [[nodiscard]] static execution_state pop(request_type& request)
    {
        return pop(request, request.owned_protocols());
    }

    [[nodiscard]] static execution_state pop(request_type& request, protocol_tuple_type& protocols)
    {
        static_assert(sizeof...(Protocols) > 0, "runtime requires at least one protocol");
        bind(request, protocols);
        return detail::drive_request(request.core(), sizeof...(Protocols) - 1, flow_direction::pop);
    }

    template <typename Protocol>
    [[nodiscard]] static execution_state pop_from(request_type& request)
    {
        return pop_from<Protocol>(request, request.owned_protocols());
    }

    template <typename Protocol>
    [[nodiscard]] static execution_state pop_from(request_type& request, protocol_tuple_type& protocols)
    {
        bind(request, protocols);
        return detail::drive_request(request.core(), detail::protocol_index<Protocol, Protocols...>::value,
                                     flow_direction::pop);
    }

    [[nodiscard]] static mc::future<void> push_async(request_type& request)
    {
        return push_async(request, request.owned_protocols());
    }

    [[nodiscard]] static mc::future<void> push_async(request_type& request, protocol_tuple_type& protocols)
    {
        bind(request, protocols);
        auto future = request.enable_async_completion();
        detail::finalize_async_result(request.core(), detail::drive_request(request.core(), 0, flow_direction::push));
        return future;
    }

    template <typename Protocol>
    [[nodiscard]] static mc::future<void> push_from_async(request_type& request)
    {
        return push_from_async<Protocol>(request, request.owned_protocols());
    }

    template <typename Protocol>
    [[nodiscard]] static mc::future<void> push_from_async(request_type& request, protocol_tuple_type& protocols)
    {
        bind(request, protocols);
        auto future = request.enable_async_completion();
        detail::finalize_async_result(
            request.core(), detail::drive_request(request.core(), detail::protocol_index<Protocol, Protocols...>::value,
                                                  flow_direction::push));
        return future;
    }

    [[nodiscard]] static mc::future<void> pop_async(request_type& request)
    {
        return pop_async(request, request.owned_protocols());
    }

    [[nodiscard]] static mc::future<void> pop_async(request_type& request, protocol_tuple_type& protocols)
    {
        bind(request, protocols);
        auto future = request.enable_async_completion();
        detail::finalize_async_result(
            request.core(), detail::drive_request(request.core(), sizeof...(Protocols) - 1, flow_direction::pop));
        return future;
    }

    template <typename Protocol>
    [[nodiscard]] static mc::future<void> pop_from_async(request_type& request)
    {
        return pop_from_async<Protocol>(request, request.owned_protocols());
    }

    template <typename Protocol>
    [[nodiscard]] static mc::future<void> pop_from_async(request_type& request, protocol_tuple_type& protocols)
    {
        bind(request, protocols);
        auto future = request.enable_async_completion();
        detail::finalize_async_result(
            request.core(), detail::drive_request(request.core(), detail::protocol_index<Protocol, Protocols...>::value,
                                                  flow_direction::pop));
        return future;
    }

    [[nodiscard]] static execution_state resume(request_type& request)
    {
        return resume(request, request.owned_protocols());
    }

    [[nodiscard]] static execution_state resume(request_type& request, protocol_tuple_type& protocols)
    {
        bind(request, protocols);
        return detail::resume_request(request.core());
    }

private:
    static void bind(request_type& request, protocol_tuple_type& protocols)
    {
        detail::link_protocol_parents(protocols);
        request.bind_runtime(detail::stack_descriptor_for<spec_type>(), &request, &protocols);
    }
};

} // namespace mc::proto

#endif // MC_PROTOCOL_RUNTIME_H
