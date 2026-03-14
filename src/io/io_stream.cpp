/*
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <algorithm>
#include <limits>
#include <mc/io/io_stream.h>
#include <mc/exception.h>
#include "securec.h"

namespace mc {
namespace io {

io_stream::io_stream() : m_buffer(std::make_unique<io_buffer>())
{}

io_stream::io_stream(std::unique_ptr<io_buffer> buffer, bool writable)
    : m_buffer(std::move(buffer)), m_writable(writable)
{
    if (!m_buffer) {
        m_buffer = std::make_unique<io_buffer>();
    }
}

io_stream::io_stream(std::size_t capacity) : m_buffer(io_buffer::create(capacity))
{}

io_stream::io_stream(io_stream&& other) noexcept
    : m_buffer(std::move(other.m_buffer)), m_read_pos(other.m_read_pos), m_write_pos(other.m_write_pos),
      m_writable(other.m_writable)
{
    other.m_read_pos  = 0;
    other.m_write_pos = 0;
    other.m_writable  = true;
    other.m_buffer    = std::make_unique<io_buffer>();
}

io_stream& io_stream::operator=(io_stream&& other) noexcept
{
    if (this != &other) {
        m_buffer    = std::move(other.m_buffer);
        m_read_pos  = other.m_read_pos;
        m_write_pos = other.m_write_pos;
        m_writable  = other.m_writable;

        other.m_read_pos  = 0;
        other.m_write_pos = 0;
        other.m_writable  = true;
        other.m_buffer    = std::make_unique<io_buffer>();
    }
    return *this;
}

io_stream::~io_stream() = default;

io_buffer* io_stream::get_buffer() const
{
    return m_buffer.get();
}

std::unique_ptr<io_buffer> io_stream::release_buffer()
{
    m_read_pos  = 0;
    m_write_pos = 0;
    auto result = std::move(m_buffer);
    m_buffer    = std::make_unique<io_buffer>();
    return result;
}

void io_stream::reset(std::unique_ptr<io_buffer> buffer, bool writable)
{
    m_buffer = std::move(buffer);
    if (!m_buffer) {
        m_buffer = std::make_unique<io_buffer>();
    }
    m_read_pos  = 0;
    m_write_pos = 0;
    m_writable  = writable;
}

std::size_t io_stream::get_read_pos() const noexcept
{
    return m_read_pos;
}

std::size_t io_stream::get_write_pos() const noexcept
{
    return m_write_pos;
}

std::size_t io_stream::length() const noexcept
{
    return m_buffer->length();
}

std::size_t io_stream::readable_bytes() const noexcept
{
    if (m_read_pos >= m_buffer->length()) {
        return 0;
    }

    return m_buffer->length() - m_read_pos;
}

std::size_t io_stream::written_bytes() const noexcept
{
    return m_write_pos;
}

bool io_stream::has_remaining(std::size_t length) const noexcept
{
    return m_read_pos + length <= m_buffer->length();
}

std::size_t io_stream::seek_read(std::int64_t pos, seek_mode mode)
{
    std::int64_t new_pos = 0;

    switch (mode) {
        case seek_mode::begin:
            new_pos = pos;
            break;
        case seek_mode::current:
            new_pos = static_cast<std::int64_t>(m_read_pos) + pos;
            break;
        case seek_mode::end:
            new_pos = static_cast<std::int64_t>(m_buffer->length()) + pos;
            break;
    }

    // 检查边界
    if (new_pos < 0) {
        new_pos = 0;
    } else if (new_pos > static_cast<std::int64_t>(m_buffer->length())) {
        new_pos = static_cast<std::int64_t>(m_buffer->length());
    }

    m_read_pos = static_cast<std::size_t>(new_pos);
    return m_read_pos;
}

std::size_t io_stream::seek_write(std::int64_t pos, seek_mode mode)
{
    ensure_writable();

    std::int64_t new_pos = 0;

    switch (mode) {
        case seek_mode::begin:
            new_pos = pos;
            break;
        case seek_mode::current:
            new_pos = static_cast<std::int64_t>(m_write_pos) + pos;
            break;
        case seek_mode::end:
            new_pos = static_cast<std::int64_t>(m_buffer->length()) + pos;
            break;
    }

    // 检查边界
    if (new_pos < 0) {
        const std::size_t head_size = static_cast<std::size_t>(std::abs(new_pos));
        if (m_buffer->headroom() < head_size) {
            m_buffer->reserve(head_size, 0);
        }
        m_buffer->prepend(head_size);
        new_pos = 0;
    }

    // 如果写入位置超出当前长度，需要扩展缓冲区
    if (new_pos > static_cast<std::int64_t>(m_buffer->length())) {
        std::size_t additional = static_cast<std::size_t>(new_pos) - m_buffer->length();
        if (!m_buffer->ensure_tailroom(additional)) {
            MC_THROW(mc::bad_alloc_exception, "无法扩展缓冲区");
        }
        m_buffer->append(additional);
    }

    m_write_pos = static_cast<std::size_t>(new_pos);
    return m_write_pos;
}

std::size_t io_stream::skip(std::size_t skip_size)
{
    std::size_t available   = readable_bytes();
    std::size_t actual_skip = std::min(skip_size, available);
    m_read_pos += actual_skip;
    return actual_skip;
}

std::size_t io_stream::read(void* data, std::size_t length)
{
    auto result = m_buffer->read(m_read_pos, data, length);
    m_read_pos += result;
    return result;
}

std::string_view io_stream::read(std::size_t length)
{
    std::string_view result = m_buffer->read(m_read_pos, length);
    m_read_pos += length;
    return result;
}

std::size_t io_stream::read_some(void* data, std::size_t length)
{
    auto result = m_buffer->read_some(m_read_pos, data, length);
    m_read_pos += result;
    return result;
}

std::string_view io_stream::read_some(std::size_t length)
{
    std::string_view result = m_buffer->read_some(m_read_pos, length);
    m_read_pos += length;
    return result;
}

bool io_stream::try_read(void* data, std::size_t length)
{
    if (m_buffer->try_read(m_read_pos, data, length)) {
        m_read_pos += length;
        return true;
    }
    return false;
}

std::string_view io_stream::try_read(std::size_t length)
{
    std::string_view result = m_buffer->try_read(m_read_pos, length);
    if (result.empty()) {
        return {};
    }
    m_read_pos += length;
    return result;
}

std::size_t io_stream::write(const void* data, std::size_t length)
{
    ensure_writable();

    m_buffer->write_at_offset(m_write_pos, data, length);
    m_write_pos += length;
    return length;
}

std::size_t io_stream::write(std::string_view str)
{
    return write(str.data(), str.size());
}

void io_stream::clear()
{
    m_buffer->clear();
    m_read_pos  = 0;
    m_write_pos = 0;
}

std::size_t io_stream::get_headroom() const noexcept
{
    return m_buffer->headroom();
}

std::size_t io_stream::align(std::size_t alignment)
{
    ensure_writable();

    if (alignment <= 1) {
        return 0;
    }

    std::size_t padding = 0;
    if (m_write_pos == m_buffer->length()) {
        padding = m_buffer->align(alignment);
        m_write_pos += padding;
    } else {
        // 如果写入位置不是在缓存末尾，需要计算需要填充的字节数
        auto current_mod = m_write_pos % alignment;
        if (current_mod == 0) {
            return 0;
        }

        padding = alignment - current_mod;
        if (padding > m_buffer->tailroom()) {
            m_buffer->reserve(m_buffer->headroom(), padding);
        }

        errno_t ret = memset_s(m_buffer->mutable_data() + m_write_pos, m_buffer->tailroom(), 0, padding);
        if (ret != EOK) {
            MC_THROW(mc::runtime_exception, "memset_s failed with error code ${code}", ("code", ret));
        }
        m_write_pos += padding;
    }

    return padding;
}

std::size_t io_stream::align_read(std::size_t alignment)
{
    auto padding = try_align_read(alignment);
    if (!padding) {
        MC_THROW(mc::eof_exception, "无法对齐读取位置");
    }

    return padding.value();
}

std::optional<std::size_t> io_stream::try_align_read(std::size_t alignment)
{
    auto current_mod = m_read_pos % alignment;
    if (current_mod == 0) {
        return 0;
    }

    std::size_t padding = alignment - current_mod;
    if (padding > readable_bytes()) {
        return std::nullopt;
    }

    m_read_pos += padding;
    return padding;
}

std::size_t io_stream::get_tailroom() const noexcept
{
    return m_buffer->tailroom();
}

void io_stream::reserve(std::size_t headroom, std::size_t tailroom)
{
    m_buffer->reserve(headroom, tailroom);
}

void io_stream::ensure_writable() const
{
    if (!m_writable) {
        MC_THROW(mc::eof_exception, "流不可写");
    }
}

std::string_view io_stream::peek(std::size_t max_length) const
{
    std::size_t length = std::min(readable_bytes(), max_length);
    if (length == 0) {
        return {};
    }

    auto data = m_buffer->data() + m_read_pos;
    return std::string_view(reinterpret_cast<const char*>(data), std::min(length, max_length));
}

std::string_view io_stream::get_data() const
{
    return std::string_view(reinterpret_cast<const char*>(m_buffer->data()), m_buffer->length());
}

std::string_view io_stream::get_writeable_data(std::size_t min_length)
{
    if (m_read_pos == m_write_pos) {
        m_read_pos  = 0;
        m_write_pos = 0;
    }

    std::size_t length = get_tailroom();
    if (length < min_length) {
        m_buffer->reserve(get_headroom(), min_length);
    }

    auto data = m_buffer->mutable_data() + m_write_pos;
    return {reinterpret_cast<char*>(data), length};
}

} // namespace io
} // namespace mc