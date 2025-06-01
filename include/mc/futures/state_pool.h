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

#ifndef MC_FUTURES_STATE_POOL_H
#define MC_FUTURES_STATE_POOL_H

#include <functional>
#include <memory>
#include <mutex>

#include <mc/futures/state.h>

namespace mc::futures {

// State 缓存池配置结构
struct state_pool_config {
    std::size_t max_pool_count     = 32;   // 最大缓存池数量（相同大小的 State 一个缓存池）
    std::size_t max_count_per_pool = 100;  // 每个缓存池最大缓存的 State 数量
    std::size_t max_cacheable_size = 1024; // 最大可缓存的 State 大小，超过此大小不会被缓存（实际是比较 Stete::state_value 对齐后的大小）
    std::size_t alignment          = 8;    // 缓存池对齐大小
};

// State 缓存池类 - 复用 Future/Promise 的 State 对象，减少内存分配开销
class state_pool {
public:
    static state_pool& instance() {
        static state_pool pool;
        return pool;
    }

    void                     set_config(const state_pool_config& config);
    const state_pool_config& get_config() const;

    // 获取或创建一个 State 对象
    template <typename T, typename Executor, typename Allocator>
    std::shared_ptr<State<T, Executor, Allocator>> acquire_state(Executor executor, Allocator allocator) {
        using state_type = State<T, Executor, Allocator>;

        auto deleter = [this](state_type* ptr) {
            ptr->reset(); // 重置 State 对象
            this->try_release_to_pool(ptr, sizeof(typename state_type::value_type));
        };

        void* state_ptr = try_acquire_state(sizeof(typename state_type::value_type));
        if (state_ptr) {
            auto* state = static_cast<state_type*>(state_ptr);
            state->reuse(std::move(executor), std::move(allocator));
            return std::shared_ptr<state_type>(state, deleter);
        } else {
            state_ptr   = malloc(sizeof(state_type));
            auto* state = new (state_ptr) state_type(std::move(executor), std::move(allocator));
            return std::shared_ptr<state_type>(state, deleter);
        }
    }

    // 池统计信息结构
    struct pool_stats {
        std::size_t total_global_states = 0; // 全局池中缓存的 State 数量
        std::size_t total_pools         = 0; // State 池的总数量
    };

    pool_stats get_stats() const;

    void clear_all_pools();

private:
    class impl;
    std::unique_ptr<impl> m_pimpl;

    state_pool();
    ~state_pool();

    void* try_acquire_state(std::size_t state_size);
    void  try_release_to_pool(void* state_ptr, std::size_t state_size);
};

// 便利函数：创建使用池的 State 对象
template <typename T, typename Executor, typename Allocator>
std::shared_ptr<State<T, Executor, Allocator>> make_pooled_state(Executor executor, Allocator allocator) {
    return state_pool::instance().acquire_state<T, Executor, Allocator>(std::move(executor), std::move(allocator));
}

} // namespace mc::futures

#endif // MC_FUTURES_STATE_POOL_H