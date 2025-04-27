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

#include <mc/common.h>
#include <mc/exception.h>
#include <mc/io/io_buffer.h>

#include <algorithm>
#include <new>

namespace mc {
namespace io {

io_buffer::io_buffer() noexcept
    : m_data(nullptr), m_length(0), m_capacity(0), m_next(this), m_prev(this) {
}

io_buffer::io_buffer(io_buffer&& other) noexcept
    : m_buffer(std::move(other.m_buffer)), m_data(other.m_data), m_length(other.m_length),
      m_capacity(other.m_capacity), m_next(other.m_next == &other ? this : other.m_next),
      m_prev(other.m_prev == &other ? this : other.m_prev) {
    if (m_next != this) {
        m_next->m_prev = this;
        m_prev->m_next = this;
    }

    other.m_data     = nullptr;
    other.m_length   = 0;
    other.m_capacity = 0;
    other.m_next     = &other;
    other.m_prev     = &other;
}

io_buffer& io_buffer::operator=(io_buffer&& other) noexcept {
    if (this != &other) {
        m_buffer = std::move(other.m_buffer);
        free_chain();

        m_data     = other.m_data;
        m_length   = other.m_length;
        m_capacity = other.m_capacity;

        if (other.m_next == &other) {
            m_next = this;
            m_prev = this;
        } else {
            m_next         = other.m_next;
            m_prev         = other.m_prev;
            m_next->m_prev = this;
            m_prev->m_next = this;
        }

        other.m_data     = nullptr;
        other.m_length   = 0;
        other.m_capacity = 0;
        other.m_next     = &other;
        other.m_prev     = &other;
    }
    return *this;
}

io_buffer::io_buffer(const io_buffer& other)
    : m_buffer(other.m_buffer), m_data(other.m_data), m_length(other.m_length),
      m_capacity(other.m_capacity), m_next(this), m_prev(this) {
    if (other.m_next != &other) {
        const io_buffer* current = &other;
        io_buffer*       tail    = this;

        do {
            current = current->m_next;
            if (current == &other) {
                break;
            }

            auto new_node    = std::make_unique<io_buffer>(*current);
            tail->m_next     = new_node.get();
            new_node->m_prev = tail;
            tail             = new_node.release();

        } while (true);

        tail->m_next = this;
        m_prev       = tail;
    }
}

io_buffer& io_buffer::operator=(const io_buffer& other) {
    if (this != &other) {
        io_buffer temp(other);
        *this = std::move(temp);
    }
    return *this;
}

io_buffer::~io_buffer() {
    free_chain();
}

void io_buffer::free_chain() {
    if (m_next == this) {
        return;
    }

    io_buffer* buf = m_next;
    io_buffer* end = this;

    m_next = this; // 断开链接
    m_prev = this;

    while (buf != end) {
        io_buffer* next = buf->m_next;
        buf->m_next     = buf;
        buf->m_prev     = buf;
        delete buf;
        buf = next;
    }
}

io_buffer::io_buffer(std::size_t capacity) : m_next(this), m_prev(this) {
    allocate(capacity);
}

io_buffer::io_buffer(void* buf, std::size_t capacity, std::size_t length, free_function fn,
                     void* user_data)
    : m_buffer(buf, fn, user_data), m_data(static_cast<uint8_t*>(buf)), m_length(length),
      m_capacity(capacity), m_next(this), m_prev(this) {
}

std::unique_ptr<io_buffer> io_buffer::create(std::size_t capacity) {
    return std::unique_ptr<io_buffer>(new io_buffer(capacity));
}

std::unique_ptr<io_buffer> io_buffer::take_ownership(void* buf, std::size_t capacity,
                                                     std::size_t length, free_function free_fn,
                                                     void* user_data) {
    if (!buf) {
        MC_THROW(mc::invalid_arg_exception, "缓冲区指针为空");
    }

    if (length > capacity) {
        MC_THROW(mc::invalid_arg_exception, "长度超出容量范围");
    }

    return std::unique_ptr<io_buffer>(new io_buffer(buf, capacity, length, free_fn, user_data));
}

std::unique_ptr<io_buffer> io_buffer::wrap(const void* buf, std::size_t length) {
    if (!buf && length > 0) {
        MC_THROW(mc::invalid_arg_exception, "缓冲区指针为空但长度大于0");
    }

    return std::unique_ptr<io_buffer>(
        new io_buffer(const_cast<void*>(buf), length, length, nullptr, nullptr));
}

std::unique_ptr<io_buffer> io_buffer::copy_buffer(const void* data, std::size_t length,
                                                  std::size_t headroom, std::size_t tailroom) {
    if (!data && length > 0) {
        MC_THROW(mc::invalid_arg_exception, "数据指针为空但长度大于0");
    }

    auto buf    = create(headroom + length + tailroom);
    buf->m_data = buf->m_buffer.data() + headroom;

    if (length > 0) {
        std::memcpy(buf->m_data, data, length);
        buf->m_length = length;
    }

    return buf;
}

// 判断缓冲区是否被共享
bool io_buffer::is_shared() const noexcept {
    return m_buffer.is_shared();
}

void io_buffer::allocate(std::size_t capacity) {
    if (capacity == 0) {
        m_buffer   = detail::shard_buffer();
        m_data     = nullptr;
        m_length   = 0;
        m_capacity = 0;
        return;
    }

    std::size_t alloc_size = MC_ALIGN_UP(capacity, IO_BUFFER_DEFAULT_ALIGNMENT);
    auto        buf        = static_cast<uint8_t*>(std::malloc(alloc_size));
    if (!buf) {
        MC_THROW(mc::bad_alloc_exception, "内存分配失败");
    }

    m_buffer   = detail::shard_buffer(buf, io::io_buffer::default_free, nullptr);
    m_data     = buf;
    m_length   = 0;
    m_capacity = alloc_size;
}

void io_buffer::append(std::size_t amount) noexcept {
    if (amount == 0 || tailroom() < amount) {
        return;
    }
    m_length += amount;
}

void io_buffer::prepend(std::size_t amount) noexcept {
    if (amount == 0 || headroom() < amount) {
        return;
    }
    m_data -= amount;
    m_length += amount;
}

void io_buffer::trim_end(std::size_t amount) noexcept {
    if (amount >= m_length) {
        m_length = 0;
    } else {
        m_length -= amount;
    }
}

void io_buffer::trim_start(std::size_t amount) noexcept {
    if (amount >= m_length) {
        m_data += m_length;
        m_length = 0;
    } else {
        m_data += amount;
        m_length -= amount;
    }
}

// 清空数据
void io_buffer::clear() noexcept {
    m_data   = m_buffer.data();
    m_length = 0;
}

void io_buffer::reserve(std::size_t min_headroom, std::size_t min_tailroom) {
    if (headroom() >= min_headroom && tailroom() >= min_tailroom) {
        return;
    }

    // 需要扩容的场景，按当前容量的两倍扩容
    std::size_t need_capacity = m_length + min_headroom + min_tailroom;
    if (need_capacity <= m_capacity) {
        if (m_length == 0) {
            m_data = m_buffer.data() + min_headroom;
            return;
        }

        uint8_t* new_data_pos = m_buffer.data() + min_headroom;
        if (new_data_pos != m_data) {
            std::memmove(new_data_pos, m_data, m_length);
            m_data = new_data_pos;
        }
        return;
    }

    // 如果扩容后的容量大于当前容量，则按当前容量的两倍扩容
    std::size_t new_capacity = m_capacity ? (m_capacity * 2) : IO_BUFFER_DEFAULT_ALIGNMENT;
    while (new_capacity < need_capacity) {
        new_capacity *= 2;
    }

    auto new_buf    = create(new_capacity);
    new_buf->m_data = new_buf->m_buffer.data() + min_headroom;

    if (m_length > 0) {
        std::memcpy(new_buf->m_data, m_data, m_length);
        new_buf->m_length = m_length;
    }

    *this = std::move(*new_buf);
}

// 数据对齐到指定边界
std::size_t io_buffer::align(std::size_t alignment) {
    if (alignment <= 1) {
        return 0;
    }

    auto current_mod = m_length % alignment;
    if (current_mod == 0) {
        return 0; // 已经对齐
    }

    auto padding = alignment - current_mod;
    if (tailroom() < padding) {
        reserve(headroom(), padding);
    }

    std::memset(m_data + m_length, 0, padding);
    m_length += padding;

    return padding;
}

std::size_t io_buffer::write_at_offset(std::size_t offset, const void* data, std::size_t length) {
    if (length == 0) {
        return 0;
    }

    if (is_shared()) {
        unshare();
    }

    std::size_t end_pos = offset + length;
    if (end_pos > m_length) {
        std::size_t additional = end_pos - m_length;
        if (!ensure_tailroom(additional)) {
            MC_THROW(mc::bad_alloc_exception, "无法扩展缓冲区");
        }

        m_length += additional;
    }

    std::memcpy(m_data + offset, data, length);
    return length;
}

std::size_t io_buffer::write(const void* data, std::size_t length) {
    if (length == 0) {
        return 0;
    }

    if (is_shared()) {
        unshare();
    }

    if (tailroom() < length && !ensure_tailroom(length)) {
        MC_THROW(mc::bad_alloc_exception, "无法扩展缓冲区");
    }

    std::memcpy(m_data + m_length, data, length);
    m_length += length;

    return length;
}

std::string_view io_buffer::read(std::size_t offset, std::size_t length) const {
    if (offset + length > m_length) {
        MC_THROW(mc::out_of_range_exception, "读取位置超出缓冲区范围");
    }

    return std::string_view(reinterpret_cast<const char*>(m_data + offset), length);
}

std::size_t io_buffer::read(std::size_t offset, void* data, std::size_t length) const {
    if (data == nullptr) {
        MC_THROW(mc::invalid_arg_exception, "数据指针为空");
    }

    if (offset + length > m_length) {
        MC_THROW(mc::out_of_range_exception, "读取位置超出缓冲区范围");
    }

    std::memcpy(data, m_data + offset, length);
    return length;
}

bool io_buffer::try_read(std::size_t offset, void* data, std::size_t length) const noexcept {
    if (data == nullptr || offset + length > m_length) {
        return false;
    }

    std::memcpy(data, m_data + offset, length);
    return true;
}

std::string_view io_buffer::try_read(std::size_t offset, std::size_t length) const noexcept {
    if (offset + length > m_length) {
        return {};
    }

    return std::string_view(reinterpret_cast<const char*>(m_data + offset), length);
}

std::string_view io_buffer::read_some(std::size_t offset, std::size_t length) const {
    if (offset >= m_length) {
        return {};
    }

    std::size_t read_length = std::min(length, m_length - offset);
    return std::string_view(reinterpret_cast<const char*>(m_data + offset), read_length);
}

std::size_t io_buffer::read_some(std::size_t offset, void* data, std::size_t length) const {
    if (data == nullptr) {
        MC_THROW(mc::invalid_arg_exception, "数据指针为空");
    }

    if (offset >= m_length) {
        return 0;
    }

    std::size_t read_length = std::min(length, m_length - offset);
    std::memcpy(data, m_data + offset, read_length);
    return read_length;
}

bool io_buffer::ensure_tailroom(std::size_t amount) {
    if (tailroom() >= amount) {
        return true;
    }

    try {
        reserve(headroom(), amount);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

std::unique_ptr<io_buffer> io_buffer::clone() const {
    if (empty()) {
        return create(0);
    }

    if (m_next == this) {
        auto result = std::make_unique<io_buffer>(*this);
        result->unshare();

        return result;
    }

    auto head = std::make_unique<io_buffer>(*this);
    head->unshare();

    io_buffer*       tail = head.get();
    const io_buffer* curr = this;

    do {
        curr = curr->m_next;
        if (curr == this) {
            break;
        }

        auto new_node = std::make_unique<io_buffer>(*curr);
        new_node->unshare();

        tail->m_next     = new_node.get();
        new_node->m_prev = tail;
        tail             = new_node.release();

    } while (true);

    tail->m_next = head.get();
    head->m_prev = tail;

    return head;
}

void io_buffer::unshare() {
    if (!is_shared()) {
        return;
    }

    auto new_buf = copy_buffer(m_data, m_length, headroom(), tailroom());
    // m_buffer.unshare();
    *this = std::move(*new_buf);
}

bool io_buffer::is_chained() const noexcept {
    return m_next != this;
}

std::size_t io_buffer::compute_chain_length() const noexcept {
    if (m_next == this) {
        return m_length;
    }

    std::size_t      total   = m_length;
    const io_buffer* current = this;

    do {
        current = current->m_next;
        if (current == this) {
            break;
        }

        total += current->m_length;
    } while (true);

    return total;
}

void io_buffer::append_to_chain(std::unique_ptr<io_buffer>&& buf) {
    if (!buf) {
        return;
    }

    io_buffer* node = buf.release();

    if (m_next == this) {
        m_next       = node;
        m_prev       = node;
        node->m_next = this;
        node->m_prev = this;
    } else {
        node->m_next   = this;
        node->m_prev   = m_prev;
        m_prev->m_next = node;
        m_prev         = node;
    }
}

void io_buffer::insert_after(std::unique_ptr<io_buffer>&& buf) {
    if (!buf) {
        return;
    }

    io_buffer* node = buf.release();

    node->m_next   = m_next;
    node->m_prev   = this;
    m_next->m_prev = node;
    m_next         = node;
}

std::string_view io_buffer::normalize() {
    if (m_next == this) {
        return std::string_view(reinterpret_cast<const char*>(m_data), m_length);
    }

    std::size_t total_length = compute_chain_length();
    auto        new_buf      = create(total_length);
    uint8_t*    dest         = new_buf->m_buffer.data();

    const io_buffer* current = this;
    do {
        if (current->m_length > 0) {
            std::memcpy(dest, current->m_data, current->m_length);
            dest += current->m_length;
        }

        current = current->m_next;
    } while (current != this);

    new_buf->m_length = total_length;
    *this             = std::move(*new_buf);

    return std::string_view(reinterpret_cast<const char*>(m_data), m_length);
}

detail::shard_buffer::shard_buffer()
    : m_shared_info(nullptr), m_buffer(nullptr), m_free_fn(nullptr), m_user_data(nullptr) {
}

detail::shard_buffer::shard_buffer(const detail::shard_buffer& other)
    : m_shared_info(other.m_shared_info), m_buffer(other.m_buffer), m_free_fn(other.m_free_fn),
      m_user_data(other.m_user_data) {
    share_from(other);
}

detail::shard_buffer& detail::shard_buffer::operator=(const detail::shard_buffer& other) {
    m_shared_info = other.m_shared_info;
    m_buffer      = other.m_buffer;
    m_free_fn     = other.m_free_fn;
    m_user_data   = other.m_user_data;

    share_from(other);
    return *this;
}

detail::shard_buffer::shard_buffer(detail::shard_buffer&& other) noexcept {
    m_shared_info       = other.m_shared_info;
    m_buffer            = other.m_buffer;
    m_free_fn           = other.m_free_fn;
    m_user_data         = other.m_user_data;
    other.m_shared_info = nullptr;
    other.m_buffer      = nullptr;
    other.m_free_fn     = nullptr;
    other.m_user_data   = nullptr;
}

detail::shard_buffer& detail::shard_buffer::operator=(detail::shard_buffer&& other) noexcept {
    if (this != &other) {
        free();

        m_shared_info       = other.m_shared_info;
        m_buffer            = other.m_buffer;
        m_free_fn           = other.m_free_fn;
        m_user_data         = other.m_user_data;
        other.m_shared_info = nullptr;
        other.m_buffer      = nullptr;
        other.m_free_fn     = nullptr;
        other.m_user_data   = nullptr;
    }
    return *this;
}

detail::shard_buffer::~shard_buffer() {
    free();
}

detail::shard_buffer::shard_buffer(void* buf, free_function fn, void* user_data)
    : m_buffer(static_cast<uint8_t*>(buf)), m_free_fn(fn), m_user_data(user_data) {
}

void detail::shard_buffer::ensure_shared_info() {
    if (!m_shared_info) {
        m_shared_info = new shared_info();
    }
}

void detail::shard_buffer::share_from(const detail::shard_buffer& other) {
    if (!other.m_buffer) {
        return;
    }

    if (!other.m_shared_info) {
        const_cast<detail::shard_buffer&>(other).ensure_shared_info();
    }

    m_shared_info = other.m_shared_info;
    m_shared_info->ref_count.fetch_add(1, std::memory_order_acq_rel);
}

bool detail::shard_buffer::is_shared() const noexcept {
    return m_shared_info && m_shared_info->ref_count.load(std::memory_order_acquire) > 1;
}

void detail::shard_buffer::unshare() {
    if (!is_shared()) {
        return;
    }

    free();
    m_shared_info = nullptr;
    m_buffer      = nullptr;
    m_free_fn     = nullptr;
    m_user_data   = nullptr;
}

void detail::shard_buffer::free() {
    if (!m_buffer) {
        return;
    }

    if (m_shared_info) {
        if (m_shared_info->ref_count.fetch_sub(1, std::memory_order_acq_rel) > 1) {
            return;
        }

        delete m_shared_info;
        m_shared_info = nullptr;
    }

    if (m_free_fn) {
        m_free_fn(m_buffer, m_user_data);
        m_free_fn = nullptr;
    }

    m_user_data = nullptr;
    m_buffer    = nullptr;
}

} // namespace io
} // namespace mc