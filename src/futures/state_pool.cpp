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

#include <mc/futures/state_pool.h>
#include <mc/intrusive/list.h>

#include <algorithm>
#include <queue>
#include <thread>
#include <unordered_map>

namespace mc::futures {

struct empty_mutex {
    void lock() {
    }

    void unlock() {
    }
};

// 固定大小的 State 缓存池
class sized_state_pool {
public:
    explicit sized_state_pool(std::size_t max_size) : m_max_size(max_size) {
    }

    ~sized_state_pool() {
        clear();
    }

    void* acquire() {
        if (m_pool.empty()) {
            return nullptr;
        }

        auto state_ptr = m_pool.front();
        m_pool.pop();
        return state_ptr;
    }

    bool release(void* state_ptr) {
        if (m_pool.size() >= m_max_size) {
            return false;
        }

        m_pool.push(state_ptr);
        return true;
    }

    bool empty() const {
        return m_pool.empty();
    }

    std::size_t size() const {
        return m_pool.size();
    }

    // 清空池中的所有对象
    void clear() {
        while (!m_pool.empty()) {
            auto* state_ptr = m_pool.front();
            m_pool.pop();

            // 放入缓存池的对象，state_value 部分已经析构了，要补充 state_base 析构
            auto* base = static_cast<state_base*>(state_ptr);
            base->~state_base();
            free(state_ptr);
        }
    }

private:
    std::queue<void*> m_pool;     // 存储 State 指针的队列
    std::size_t       m_max_size; // 最大缓存池大小
};

using state_pool_map = std::unordered_map<std::size_t, std::unique_ptr<sized_state_pool>>;

class state_pool::impl {
public:
    impl() = default;

    // 设置缓存池配置
    void                     set_config(const state_pool_config& config);
    const state_pool_config& get_config() const;

    state_pool::pool_stats get_stats() const;

    void  clear_all_pools();
    void* try_acquire_state(std::size_t state_size);
    bool  try_release_state_to_pool(void* state_ptr, std::size_t state_size);

private:
    sized_state_pool* get_global_pool(std::size_t state_size, bool need_create = true);

    void clear_all_pools_unlocked();
    void remove_global_pool_unlocked(std::size_t state_size);

    mutable std::mutex m_mutex;        // 保护全局状态的互斥锁
    state_pool_config  m_config;       // 缓存池配置
    state_pool_map     m_global_pools; // 全局缓存池
};

// 重置状态，但保留 mutex 和 cv 以避免重构造开销
void state_base::reset() {
    ready.store(false);
    deferred.store(false);
    cancelled.store(false);
    policy = launch::async;
    m_continuations.clear();
    m_cancel_callbacks.clear();
}

void state_base::reuse() {
    // 因为 reset 时重置了状态，并且保留了 mutex 和 cv，所以这里什么都不需要做
}

// 设置缓存池配置
void state_pool::impl::set_config(const state_pool_config& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
    clear_all_pools_unlocked();
}

// 获取当前缓存池配置
const state_pool_config& state_pool::impl::get_config() const {
    return m_config;
}

// 获取缓存池统计信息
state_pool::pool_stats state_pool::impl::get_stats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    state_pool::pool_stats      stats;

    // 统计全局池
    for (const auto& [state_size, pool] : m_global_pools) {
        stats.total_global_states += pool->size();
        stats.total_pools++;
    }

    return stats;
}

// 清理所有池中的缓存对象
void state_pool::impl::clear_all_pools() {
    std::lock_guard<std::mutex> lock(m_mutex);
    clear_all_pools_unlocked();
}

// 尝试获取指定大小的 State 对象
void* state_pool::impl::try_acquire_state(std::size_t size) {
    auto aligned_size = MC_ALIGN_UP(size, m_config.alignment);
    if (aligned_size > m_config.max_cacheable_size) {
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    auto* global_pool = get_global_pool(aligned_size, true);
    if (global_pool) {
        return global_pool->acquire();
    }

    return nullptr;
}

// 将 State 对象释放回池中，返回是否成功放入池中
bool state_pool::impl::try_release_state_to_pool(void* state_ptr, std::size_t size) {
    if (!state_ptr) {
        return false;
    }

    auto aligned_size = MC_ALIGN_UP(size, m_config.alignment);
    if (aligned_size > m_config.max_cacheable_size) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    auto* global_pool = get_global_pool(aligned_size, false);
    if (global_pool && global_pool->release(state_ptr)) {
        return true;
    }

    return false;
}

// 获取或创建全局池
sized_state_pool* state_pool::impl::get_global_pool(std::size_t state_size, bool need_create) {
    auto it = m_global_pools.find(state_size);
    if (it != m_global_pools.end()) {
        return &*it->second;
    }

    // 检查是否需要创建或者超过最大大小池数量
    if (!need_create || m_global_pools.size() >= m_config.max_pool_count) {
        return nullptr;
    }

    auto  pool     = std::make_unique<sized_state_pool>(m_config.max_count_per_pool);
    auto* pool_ptr = pool.get();
    m_global_pools.emplace(state_size, std::move(pool));
    return pool_ptr;
}

// 清理所有池（不加锁版本，假设调用者已持有锁）
void state_pool::impl::clear_all_pools_unlocked() {
    m_global_pools.clear();
}

void state_pool::impl::remove_global_pool_unlocked(std::size_t state_size) {
    auto it = m_global_pools.find(state_size);
    if (it != m_global_pools.end()) {
        m_global_pools.erase(it);
    }
}

// state_pool 实现

state_pool::state_pool() : m_pimpl(std::make_unique<impl>()) {
}

state_pool::~state_pool() = default;

void state_pool::set_config(const state_pool_config& config) {
    m_pimpl->set_config(config);
}

const state_pool_config& state_pool::get_config() const {
    return m_pimpl->get_config();
}

state_pool::pool_stats state_pool::get_stats() const {
    return m_pimpl->get_stats();
}

void state_pool::clear_all_pools() {
    m_pimpl->clear_all_pools();
}

void* state_pool::try_acquire_state(std::size_t state_size) {
    return m_pimpl->try_acquire_state(state_size);
}

void state_pool::try_release_to_pool(void* state_ptr, std::size_t state_size) {
    if (!m_pimpl->try_release_state_to_pool(state_ptr, state_size)) {
        auto* p = static_cast<state_base*>(state_ptr);
        p->~state_base(); // 回收失败，析构 State 对象（state_value部分在reset时已经析构了）
        free(state_ptr);
    }
}

} // namespace mc::futures