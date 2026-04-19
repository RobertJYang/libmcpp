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

#include <mc/shm/region.h>

#include <algorithm>
#include <cstring>

#include <mc/shm/ipc_mutex.h>

#include "detail/region_registry.h"

namespace mc::shm {
namespace {

constexpr std::size_t align_up(std::size_t value, std::size_t alignment) noexcept
{
    return (value + alignment - 1) & ~(alignment - 1);
}

struct root_entry {
    std::atomic<std::uint32_t> in_use{0};
    std::atomic<std::uint64_t> generation{0};
    std::uint32_t              kind{0};
    std::uint32_t              reserved0{0};
    std::uint64_t              offset{0};
    std::uint64_t              size{0};
    char                       name[shm_region::max_root_name_size]{};
};

struct region_header {
    std::uint32_t magic{0};
    std::uint32_t version{0};
    ipc_mutex     root_lock{};
    std::uint64_t total_size{0};
    std::uint64_t root_capacity{0};
    std::uint64_t root_table_offset{0};
    std::uint64_t user_offset{0};
    std::uint64_t user_size{0};
};

mc::string read_name(const char* value, std::size_t capacity)
{
    return mc::string(mc::string_view(value, ::strnlen(value, capacity)));
}

void write_name(char* target, std::size_t capacity, mc::string_view value)
{
    std::memset(target, 0, capacity);
    if (!value.empty()) {
        const auto count = std::min(capacity - 1, value.size());
        std::memcpy(target, value.data(), count);
    }
}

} // namespace

struct shm_region::impl {
    shm_region_options  options;
    shared_mapping_view mapping;
    region_header*      header            = nullptr;
    root_entry*         roots             = nullptr;
    void*               registered_base   = nullptr; // 已注册到 process registry 的 user_base，用于析构注销

