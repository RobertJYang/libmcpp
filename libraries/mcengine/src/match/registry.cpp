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

#include <mc/engine/match/filter.h>

#include <list>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>

namespace mc::engine::match {

namespace {

constexpr std::size_t kDefaultCacheCapacity = 1024;

// 缓存键：(backend_type, text)。pair 自带哈希组合可省事，用结构体显式实现 hash
// 让 unordered_map 直接用 std::string 比较，避免每次 lookup 拼字符串。
struct cache_key {
    std::uint32_t backend_type;
    std::string   text;

    bool operator==(const cache_key& other) const noexcept
    {
        return backend_type == other.backend_type && text == other.text;
    }
};

struct cache_key_hash {
    std::size_t operator()(const cache_key& k) const noexcept
    {
        return std::hash<std::string>{}(k.text) ^ (std::size_t(k.backend_type) * 0x9e3779b97f4a7c15ULL);
    }
};

} // namespace

struct filter_backend_registry::impl {
    mutable std::mutex                                          backends_mutex;
    std::unordered_map<std::uint32_t, filter_backend_ptr>       backends;

    mutable std::mutex                                          cache_mutex;
    std::size_t                                                 capacity{kDefaultCacheCapacity};

    // LRU：list 维护使用顺序（front 最新），map 把 key 映射到 list 节点 + 编译产物。
    using cache_entry  = std::pair<cache_key, filter_compiled_ptr>;
    using cache_list   = std::list<cache_entry>;

    cache_list                                                  cache_order;
    std::unordered_map<cache_key, cache_list::iterator, cache_key_hash> cache_index;
};

filter_backend_registry& filter_backend_registry::instance()
{
    static filter_backend_registry s_registry;
    return s_registry;
}

filter_backend_registry::filter_backend_registry() : m_impl(std::make_unique<impl>()) {}
filter_backend_registry::~filter_backend_registry() = default;

void filter_backend_registry::register_backend(filter_backend_ptr backend)
{
    if (!backend) {
        return;
    }
    const auto type = backend->backend_type();
    if (type == no_filter) {
        return; // 0 是保留值，不允许任何 backend 占用。
    }

    std::lock_guard lock(m_impl->backends_mutex);
    m_impl->backends[type] = std::move(backend);

    // 替换 backend 时，旧编译产物已经无效，必须清空缓存防止误用。
    {
        std::lock_guard cache_lock(m_impl->cache_mutex);
        m_impl->cache_order.clear();
        m_impl->cache_index.clear();
    }
}

filter_backend_ptr filter_backend_registry::find(std::uint32_t backend_type) const
{
    std::lock_guard lock(m_impl->backends_mutex);
    auto            it = m_impl->backends.find(backend_type);
    if (it == m_impl->backends.end()) {
        return {};
    }
    return it->second;
}

filter_compiled_ptr filter_backend_registry::get_or_compile(std::uint32_t backend_type, mc::string_view text)
{
    if (backend_type == no_filter) {
        return {};
    }

    cache_key key{backend_type, std::string(text.data(), text.size())};
    {
        std::lock_guard lock(m_impl->cache_mutex);
        auto            it = m_impl->cache_index.find(key);
        if (it != m_impl->cache_index.end()) {
            // 命中：把节点搬到 list 头部表示"最近使用"。
            m_impl->cache_order.splice(m_impl->cache_order.begin(), m_impl->cache_order, it->second);
            return it->second->second;
        }
    }

    // 缓存未命中：加 backends 锁找 backend 然后编译。compile 可能耗时，放到锁外。
    filter_backend_ptr backend;
    {
        std::lock_guard lock(m_impl->backends_mutex);
        auto            it = m_impl->backends.find(backend_type);
        if (it == m_impl->backends.end()) {
            return {};
        }
        backend = it->second;
    }

    filter_compiled_ptr compiled;
    try {
        compiled = backend->compile(text);
    } catch (...) {
        return {};
    }
    if (!compiled) {
        return {};
    }

    // 写回缓存：可能其它线程并发也编译了同一份；按"最后一个写入者赢"，幂等即可。
    {
        std::lock_guard lock(m_impl->cache_mutex);
        auto            it = m_impl->cache_index.find(key);
        if (it != m_impl->cache_index.end()) {
            m_impl->cache_order.splice(m_impl->cache_order.begin(), m_impl->cache_order, it->second);
            it->second->second = compiled;
            return compiled;
        }

        m_impl->cache_order.emplace_front(std::move(key), compiled);
        m_impl->cache_index.emplace(m_impl->cache_order.front().first, m_impl->cache_order.begin());

        while (m_impl->cache_order.size() > m_impl->capacity) {
            const auto& victim = m_impl->cache_order.back();
            m_impl->cache_index.erase(victim.first);
            m_impl->cache_order.pop_back();
        }
    }
    return compiled;
}

bool filter_backend_registry::evaluate(std::uint32_t backend_type, mc::string_view text, const message& msg)
{
    if (backend_type == no_filter) {
        return true;
    }
    auto compiled = get_or_compile(backend_type, text);
    if (!compiled) {
        return false;
    }
    auto backend = find(backend_type);
    if (!backend) {
        return false;
    }
    try {
        return backend->evaluate(*compiled, msg);
    } catch (...) {
        return false;
    }
}

void filter_backend_registry::set_cache_capacity(std::size_t capacity)
{
    if (capacity == 0) {
        capacity = 1; // 永远保留至少一个槽位，避免边界条件。
    }
    std::lock_guard lock(m_impl->cache_mutex);
    m_impl->capacity = capacity;
    while (m_impl->cache_order.size() > m_impl->capacity) {
        const auto& victim = m_impl->cache_order.back();
        m_impl->cache_index.erase(victim.first);
        m_impl->cache_order.pop_back();
    }
}

std::size_t filter_backend_registry::cache_capacity() const noexcept
{
    std::lock_guard lock(m_impl->cache_mutex);
    return m_impl->capacity;
}

std::size_t filter_backend_registry::cache_size() const noexcept
{
    std::lock_guard lock(m_impl->cache_mutex);
    return m_impl->cache_order.size();
}

void filter_backend_registry::clear_cache()
{
    std::lock_guard lock(m_impl->cache_mutex);
    m_impl->cache_order.clear();
    m_impl->cache_index.clear();
}

void filter_backend_registry::reset_for_test()
{
    auto& self = instance();
    {
        std::lock_guard lock(self.m_impl->backends_mutex);
        self.m_impl->backends.clear();
    }
    {
        std::lock_guard lock(self.m_impl->cache_mutex);
        self.m_impl->cache_order.clear();
        self.m_impl->cache_index.clear();
        self.m_impl->capacity = kDefaultCacheCapacity;
    }
}

} // namespace mc::engine::match
