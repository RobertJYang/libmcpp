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

#include <cstdint>
#include <functional>
#include <memory>
#include <new>

#include <mc/common.h>
#include <mc/futures/state.h>

namespace mc::futures {

using state_base_ptr = mc::shared_ptr<state_base>;

namespace detail {
MC_API void recover_state_slot_to_pool(void* ptr);
}

class MC_API state_pool {
public:
    static state_pool& instance();
    ~state_pool();

    // 获取或创建一个 State 对象
    template <typename T, typename Executor>
    auto acquire_state(Executor executor)
    {
        using state_type = State<T>;

        auto result = try_acquire_state(sizeof(typename state_type::result_type));
        MC_ASSERT_THROW(result.ptr != nullptr, mc::runtime_exception, "无法从统一 state_base 池分配控制块");
        try {
            if (result.reused) {
                auto* state = static_cast<state_base*>(result.ptr);
                state->destroy_cached_state_object();
            }

            auto* state = new (result.ptr) state_type(std::move(executor));
            return state_base_ptr{static_cast<state_base*>(state)};
        } catch (const std::bad_alloc&) {
            detail::recover_state_slot_to_pool(result.ptr);
            MC_THROW(mc::bad_alloc_exception, "创建 Future 状态对象时内存分配失败");
        }
    }

    struct acquire_state_result {
        void* ptr{nullptr};
        bool  reused{false};
    };

    // 池统计信息结构
    struct pool_stats {
        std::size_t total_global_states = 0; // 统一控制块池中缓存的 state_base 数量
        std::size_t total_pools         = 0; // 当前活跃的控制块池数量
    };

    pool_stats get_stats() const;

    void clear_all_pools();
    bool try_release_to_pool(state_base* ptr);

private:
    class impl;
    std::unique_ptr<impl> m_pimpl;

    state_pool();

    acquire_state_result try_acquire_state(std::size_t state_size);
};

// 从缓存池创建 State 对象
template <typename T, typename Executor>
auto make_pooled_state(Executor executor)
{
    return state_pool::instance().acquire_state<detail::state_tt<T>, Executor>(std::move(executor));
}

} // namespace mc::futures

namespace mc::memory {

extern template class MC_API mc::memory::shared_ptr<mc::futures::state_base>;

} // namespace mc::memory

#endif // MC_FUTURES_STATE_POOL_H