    ~impl()
    {
        if (registered_base != nullptr) {
            detail::unregister_region(registered_base);
            registered_base = nullptr;
        }
    }
};

shm_region::shm_region() noexcept = default;

shm_region::shm_region(const shm_region_options& options) : m_impl(std::make_shared<impl>())
{
    m_impl->options = options;
    m_impl->mapping = shared_mapping_view(options.segment_name, options.total_size, options.open_mode);
    if (!m_impl->mapping.is_valid()) {
        m_impl.reset();
        return;
    }

    m_impl->header = static_cast<region_header*>(m_impl->mapping.data());
    if (m_impl->mapping.created()) {
        const auto root_capacity = std::max<std::size_t>(1, options.root_capacity);
        const auto root_table_offset = align_up(sizeof(region_header), alignof(root_entry));
        const auto user_offset = align_up(root_table_offset + sizeof(root_entry) * root_capacity, alignof(std::max_align_t));
        const auto user_size = options.total_size > user_offset ? options.total_size - user_offset : 0;
        if (user_size <= sizeof(std::max_align_t)) {
            m_impl.reset();
            return;
        }

        new (m_impl->header) region_header{};
        m_impl->header->magic = region_magic;
        m_impl->header->version = region_version;
        m_impl->header->total_size = options.total_size;
        m_impl->header->root_capacity = root_capacity;
        m_impl->header->root_table_offset = root_table_offset;
        m_impl->header->user_offset = user_offset;
        m_impl->header->user_size = user_size;

        m_impl->roots = reinterpret_cast<root_entry*>(static_cast<std::byte*>(m_impl->mapping.data()) + root_table_offset);
        for (std::size_t i = 0; i < root_capacity; ++i) {
            new (&m_impl->roots[i]) root_entry{};
        }

        if (!shm_allocator::initialize(static_cast<std::byte*>(m_impl->mapping.data()) + user_offset, user_size)) {
            m_impl.reset();
            return;
        }
    } else {
        if (m_impl->header->magic != region_magic || m_impl->header->version != region_version) {
            m_impl.reset();
            return;
        }
    }

    m_impl->roots = reinterpret_cast<root_entry*>(static_cast<std::byte*>(m_impl->mapping.data()) +
                                                  m_impl->header->root_table_offset);

    // 注册到进程级 registry：使 mc::shm::string 等 owning 对象能通过自身地址反查 allocator
    void* const       user_base = static_cast<std::byte*>(m_impl->mapping.data())
                                  + m_impl->header->user_offset;
    const std::size_t user_size = static_cast<std::size_t>(m_impl->header->user_size);
    detail::register_region(user_base, user_size);
    m_impl->registered_base = user_base;
}

shm_region::~shm_region() = default;

shm_region::shm_region(shm_region&& other) noexcept : m_impl(std::move(other.m_impl))
{}

shm_region& shm_region::operator=(shm_region&& other) noexcept
{
    if (this != &other) {
        m_impl = std::move(other.m_impl);
    }
    return *this;
}

bool shm_region::is_valid() const noexcept
{
    return m_impl != nullptr;
}

bool shm_region::created() const noexcept
{
    return is_valid() && m_impl->mapping.created();
}

const mc::string& shm_region::name() const noexcept
{
    static const mc::string empty;
    return is_valid() ? m_impl->mapping.name() : empty;
}

void* shm_region::base() const noexcept
{
    return is_valid() ? m_impl->mapping.data() : nullptr;
}

std::size_t shm_region::size() const noexcept
{
    return is_valid() ? m_impl->mapping.size() : 0;
}

void* shm_region::user_base() const noexcept
{
    return is_valid() ? static_cast<std::byte*>(m_impl->mapping.data()) + m_impl->header->user_offset : nullptr;
}

std::size_t shm_region::user_size() const noexcept
{
    return is_valid() ? static_cast<std::size_t>(m_impl->header->user_size) : 0;
}

shm_allocator shm_region::user_arena() const noexcept
{
    return shm_allocator(user_base(), user_size());
}

bool shm_region::contains(const void* ptr) const noexcept
{
    if (!is_valid() || ptr == nullptr) {
        return false;
    }

    const auto* begin = static_cast<const std::byte*>(m_impl->mapping.data());
    const auto* end = begin + m_impl->mapping.size();
    const auto* current = static_cast<const std::byte*>(ptr);
    return current >= begin && current < end;
}

std::uint64_t shm_region::offset_of(const void* ptr) const noexcept
{
    if (!contains(ptr)) {
        return 0;
    }

    const auto* begin = static_cast<const std::byte*>(m_impl->mapping.data());
    const auto* current = static_cast<const std::byte*>(ptr);
    return static_cast<std::uint64_t>(current - begin);
}

void* shm_region::address_from_offset(std::uint64_t offset) const noexcept
{
    if (!is_valid() || offset >= m_impl->mapping.size()) {
        return nullptr;
    }

    return static_cast<std::byte*>(m_impl->mapping.data()) + offset;
}

bool shm_region::upsert_root(mc::string_view name, std::uint64_t offset, std::uint64_t size, root_kind kind,
                             std::uint64_t generation) noexcept
{
    if (!is_valid() || name.empty() || name.size() >= max_root_name_size) {
        return false;
    }

    ipc_mutex_guard guard(m_impl->header->root_lock);

    root_entry* free_entry = nullptr;
    for (std::size_t i = 0; i < m_impl->header->root_capacity; ++i) {
        auto& entry = m_impl->roots[i];
        if (entry.in_use.load(std::memory_order_acquire) != 0) {
            if (read_name(entry.name, sizeof(entry.name)) == name) {
                entry.offset = offset;
                entry.size = size;
                entry.kind = static_cast<std::uint32_t>(kind);
                entry.generation.store(generation, std::memory_order_release);
                return true;
            }
            continue;
        }

        if (free_entry == nullptr) {
            free_entry = &entry;
        }
    }

    if (free_entry == nullptr) {
        return false;
    }

    write_name(free_entry->name, sizeof(free_entry->name), name);
    free_entry->offset = offset;
    free_entry->size = size;
    free_entry->kind = static_cast<std::uint32_t>(kind);
    free_entry->generation.store(generation, std::memory_order_release);
    free_entry->in_use.store(1, std::memory_order_release);
    return true;
}

std::optional<root_record> shm_region::find_root(mc::string_view name) const noexcept
{
    if (!is_valid() || name.empty()) {
        return std::nullopt;
    }

    ipc_mutex_guard guard(m_impl->header->root_lock);
    for (std::size_t i = 0; i < m_impl->header->root_capacity; ++i) {
        const auto& entry = m_impl->roots[i];
        if (entry.in_use.load(std::memory_order_acquire) == 0) {
            continue;
        }

        if (read_name(entry.name, sizeof(entry.name)) == name) {
            return root_record{
                read_name(entry.name, sizeof(entry.name)),
                entry.offset,
                entry.size,
                static_cast<root_kind>(entry.kind),
                entry.generation.load(std::memory_order_acquire),
            };
        }
    }

    return std::nullopt;
}

bool shm_region::erase_root(mc::string_view name) noexcept
{
    if (!is_valid() || name.empty()) {
        return false;
    }

    ipc_mutex_guard guard(m_impl->header->root_lock);
    for (std::size_t i = 0; i < m_impl->header->root_capacity; ++i) {
        auto& entry = m_impl->roots[i];
        if (entry.in_use.load(std::memory_order_acquire) == 0) {
            continue;
        }

        if (read_name(entry.name, sizeof(entry.name)) == name) {
            entry.in_use.store(0, std::memory_order_release);
            entry.generation.store(0, std::memory_order_release);
            entry.offset = 0;
            entry.size = 0;
            entry.kind = static_cast<std::uint32_t>(root_kind::opaque);
            std::memset(entry.name, 0, sizeof(entry.name));
            return true;
        }
    }

    return false;
}

std::vector<root_record> shm_region::list_roots() const
{
    std::vector<root_record> result;
    if (!is_valid()) {
        return result;
    }

    ipc_mutex_guard guard(m_impl->header->root_lock);
    for (std::size_t i = 0; i < m_impl->header->root_capacity; ++i) {
        const auto& entry = m_impl->roots[i];
        if (entry.in_use.load(std::memory_order_acquire) == 0) {
            continue;
        }

        result.push_back(root_record{
            read_name(entry.name, sizeof(entry.name)),
            entry.offset,
            entry.size,
            static_cast<root_kind>(entry.kind),
            entry.generation.load(std::memory_order_acquire),
        });
    }

    return result;
}

} // namespace mc::shm
