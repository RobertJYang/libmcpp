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
 * 协议栈旁路观测：每层 on_push/on_pop 返回后，先经可选 layer_mask 门闸与 filter，再拷贝缓冲区并 emit。
 * filter 中 bytes 指向当前 packet；emit 中 bytes 指向本次调用内快照，回调返回后失效。
 */

#ifndef MC_PROTOCOL_TRACE_H
#define MC_PROTOCOL_TRACE_H

#include <mc/protocol/common.h>

#include <cstddef>
#include <cstdint>

namespace mc::proto {

/// 第 i 层对应掩码中的第 i 位，仅支持 0..63。
[[nodiscard]] constexpr std::uint64_t layer_bit(std::size_t layer_index) noexcept
{
    return layer_index < 64 ? (std::uint64_t{1} << layer_index) : std::uint64_t{0};
}

struct trace_event {
    std::size_t         layer_index{0};
    flow_direction      direction{flow_direction::push};
    const std::uint8_t* bytes{nullptr};
    std::size_t         length{0};
    std::uint32_t       flags{0};
    std::size_t         payload_base{0};
};

struct trace_sink {
    void* user_data{nullptr};

    /// 为 0 时不限制层；非 0 时仅对应位为 1 的层进入旁路；层号 >= 64 无法用掩码选中。
    std::uint64_t layer_mask{0};

    /// 返回 true 表示需要旁路；为 nullptr 则每一层都输出（仍要求 emit 非空）。
    bool (*filter)(void* user_data, const trace_event& e);

    /// 为 nullptr 表示关闭旁路。
    void (*emit)(void* user_data, const trace_event& e);
};

} // namespace mc::proto

#endif // MC_PROTOCOL_TRACE_H
