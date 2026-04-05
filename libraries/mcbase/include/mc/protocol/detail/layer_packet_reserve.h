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

/*
 * 协议层对 io_buffer 的预留字节数（编译期探测），分别用于出站 push 与入站 pop。
 * 各层可选声明 static constexpr push_headroom、push_tailroom、pop_headroom、pop_tailroom；
 * 未声明视为 0。仅供 stack_spec 聚合，应用代码通过 stack_spec 与 prepare_* 使用。
 */

#ifndef MC_PROTOCOL_DETAIL_LAYER_PACKET_RESERVE_H
#define MC_PROTOCOL_DETAIL_LAYER_PACKET_RESERVE_H

#include <cstddef>
#include <type_traits>

namespace mc::protocol::detail {

template <typename Layer, typename = void>
struct layer_push_headroom {
    static constexpr std::size_t value = 0;
};

template <typename Layer>
struct layer_push_headroom<Layer, std::void_t<decltype(Layer::push_headroom)>> {
    static constexpr std::size_t value = Layer::push_headroom;
};

template <typename Layer, typename = void>
struct layer_push_tailroom {
    static constexpr std::size_t value = 0;
};

template <typename Layer>
struct layer_push_tailroom<Layer, std::void_t<decltype(Layer::push_tailroom)>> {
    static constexpr std::size_t value = Layer::push_tailroom;
};

template <typename Layer, typename = void>
struct layer_pop_headroom {
    static constexpr std::size_t value = 0;
};

template <typename Layer>
struct layer_pop_headroom<Layer, std::void_t<decltype(Layer::pop_headroom)>> {
    static constexpr std::size_t value = Layer::pop_headroom;
};

template <typename Layer, typename = void>
struct layer_pop_tailroom {
    static constexpr std::size_t value = 0;
};

template <typename Layer>
struct layer_pop_tailroom<Layer, std::void_t<decltype(Layer::pop_tailroom)>> {
    static constexpr std::size_t value = Layer::pop_tailroom;
};

template <typename... Layers>
struct stack_reserved_room {
    static constexpr std::size_t push_headroom = (layer_push_headroom<Layers>::value + ... + 0);
    static constexpr std::size_t push_tailroom = (layer_push_tailroom<Layers>::value + ... + 0);
    static constexpr std::size_t pop_headroom  = (layer_pop_headroom<Layers>::value + ... + 0);
    static constexpr std::size_t pop_tailroom  = (layer_pop_tailroom<Layers>::value + ... + 0);
};

} // namespace mc::protocol::detail

#endif // MC_PROTOCOL_DETAIL_LAYER_PACKET_RESERVE_H
