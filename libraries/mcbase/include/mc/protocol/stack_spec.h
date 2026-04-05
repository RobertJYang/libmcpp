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

namespace mc::protocol {

template <typename... Layers>
struct stack_spec {
    static constexpr std::size_t layer_count = sizeof...(Layers);

    static constexpr std::size_t push_headroom = detail::stack_reserved_room<Layers...>::push_headroom;
    static constexpr std::size_t push_tailroom = detail::stack_reserved_room<Layers...>::push_tailroom;
    static constexpr std::size_t pop_headroom  = detail::stack_reserved_room<Layers...>::pop_headroom;
    static constexpr std::size_t pop_tailroom  = detail::stack_reserved_room<Layers...>::pop_tailroom;
};

namespace detail {

struct empty_state {};

template <typename T>
struct dependent_false : std::false_type {};

template <typename Layer, typename = void>
struct layer_state_type {
    using type = empty_state;
};

template <typename Layer>
struct layer_state_type<Layer, std::void_t<typename Layer::state>> {
    using type = typename Layer::state;
};

template <typename Layer>
using layer_state_t = typename layer_state_type<Layer>::type;

template <typename Layer, typename... Layers>
struct layer_index;

template <typename Layer, typename First, typename... Rest>
struct layer_index_impl;

template <typename Layer, typename First, typename... Rest>
struct layer_index_impl {
    static constexpr std::size_t value = 1 + layer_index<Layer, Rest...>::value;
};

template <typename Layer, typename... Rest>
struct layer_index_impl<Layer, Layer, Rest...> {
    static constexpr std::size_t value = 0;
};

template <typename Layer, typename First, typename... Rest>
struct layer_index<Layer, First, Rest...> {
    static constexpr std::size_t value = layer_index_impl<Layer, First, Rest...>::value;
};

template <typename Layer>
struct layer_index<Layer> {
    static_assert(dependent_false<Layer>::value, "Layer is not part of this stack_spec");
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

} // namespace detail

} // namespace mc::protocol

#endif // MC_PROTOCOL_STACK_SPEC_H
