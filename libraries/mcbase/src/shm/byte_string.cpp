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

#include "mc/shm/byte_string.h"

#include <algorithm>
#include <cstddef>
#include <cstring>

#include "mc/shm/allocator.h"

#include "detail/region_registry.h"

// buffer 物理布局（与 shm::string 对称，magic 不同）：
//   [0..3]        uint32 magic = "MCBY" (0x5942434D)
//   [4..7]        uint32 size  (payload 字节数)
//   [8..8+size)   payload

namespace mc::shm {

namespace {

constexpr std::size_t k_header_size = byte_string::buffer_header_size;

struct buffer_header {
    std::uint32_t magic;
    std::uint32_t size;
};
static_assert(sizeof(buffer_header) == 8U, "buffer_header 必须 8 字节");

inline const buffer_header* header_from_data(const std::byte* data) noexcept
{
    return data == nullptr ? nullptr : reinterpret_cast<const buffer_header*>(data);
}

inline const std::byte* payload_from_data(const std::byte* data) noexcept
{
    return data == nullptr ? nullptr : data + k_header_size;
}

bool free_buffer_if_intact(shm_allocator& alloc, const std::byte* data,
                           std::uint32_t expected_size) noexcept
{
    if (data == nullptr || expected_size == 0) {
        return false;
    }
    auto* h = const_cast<buffer_header*>(header_from_data(data));
    if (h == nullptr || h->magic != byte_string::buffer_magic || h->size != expected_size) {
        return false;
    }
    h->magic = 0;
    alloc.deallocate(h);
    return true;
}

} // namespace

// ---- Move ----
byte_string::byte_string(byte_string&& other) noexcept
    : m_data(other.m_data.get())
    , m_size(other.m_size)
    , m_capacity(other.m_capacity)
{
    other.m_data.reset();
    other.m_size     = 0;
    other.m_capacity = 0;
}

byte_string& byte_string::operator=(byte_string&& other) noexcept
{
    if (this == &other) {
        return *this;
    }
    destroy();

    m_data.reset(const_cast<std::byte*>(other.m_data.get()));
    m_size     = other.m_size;
    m_capacity = other.m_capacity;

    other.m_data.reset();
    other.m_size     = 0;
    other.m_capacity = 0;
    return *this;
}

byte_string::~byte_string() { destroy(); }

// ---- 工厂 ----
byte_string byte_string::create(shm_allocator& alloc, const void* data, std::size_t size) noexcept
{
    byte_string out;
    if (size == 0) {
        return out;
    }
    if (data == nullptr) {
        return out;
    }

    const std::size_t bytes = k_header_size + size;
    void*             mem   = alloc.allocate(bytes, 8U);
    if (mem == nullptr) {
        return out;
    }

    auto* h  = static_cast<buffer_header*>(mem);
    h->magic = buffer_magic;
    h->size  = static_cast<std::uint32_t>(size);
    std::memcpy(static_cast<std::byte*>(mem) + k_header_size, data, size);

    out.m_data.reset(static_cast<std::byte*>(mem));
    out.m_size     = static_cast<std::uint32_t>(size);
    out.m_capacity = static_cast<std::uint32_t>(size);
    return out;
}

byte_string byte_string::create(shm_allocator& alloc, std::string_view sv) noexcept
{
    return create(alloc, sv.data(), sv.size());
}

byte_string byte_string::clone(shm_allocator& alloc) const noexcept
{
    return create(alloc, data(), m_size);
}

// ---- destroy ----
void byte_string::destroy(shm_allocator& alloc) noexcept
{
    free_buffer_if_intact(alloc, m_data.get(), m_size);
    m_data.reset();
    m_size     = 0;
    m_capacity = 0;
}

void byte_string::destroy() noexcept
{
    auto* data_ptr = m_data.get();
    if (data_ptr == nullptr || m_size == 0) {
        m_data.reset();
        m_size     = 0;
        m_capacity = 0;
        return;
    }

    auto info = detail::find_region_for(data_ptr);
    if (info) {
        shm_allocator alloc(info->user_base, info->user_size);
        free_buffer_if_intact(alloc, data_ptr, m_size);
    }
    m_data.reset();
    m_size     = 0;
    m_capacity = 0;
}

// ---- view ----
const std::byte* byte_string::data() const noexcept
{
    return m_size == 0 ? nullptr : payload_from_data(m_data.get());
}

std::string_view byte_string::as_string_view() const noexcept
{
    if (m_size == 0) {
        return std::string_view{};
    }
    const auto* p = payload_from_data(m_data.get());
    return p == nullptr ? std::string_view{}
                        : std::string_view{reinterpret_cast<const char*>(p), m_size};
}

bool byte_string::buffer_intact() const noexcept
{
    if (m_size == 0) {
        return true;
    }
    const auto* h = header_from_data(m_data.get());
    return h != nullptr && h->magic == buffer_magic && h->size == m_size;
}

// ---- 比较 ----
bool byte_string::operator==(const byte_string& other) const noexcept
{
    if (m_size != other.m_size) {
        return false;
    }
    if (m_size == 0) {
        return true;
    }
    return std::memcmp(payload_from_data(m_data.get()),
                       payload_from_data(other.m_data.get()), m_size)
           == 0;
}

bool byte_string::operator<(const byte_string& other) const noexcept
{
    const std::size_t n = std::min(m_size, other.m_size);
    if (n > 0) {
        const int c = std::memcmp(payload_from_data(m_data.get()),
                                  payload_from_data(other.m_data.get()), n);
        if (c != 0) {
            return c < 0;
        }
    }
    return m_size < other.m_size;
}

bool byte_string::operator==(std::string_view sv) const noexcept
{
    if (m_size != sv.size()) {
        return false;
    }
    if (m_size == 0) {
        return true;
    }
    return std::memcmp(payload_from_data(m_data.get()), sv.data(), m_size) == 0;
}

bool byte_string::operator<(std::string_view sv) const noexcept
{
    const std::size_t n = std::min(static_cast<std::size_t>(m_size), sv.size());
    if (n > 0) {
        const auto* p = payload_from_data(m_data.get());
        const int   c = std::memcmp(p, sv.data(), n);
        if (c != 0) {
            return c < 0;
        }
    }
    return static_cast<std::size_t>(m_size) < sv.size();
}

} // namespace mc::shm
