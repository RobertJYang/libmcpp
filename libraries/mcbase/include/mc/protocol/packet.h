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

#ifndef MC_PROTOCOL_PACKET_H
#define MC_PROTOCOL_PACKET_H

#include <mc/io/io_buffer.h>

#include <cstddef>
#include <cstdint>

namespace mc::proto {
namespace detail {
struct packet_access;
struct buffer_access;
} // namespace detail

class buffer {
public:
    buffer() = default;

    void clear() noexcept
    {
        m_buffer.clear();
        m_payload_base = 0;
    }

    std::size_t length() const noexcept
    {
        return m_buffer.length();
    }

    const std::uint8_t* data() const noexcept
    {
        return m_buffer.data();
    }

    std::uint8_t* mutable_data() noexcept
    {
        return m_buffer.mutable_data();
    }

    void prepend(std::size_t amount) noexcept
    {
        m_buffer.prepend(amount);
    }

    std::size_t write(const void* data, std::size_t len)
    {
        return m_buffer.write(data, len);
    }

    std::size_t write_at_offset(std::size_t offset, const void* data, std::size_t len)
    {
        return m_buffer.write_at_offset(offset, data, len);
    }

    std::size_t payload_base() const noexcept
    {
        return m_payload_base;
    }

    buffer& append_payload(const void* data, std::size_t len)
    {
        const std::size_t off = length() < m_payload_base ? m_payload_base : length();
        write_at_offset(off, data, len);
        return *this;
    }

    template <typename T, std::size_t N>
    buffer& append_payload(const T (&arr)[N])
    {
        return append_payload(static_cast<const void*>(arr), N * sizeof(T));
    }

    void strip_prefix(std::size_t n) noexcept
    {
        m_buffer.trim_start(n);
    }

    void strip_suffix(std::size_t n) noexcept
    {
        m_buffer.trim_end(n);
    }

    void*         native_handle{nullptr};
    std::uint32_t flags{0};

private:
    friend struct detail::packet_access;
    friend struct detail::buffer_access;

    void rearm(std::size_t min_headroom, std::size_t min_tailroom, std::size_t payload_base) noexcept
    {
        m_buffer.clear();
        m_buffer.reserve(min_headroom, min_tailroom);
        m_payload_base = payload_base;
    }

    mc::io::io_buffer m_buffer;
    std::size_t       m_payload_base{0};
};

using packet = buffer;

namespace detail {

struct buffer_access {
    static void arm(buffer& b, std::size_t headroom, std::size_t tailroom, std::size_t payload_base) noexcept
    {
        b.rearm(headroom, tailroom, payload_base);
    }
};

struct packet_access {
    static void arm(packet& p, std::size_t headroom, std::size_t tailroom, std::size_t payload_base) noexcept
    {
        buffer_access::arm(p, headroom, tailroom, payload_base);
    }
};

} // namespace detail

} // namespace mc::proto

#endif // MC_PROTOCOL_PACKET_H
