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

#ifndef MC_PROTOCOL_STACK_SPEC_H
#define MC_PROTOCOL_STACK_SPEC_H

#include <mc/common.h>
#include <mc/protocol/detail/layer_packet_reserve.h>

#include <cstddef>
#include <tuple>
#include <type_traits>

namespace mc::proto {

template <typename... Protocols>
struct stack_spec {
    static_assert((std::is_base_of_v<protocol, Protocols> && ...),
                  "stack_spec only accepts mc::proto::protocol derived types");

    static constexpr std::size_t layer_count = sizeof...(Protocols);

    static constexpr std::size_t push_headroom = detail::stack_reserved_room<Protocols...>::push_headroom;
    static constexpr std::size_t push_tailroom = detail::stack_reserved_room<Protocols...>::push_tailroom;
    static constexpr std::size_t pop_headroom  = detail::stack_reserved_room<Protocols...>::pop_headroom;
    static constexpr std::size_t pop_tailroom  = detail::stack_reserved_room<Protocols...>::pop_tailroom;
};

namespace detail {

struct empty_packet {};

using empty_state = empty_packet;

struct empty_context {};

template <typename T>
struct dependent_false : std::false_type {};

template <typename Protocol, typename = void>
struct protocol_packet_type_from_state {
    using type = empty_packet;
};

template <typename Protocol>
struct protocol_packet_type_from_state<Protocol, std::void_t<typename Protocol::state>> {
    using type = typename Protocol::state;
};

template <typename Protocol, typename = void>
struct protocol_packet_type_from_request_state {
    using type = typename protocol_packet_type_from_state<Protocol>::type;
};

template <typename Protocol>
struct protocol_packet_type_from_request_state<Protocol, std::void_t<typename Protocol::request_state>> {
    using type = typename Protocol::request_state;
};

template <typename Protocol, typename = void>
struct protocol_packet_type {
    using type = typename protocol_packet_type_from_request_state<Protocol>::type;
};

template <typename Protocol>
struct protocol_packet_type<Protocol, std::void_t<typename Protocol::packet>> {
    using type = typename Protocol::packet;
};

template <typename Protocol>
using protocol_packet_t = typename protocol_packet_type<Protocol>::type;

template <typename Protocol>
using protocol_request_t = protocol_packet_t<Protocol>;

template <typename Protocol, typename = void>
struct protocol_context_type {
    using type = empty_context;
};

template <typename Protocol>
struct protocol_context_type<Protocol, std::void_t<typename Protocol::context>> {
    using type = typename Protocol::context;
};

template <typename Protocol>
using protocol_context_t = typename protocol_context_type<Protocol>::type;

template <typename Protocol, typename... Protocols>
struct protocol_index;

template <typename Protocol, typename First, typename... Rest>
struct protocol_index_impl;

template <typename Protocol, typename First, typename... Rest>
struct protocol_index_impl {
    static constexpr std::size_t value = 1 + protocol_index<Protocol, Rest...>::value;
};

template <typename Protocol, typename... Rest>
struct protocol_index_impl<Protocol, Protocol, Rest...> {
    static constexpr std::size_t value = 0;
};

template <typename Protocol, typename First, typename... Rest>
struct protocol_index<Protocol, First, Rest...> {
    static constexpr std::size_t value = protocol_index_impl<Protocol, First, Rest...>::value;
};

template <typename Protocol>
struct protocol_index<Protocol> {
    static_assert(dependent_false<Protocol>::value, "Protocol is not part of this stack_spec");
};

template <typename Tuple, std::size_t Index = 0>
inline void* tuple_ptr_at(Tuple& tuple, std::size_t index) noexcept
{
    if constexpr (Index < std::tuple_size_v<std::remove_reference_t<Tuple>>) {
        if (Index == index) {
            return static_cast<void*>(&std::get<Index>(tuple));
        }
        return tuple_ptr_at<Tuple, Index + 1>(tuple, index);
    }
    MC_UNUSED(tuple);
    MC_UNUSED(index);
    return nullptr;
}

template <typename Tuple, std::size_t Index = 0>
inline const void* tuple_ptr_at_const(const Tuple& tuple, std::size_t index) noexcept
{
    if constexpr (Index < std::tuple_size_v<std::remove_reference_t<Tuple>>) {
        if (Index == index) {
            return static_cast<const void*>(&std::get<Index>(tuple));
        }
        return tuple_ptr_at_const<Tuple, Index + 1>(tuple, index);
    }
    MC_UNUSED(tuple);
    MC_UNUSED(index);
    return nullptr;
}

template <typename Protocol>
using layer_state_t = protocol_request_t<Protocol>;

template <typename Protocol, typename... Protocols>
using layer_index = protocol_index<Protocol, Protocols...>;

} // namespace detail

} // namespace mc::proto

#endif // MC_PROTOCOL_STACK_SPEC_H
