/*
* Copyright (c) 2024 Huawei Technologies Co., Ltd.
* openUBMC is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*         http://license.coscl.org.cn/MulanPSL2
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
* See the Mulan PSL v2 for more details.
*/

#include <mc/im/byte_buffer.h>
#include <algorithm>

namespace mc::im {

byte_buffer::byte_buffer() {}

void byte_buffer::reset()
{
    m_buf.clear();
}

void byte_buffer::grow(size_t n)
{
    size_t m = grow_internal(n);
    m_buf.resize(m);
}

std::pair<size_t, bool> byte_buffer::try_grow_by_reslice(size_t n)
{
    size_t l = m_buf.size();
    if (l + n <= m_buf.capacity()) {
        m_buf.resize(l + n);
        return {l, true};
    }
    return {0, false};
}

size_t byte_buffer::len() const
{
    return m_buf.size();
}

size_t byte_buffer::cap() const
{
    return m_buf.capacity();
}

const std::vector<uint8_t>& byte_buffer::bytes() const
{
    return m_buf;
}

size_t byte_buffer::grow_internal(size_t n)
{
    size_t m = m_buf.size();
    auto [i, ok] = try_grow_by_reslice(n);
    if (ok) {
        return i;
    }
    
    if (m_buf.empty() && n <= sizeof(m_bootstrap)) {
        m_buf.reserve(n);
        m_buf.resize(n);
        std::memcpy(m_buf.data(), m_bootstrap, n);
        return 0;
    }
    
    size_t new_cap = 2 * m_buf.capacity() + n;
    std::vector<uint8_t> buf;
    buf.reserve(new_cap);
    buf.resize(m + n);
    
    std::copy(m_buf.begin(), m_buf.end(), buf.begin());
    m_buf = std::move(buf);
    
    return m;
}

void byte_buffer::truncate(size_t n)
{
    if (n == 0) {
        reset();
        return;
    }
    
    m_buf.resize(n);
}

bool byte_buffer::write(const uint8_t* p, size_t size)
{
    if (size == 0) {
        return true;
    }
    
    size_t m;
    bool ok;
    std::tie(m, ok) = try_grow_by_reslice(size);
    
    if (!ok) {
        m = grow_internal(size);
    }
    
    std::copy(p, p + size, m_buf.begin() + m);
    return true;
}

bool byte_buffer::write(const std::vector<uint8_t>& p)
{
    return write(p.data(), p.size());
}

bool byte_buffer::write_byte(uint8_t c)
{
    size_t m;
    bool ok;
    std::tie(m, ok) = try_grow_by_reslice(1);
    
    if (!ok) {
        m = grow_internal(1);
    }
    
    m_buf[m] = c;
    return true;
}

bool byte_buffer::write_string(std::string_view v)
{
    return write(reinterpret_cast<const uint8_t*>(v.data()), v.size());
}

bool byte_buffer::write_uint16(uint16_t v)
{
    size_t m;
    bool ok;
    std::tie(m, ok) = try_grow_by_reslice(2);
    
    if (!ok) {
        m = grow_internal(2);
    }
    
    // 大端序写入
    m_buf[m] = static_cast<uint8_t>((v >> 8) & 0xFF);
    m_buf[m + 1] = static_cast<uint8_t>(v & 0xFF);
    
    return true;
}

bool byte_buffer::write_uint32(uint32_t v)
{
    size_t m;
    bool ok;
    std::tie(m, ok) = try_grow_by_reslice(4);
    
    if (!ok) {
        m = grow_internal(4);
    }
    
    // 大端序写入
    m_buf[m] = static_cast<uint8_t>((v >> 24) & 0xFF);
    m_buf[m + 1] = static_cast<uint8_t>((v >> 16) & 0xFF);
    m_buf[m + 2] = static_cast<uint8_t>((v >> 8) & 0xFF);
    m_buf[m + 3] = static_cast<uint8_t>(v & 0xFF);
    
    return true;
}

bool byte_buffer::write_uint64(uint64_t v)
{
    size_t m;
    bool ok;
    std::tie(m, ok) = try_grow_by_reslice(8);
    
    if (!ok) {
        m = grow_internal(8);
    }
    
    // 大端序写入
    m_buf[m] = static_cast<uint8_t>((v >> 56) & 0xFF);
    m_buf[m + 1] = static_cast<uint8_t>((v >> 48) & 0xFF);
    m_buf[m + 2] = static_cast<uint8_t>((v >> 40) & 0xFF);
    m_buf[m + 3] = static_cast<uint8_t>((v >> 32) & 0xFF);
    m_buf[m + 4] = static_cast<uint8_t>((v >> 24) & 0xFF);
    m_buf[m + 5] = static_cast<uint8_t>((v >> 16) & 0xFF);
    m_buf[m + 6] = static_cast<uint8_t>((v >> 8) & 0xFF);
    m_buf[m + 7] = static_cast<uint8_t>(v & 0xFF);
    
    return true;
}

} // namespace mc::im 