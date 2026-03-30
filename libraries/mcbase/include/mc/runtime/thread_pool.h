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

#ifndef MC_RUNTIME_THREAD_POOL_H
#define MC_RUNTIME_THREAD_POOL_H

#include <memory>
#include <type_traits>
#include <utility>

#include <mc/common.h>
#include <mc/runtime/detail/task.h>

namespace mc::runtime {
namespace detail {
struct thread_pool_impl;

using thread_pool_task = task;

} // namespace detail

class MC_API thread_pool {
public:
    class executor_type;

    class executor_type {
    public:
        executor_type(const executor_type&)            = default;
        executor_type& operator=(const executor_type&) = default;
        executor_type(executor_type&&)                 = default;
        executor_type& operator=(executor_type&&)      = default;

        template <typename Function, typename Allocator = std::allocator<void>>
        void dispatch(Function&& f, const Allocator& a = Allocator()) const
        {
            MC_UNUSED(a);
            dispatch_task(detail::thread_pool_task(std::forward<Function>(f)));
        }

        template <typename Function, typename Allocator = std::allocator<void>>
        void post(Function&& f, const Allocator& a = Allocator()) const
        {
            MC_UNUSED(a);
            post_task(detail::thread_pool_task(std::forward<Function>(f)));
        }

        template <typename Function, typename Allocator = std::allocator<void>>
        void defer(Function&& f, const Allocator& a = Allocator()) const
        {
            MC_UNUSED(a);
            defer_task(detail::thread_pool_task(std::forward<Function>(f)));
        }

        bool operator==(const executor_type& other) const noexcept
        {
            return m_impl == other.m_impl;
        }

        bool operator!=(const executor_type& other) const noexcept
        {
            return !(*this == other);
        }

        /**
         * @brief 检查当前线程是否在此 executor 上执行
         */
        bool running_in_this_thread() const noexcept;

    private:
        friend class thread_pool;
        friend struct detail::thread_pool_impl;

        explicit executor_type(std::shared_ptr<detail::thread_pool_impl> impl) noexcept : m_impl(std::move(impl))
        {}

        std::shared_ptr<detail::thread_pool_impl> m_impl;

        void dispatch_task(detail::thread_pool_task task) const;
        void post_task(detail::thread_pool_task task) const;
        void defer_task(detail::thread_pool_task task) const;
    };

    explicit thread_pool(std::size_t num_threads, mc::string_view name = "pool");
    ~thread_pool();

    thread_pool(const thread_pool&)            = delete;
    thread_pool& operator=(const thread_pool&) = delete;
    thread_pool(thread_pool&&)                 = delete;
    thread_pool& operator=(thread_pool&&)      = delete;

    executor_type get_executor() noexcept;

    void start();
    void stop();
    void join();
    bool stopped() const;

    std::size_t shard_count() const;

    /// @brief 将当前线程附加到线程池并运行工作循环
    /// @note 进入时自动 attach，退出时自动 detach
    /// @note 阻塞直到线程池停止
    void run();

    std::size_t     poll_one();
    mc::string_view get_name() const noexcept;

private:
    friend struct detail::thread_pool_impl;

    std::shared_ptr<detail::thread_pool_impl> m_impl;
};

} // namespace mc::runtime

#endif // MC_RUNTIME_THREAD_POOL_H
