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

#include "mc/shm/string.h"

#include <algorithm>
#include <cstddef>
#include <cstring>

#include "mc/shm/allocator.h"

#include "detail/region_registry.h"

// buffer 物理布局：
//   [0..3]        uint32 magic = "MCSS" (0x5353434D)
//   [4..7]        uint32 size  (payload 字节数)
//   [8..8+size)   payload
//
// 前 8 字节用作 header；size == 0 时 string 不分配 buffer

namespace mc::shm {

namespace {

constexpr std::size_t k_header_size = string::buffer_header_size;

struct buffer_header {
    std::uint32_t magic;
    std::uint32_t size;
};
static_assert(sizeof(buffer_header) == 8U, "buffer_header 必须 8 字节");

inline const buffer_header* header_from_data(const std::byte* data) noexcept
{
    return data == nullptr ? nullptr : reinterpret_cast<const buffer_header*>(data);
}

inline const char* payload_from_data(const std::byte* data) noexcept
{
    return data == nullptr ? nullptr
                           : reinterpret_cast<const char*>(data + k_header_size);
}

// 核心 free 路径：返回是否成功释放（magic 校验通过）
// 不修改 string 自身字段；调用者负责后续清零
bool free_buffer_if_intact(shm_allocator& alloc, const std::byte* data,
                           std::uint32_t expected_size) noexcept
{
    if (data == nullptr || expected_size == 0) {
        return false;
    }
    auto* h = const_cast<buffer_header*>(header_from_data(data));
    if (h == nullptr || h->magic != string::buffer_magic || h->size != expected_size) {
        return false;
    }
    // 清 magic 后 free，防止并发多个 string 别名 double-free
    h->magic = 0;
    alloc.deallocate(h);
    return true;
}

} // namespace

// ---- Move ----
string::string(string&& other) noexcept
    // offset_ptr 的 copy ctor 走 reset(other.get())：根据 this 新地址重算 offset
    : m_data(other.m_data.get())
    , m_size(other.m_size)
    , m_capacity(other.m_capacity)
{
    other.m_data.reset();
    other.m_size     = 0;
    other.m_capacity = 0;
}

string& string::operator=(string&& other) noexcept
{
    if (this == &other) {
        return *this;
    }
    // 先释放自己持有的 buffer（避免覆盖导致 leak）
    destroy();

    m_data.reset(const_cast<std::byte*>(other.m_data.get()));
    m_size     = other.m_size;
    m_capacity = other.m_capacity;

    other.m_data.reset();
    other.m_size     = 0;
    other.m_capacity = 0;
    return *this;
}

// ---- 析构 ----
string::~string() { destroy(); }

// ---- 工厂 ----
string string::create(shm_allocator& alloc, std::string_view sv) noexcept
{
    string out;
    if (sv.empty()) {
        return out;
    }

    const std::size_t bytes = k_header_size + sv.size();
    void*             mem   = alloc.allocate(bytes, 8U);
    if (mem == nullptr) {
        return out;
    }

    auto* h  = static_cast<buffer_header*>(mem);
    h->magic = buffer_magic;
    h->size  = static_cast<std::uint32_t>(sv.size());
    std::memcpy(static_cast<std::byte*>(mem) + k_header_size, sv.data(), sv.size());

    out.m_data.reset(static_cast<std::byte*>(mem));
    out.m_size     = static_cast<std::uint32_t>(sv.size());
    out.m_capacity = static_cast<std::uint32_t>(sv.size());
    return out;
}

string string::clone(shm_allocator& alloc) const noexcept
{
    return create(alloc, std_view());
}

// ---- destroy ----
void string::destroy(shm_allocator& alloc) noexcept
{
    free_buffer_if_intact(alloc, m_data.get(), m_size);
    m_data.reset();
    m_size     = 0;
    m_capacity = 0;
}

void string::destroy() noexcept
{
    auto* data = m_data.get();
    if (data == nullptr || m_size == 0) {
        m_data.reset();
        m_size     = 0;
        m_capacity = 0;
        return;
    }

    auto info = detail::find_region_for(data);
    if (info) {
        shm_allocator alloc(info->user_base, info->user_size);
        free_buffer_if_intact(alloc, data, m_size);
    }
    // 否则静默清零（buffer 成 leaked，由 region-level fsck 后续回收）
    m_data.reset();
    m_size     = 0;
    m_capacity = 0;
}

// ---- view ----
string_view string::view() const noexcept
{
    if (m_size == 0) {
        return string_view{};
    }
    const char* p = payload_from_data(m_data.get());
    return p == nullptr ? string_view{} : string_view{p, m_size};
}

std::string_view string::std_view() const noexcept
{
    if (m_size == 0) {
        return std::string_view{};
    }
    const char* p = payload_from_data(m_data.get());
    return p == nullptr ? std::string_view{} : std::string_view{p, m_size};
}

const char* string::data() const noexcept
{
    return m_size == 0 ? nullptr : payload_from_data(m_data.get());
}

bool string::buffer_intact() const noexcept
{
    if (m_size == 0) {
        return true;
    }
    const auto* h = header_from_data(m_data.get());
    return h != nullptr && h->magic == buffer_magic && h->size == m_size;
}

// ---- 比较 ----
bool string::operator==(const string& other) const noexcept
{
    if (m_size != other.m_size) {
        return false;
    }
    if (m_size == 0) {
        return true;
    }
    return std::memcmp(payload_from_data(m_data.get()),
                       payload_from_data(other.m_data.get()), m_size) == 0;
}

bool string::operator<(const string& other) const noexcept
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

bool string::operator==(std::string_view sv) const noexcept
{
    if (m_size != sv.size()) {
        return false;
    }
    if (m_size == 0) {
        return true;
    }
    return std::memcmp(payload_from_data(m_data.get()), sv.data(), m_size) == 0;
}

bool string::operator<(std::string_view sv) const noexcept
{
    const std::size_t n = std::min(static_cast<std::size_t>(m_size), sv.size());
    if (n > 0) {
        const char* p = payload_from_data(m_data.get());
        const int   c = std::memcmp(p, sv.data(), n);
        if (c != 0) {
            return c < 0;
        }
    }
    return static_cast<std::size_t>(m_size) < sv.size();
}

} // namespace mc::shm
