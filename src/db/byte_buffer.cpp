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

#include <mc/db/byte_buffer.h>

#include <algorithm>

namespace mc::db {

byte_buffer::byte_buffer() : m_size(0), m_capacity(64), m_using_bootstrap(true) {
}

void byte_buffer::reset() {
    // 如果当前使用的是动态分配的缓冲区，则切换回内部缓冲区
    if (!m_using_bootstrap) {
        m_buf.clear();
        m_using_bootstrap = true;
    }
    m_size     = 0;
    m_capacity = 64;
}

void byte_buffer::grow(size_t n) {
    grow_internal(n);
}

std::pair<size_t, bool> byte_buffer::try_grow_by_reslice(size_t n) {
    size_t old_size = m_size;
    if (n <= capacity() - m_size) {
        return {old_size, true};
    }
    return {old_size, false};
}

size_t byte_buffer::len() const {
    return m_size;
}

size_t byte_buffer::cap() const {
    return capacity();
}

size_t byte_buffer::capacity() const {
    return m_using_bootstrap ? 64 : m_capacity;
}

std::string_view byte_buffer::bytes() const {
    if (m_using_bootstrap) {
        return std::string_view(reinterpret_cast<const char*>(m_bootstrap), m_size);
    }
    return std::string_view(reinterpret_cast<const char*>(m_buf.data()), m_size);
}

const uint8_t* byte_buffer::data() const {
    return m_using_bootstrap ? m_bootstrap : m_buf.data();
}

size_t byte_buffer::grow_internal(size_t n) {
    size_t old_size = m_size;
    size_t new_size = old_size + n;

    // 检查是否需要从内部缓冲区切换到动态分配
    if (m_using_bootstrap && new_size > 64) {
        // 切换到动态分配
        m_buf.reserve(new_size * 2); // 预分配额外空间
        m_buf.assign(m_bootstrap, m_bootstrap + old_size);
        m_using_bootstrap = false;
        m_capacity        = m_buf.capacity();
    } else if (!m_using_bootstrap && new_size > m_capacity) {
        // 已经使用动态分配，但需要扩容
        m_buf.reserve(new_size * 2);
        m_capacity = m_buf.capacity();
    }

    // 调整大小
    if (m_using_bootstrap) {
        m_size = new_size;
    } else {
        m_buf.resize(new_size);
        m_size = m_buf.size();
    }

    return old_size;
}

void byte_buffer::truncate(size_t n) {
    if (n >= m_size) {
        return;
    }

    if (m_using_bootstrap) {
        m_size = n;
    } else {
        m_buf.resize(n);
        m_size = m_buf.size();

        // 如果新大小足够小，考虑切换回内部缓冲区
        if (n <= 64) {
            std::memcpy(m_bootstrap, m_buf.data(), n);
            m_buf.clear();
            m_using_bootstrap = true;
            m_capacity        = 64;
        }
    }
}

void byte_buffer::write(const uint8_t* p, size_t size) {
    size_t old_size = grow_internal(size);
    if (m_using_bootstrap) {
        std::memcpy(m_bootstrap + old_size, p, size);
    } else {
        std::memcpy(m_buf.data() + old_size, p, size);
    }
}

void byte_buffer::write(const std::vector<uint8_t>& p) {
    write(p.data(), p.size());
}

void byte_buffer::write_byte(uint8_t c) {
    size_t old_size = grow_internal(1);
    if (m_using_bootstrap) {
        m_bootstrap[old_size] = c;
    } else {
        m_buf[old_size] = c;
    }
}

void byte_buffer::write_string(std::string_view v) {
    write(reinterpret_cast<const uint8_t*>(v.data()), v.size());
}

void byte_buffer::write_uint16(uint16_t v) {
    size_t   old_size = grow_internal(2);
    uint8_t* dest     = m_using_bootstrap ? m_bootstrap + old_size : m_buf.data() + old_size;
    // 大端序写入
    dest[0] = static_cast<uint8_t>((v >> 8) & 0xFF);
    dest[1] = static_cast<uint8_t>(v & 0xFF);
}

void byte_buffer::write_uint32(uint32_t v) {
    size_t   old_size = grow_internal(4);
    uint8_t* dest     = m_using_bootstrap ? m_bootstrap + old_size : m_buf.data() + old_size;
    // 大端序写入
    dest[0] = static_cast<uint8_t>((v >> 24) & 0xFF);
    dest[1] = static_cast<uint8_t>((v >> 16) & 0xFF);
    dest[2] = static_cast<uint8_t>((v >> 8) & 0xFF);
    dest[3] = static_cast<uint8_t>(v & 0xFF);
}

void byte_buffer::write_uint64(uint64_t v) {
    size_t   old_size = grow_internal(8);
    uint8_t* dest     = m_using_bootstrap ? m_bootstrap + old_size : m_buf.data() + old_size;
    // 大端序写入
    dest[0] = static_cast<uint8_t>((v >> 56) & 0xFF);
    dest[1] = static_cast<uint8_t>((v >> 48) & 0xFF);
    dest[2] = static_cast<uint8_t>((v >> 40) & 0xFF);
    dest[3] = static_cast<uint8_t>((v >> 32) & 0xFF);
    dest[4] = static_cast<uint8_t>((v >> 24) & 0xFF);
    dest[5] = static_cast<uint8_t>((v >> 16) & 0xFF);
    dest[6] = static_cast<uint8_t>((v >> 8) & 0xFF);
    dest[7] = static_cast<uint8_t>(v & 0xFF);
}

} // namespace mc::db