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

// 前向声明
class state_pool;

template <typename StateType>
struct state_deleter {
    template <typename U>
    using rebind = state_deleter<U>;

    void destroy(StateType* ptr);
    void deallocate(const void* ptr);
};

template <typename StateType>
using state_ptr = mc::shared_ptr<StateType, state_deleter<StateType>, StateType*>;

// State 缓存池类
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
    auto acquire_state(Executor executor, Allocator allocator) {
        using state_type = State<T, Executor, Allocator>;

        void* ptr = try_acquire_state(sizeof(typename state_type::value_type));
        if (ptr) {
            auto* state = static_cast<state_type*>(ptr);
            state->reuse(std::move(executor), std::move(allocator));
            return state_ptr<state_type>{state};
        } else {
            ptr         = malloc(sizeof(state_type));
            auto* state = new (ptr) state_type(std::move(executor), std::move(allocator));
            return state_ptr<state_type>{state};
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

    template <typename StateType>
    friend struct state_deleter;

    state_pool();
    ~state_pool();

    void* try_acquire_state(std::size_t state_size);
    bool  try_release_to_pool(void* ptr, std::size_t state_size);
};

// 从缓存池创建 State 对象
template <typename T, typename Executor, typename Allocator>
auto make_pooled_state(Executor executor, Allocator allocator) {
    return state_pool::instance().acquire_state<T, Executor, Allocator>(std::move(executor), std::move(allocator));
}

// state_deleter 的实现，需要放在 state_pool 声明之后
template <typename StateType>
void state_deleter<StateType>::destroy(StateType* ptr) {
    ptr->reset();
}

template <typename StateType>
void state_deleter<StateType>::deallocate(const void* ptr) {
    auto& pool          = state_pool::instance();
    void* non_const_ptr = const_cast<void*>(ptr);
    if (!pool.try_release_to_pool(non_const_ptr, sizeof(typename StateType::value_type))) {
        auto* p_state = static_cast<StateType*>(non_const_ptr);
        p_state->~state_base(); // 回收失败，析构 State 对象（state_value部分在reset时已经析构了）
        free(non_const_ptr);
    }
}

} // namespace mc::futures

#endif // MC_FUTURES_STATE_POOL_H