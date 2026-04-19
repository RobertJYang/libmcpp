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

#include "mc/shm/container/node_header.h"

#include <array>
#include <cstring>

namespace mc::shm::container {

namespace {

// CRC16-CCITT 字节表（poly 0x1021，init 0xFFFF，不反转）
// 运行期惰性构造以避免启动顺序问题
const std::array<std::uint16_t, 256>& crc16_table() noexcept
{
    static const std::array<std::uint16_t, 256> table = [] {
        std::array<std::uint16_t, 256> t{};
        for (std::uint32_t i = 0; i < 256; ++i) {
            std::uint16_t c = static_cast<std::uint16_t>(i << 8);
            for (int k = 0; k < 8; ++k) {
                c = static_cast<std::uint16_t>((c & 0x8000U) ? ((c << 1) ^ 0x1021U) : (c << 1));
            }
            t[i] = c;
        }
        return t;
    }();
    return table;
}

// 把 header 的静态字段打包到栈上连续 12 字节 buffer，避免结构对齐带来的 padding
void snapshot_static_bytes(const node_header& h, std::uint8_t (&buf)[12]) noexcept
{
    std::memcpy(buf, &h.magic, sizeof(h.magic));
    std::memcpy(buf + sizeof(h.magic), &h.owner_offset, sizeof(h.owner_offset));
}

} // namespace

std::uint16_t crc16_ccitt(const void* data, std::size_t len) noexcept
{
    const auto*         bytes = static_cast<const std::uint8_t*>(data);
    std::uint16_t       crc   = 0xFFFFU;
    const auto&         table = crc16_table();
    for (std::size_t i = 0; i < len; ++i) {
        const std::uint8_t idx = static_cast<std::uint8_t>((crc >> 8) ^ bytes[i]);
        crc                    = static_cast<std::uint16_t>((crc << 8) ^ table[idx]);
    }
    return crc;
}

std::uint16_t node_header_checksum(const node_header& h) noexcept
{
    std::uint8_t buf[12];
    snapshot_static_bytes(h, buf);
    return crc16_ccitt(buf, sizeof(buf));
}

void node_header_init(node_header& h, std::uint64_t magic, std::uint32_t owner_offset) noexcept
{
    h.magic        = magic;
    h.owner_offset = owner_offset;
    h._reserved    = 0;
    h.checksum     = node_header_checksum(h);
    h.state.store(static_cast<std::uint8_t>(node_state::free), std::memory_order_release);
}

void node_header_transition(node_header& h, node_state new_state) noexcept
{
    h.state.store(static_cast<std::uint8_t>(new_state), std::memory_order_release);
}

node_state node_header_load_state(const node_header& h) noexcept
{
    const auto v = h.state.load(std::memory_order_acquire);
    if (v > static_cast<std::uint8_t>(node_state::degraded)) {
        return node_state::degraded;
    }
    return static_cast<node_state>(v);
}

bool node_header_check(const node_header& h) noexcept
{
    if (h.magic == 0) {
        return false;
    }
    if (h.checksum != node_header_checksum(h)) {
        return false;
    }
    const auto s = h.state.load(std::memory_order_acquire);
    if (s > static_cast<std::uint8_t>(node_state::degraded)) {
        return false;
    }
    return true;
}

void node_header_mark_degraded(node_header& h) noexcept
{
    h.state.store(static_cast<std::uint8_t>(node_state::degraded), std::memory_order_release);
}

} // namespace mc::shm::container
