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

/**
 * @file runtime_strand.h
 * @brief 线程池感知的 strand 实现，可以指定目标线程池执行任务
 */

#ifndef MC_RUNTIME_STRAND_H
#define MC_RUNTIME_STRAND_H

#include <atomic>
#include <memory>
#include <thread>
#include <type_traits>

#include <mc/common.h>
#include <mc/memory/shared_ptr.h>
#include <mc/runtime/detail/task.h>
#include <mc/runtime/op_queue.h>
#include <mc/runtime/operation_base.h>
#include <mc/runtime/thread_pool.h>
#include <mc/sync/small_mutex.h>

namespace mc::runtime {

class MC_API runtime_strand {
    struct data_t;

    struct task_operation_base : operation_base {
        data_t*      owner;
        thread_pool* target_pool;
        task_operation_base(func_type f, destroy_type d, data_t* owner_, thread_pool* pool)
            : operation_base(f, d), owner(owner_), target_pool(pool)
        {}
    };

    struct task_operation : task_operation_base {
        static void execute_impl(operation_base* op)
        {
            auto* self = static_cast<task_operation*>(op);
            self->func_storage();
        }

        static void destroy_impl(operation_base* op)
        {
            auto* self = static_cast<task_operation*>(op);
            self->owner->recycle_operation(self);
        }

        explicit task_operation(data_t* owner) : task_operation_base(&execute_impl, &destroy_impl, owner, nullptr)
        {}

        void reset(detail::task task, thread_pool* pool)
        {
            next         = nullptr;
            target_pool  = pool;
            func_storage = std::move(task);
        }

        detail::task func_storage;
    };

    using task_queue = op_queue<task_operation_base>;

    struct MC_API data_t : mc::shared_base {
        mutable small_mutex          mutex;
        bool                         locked   = false;
        task_queue                   waiting_queue;
        task_queue                   ready_queue;
        std::atomic<std::thread::id> running_thread{};
        task_operation*              free_operations{nullptr};

        ~data_t();
        void            destory_queue(task_queue* q);
        task_operation* acquire_operation(detail::task task, thread_pool* target_pool);
        void            recycle_operation(task_operation* op) noexcept;
        void            destroy_free_operations() noexcept;
    };
    using data_ptr = mc::shared_ptr<data_t>;

public:
    runtime_strand();
    runtime_strand(const runtime_strand&)            = default;
    runtime_strand& operator=(const runtime_strand&) = default;
    runtime_strand(runtime_strand&&)                 = default;
    runtime_strand& operator=(runtime_strand&&)      = default;

    ~runtime_strand();

    template <typename Function, typename Allocator = std::allocator<void>>
    void dispatch(Function&& f, const Allocator& a = Allocator()) const
    {
        dispatch_impl(detail::task(std::forward<Function>(f)));
    }

    template <typename Function, typename Allocator = std::allocator<void>>
    void post(Function&& f, const Allocator& a = Allocator()) const
    {
        post_impl(detail::task(std::forward<Function>(f)));
    }

    template <typename Function, typename Allocator = std::allocator<void>>
    void defer(Function&& f, const Allocator& a = Allocator()) const
    {
        defer_impl(detail::task(std::forward<Function>(f)));
    }

    bool operator==(const runtime_strand& other) const noexcept;
    bool operator!=(const runtime_strand& other) const noexcept;

    bool running_in_this_thread() const noexcept;

    void on_work_started() const noexcept
    {}
    void on_work_finished() const noexcept
    {}

    runtime_strand& bound_pool(thread_pool* pool) noexcept
    {
        m_bound_pool = pool;
        return *this;
    }

    thread_pool* get_bound_pool() const noexcept
    {
        return m_bound_pool;
    }

private:
    class MC_API invoker {
    public:
        invoker(mc::shared_ptr<data_t> data);
        invoker(const invoker&) = default;
        invoker(invoker&&)      = default;

        void operator()();

    private:
        mc::shared_ptr<data_t> m_data;

        bool run_ready_handlers();
        void schedule_next(thread_pool* target_pool);
    };

    template <typename Function>
    bool enqueue(Function&& f) const
    {
        auto* op = m_data->acquire_operation(detail::task(std::forward<Function>(f)), m_bound_pool);
        return enqueue_op(op);
    }
    bool enqueue_op(task_operation_base* op) const;

    bool can_execute_in_this_shard() const noexcept;

    static thread_pool::executor_type get_default_executor();

    void dispatch_impl(detail::task task) const;
    void post_impl(detail::task task) const;
    void defer_impl(detail::task task) const;

    mc::shared_ptr<data_t> m_data;
    thread_pool*           m_bound_pool = nullptr;
};

} // namespace mc::runtime

#endif // MC_RUNTIME_STRAND_H
