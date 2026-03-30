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

#include <mc/log/log.h>
#include <mc/runtime.h>
#include <mc/runtime/runtime_context.h>
#include <mc/runtime/runtime_strand.h>

#include "runtime/include/thread_pool_impl.h"

namespace mc::runtime {

// ============================================================================
// data_t 实现
// ============================================================================

runtime_strand::data_t::~data_t()
{
    destory_queue(&waiting_queue);
    destory_queue(&ready_queue);
    destroy_free_operations();
}

void runtime_strand::data_t::destory_queue(task_queue* q)
{
    while (!q->empty()) {
        task_operation_base* op = q->front();
        q->pop_front();
        op->destroy();
    }
}

runtime_strand::task_operation* runtime_strand::data_t::acquire_operation(detail::task task, thread_pool* target_pool)
{
    task_operation* operation = nullptr;
    {
        std::lock_guard lock(mutex);
        operation = free_operations;
        if (operation) {
            free_operations = static_cast<task_operation*>(free_operations->next);
        }
    }

    if (!operation) {
        operation = new task_operation(this);
    } else {
        operation->next = nullptr;
    }

    operation->reset(std::move(task), target_pool);
    return operation;
}

void runtime_strand::data_t::recycle_operation(task_operation* op) noexcept
{
    op->func_storage.reset();
    op->target_pool = nullptr;
    std::lock_guard lock(mutex);
    op->next        = free_operations;
    free_operations = op;
}

void runtime_strand::data_t::destroy_free_operations() noexcept
{
    while (free_operations) {
        task_operation* operation = free_operations;
        free_operations           = static_cast<task_operation*>(free_operations->next);
        delete operation;
    }
}

// ============================================================================
// invoker 实现
// ============================================================================

runtime_strand::invoker::invoker(mc::shared_ptr<data_t> data) : m_data(data)
{}

void runtime_strand::invoker::operator()()
{
    while (true) {
        if (run_ready_handlers()) {
            return; // 已经调度到新线程池直接返回
        }

        std::lock_guard lock(m_data->mutex);
        if (!m_data->waiting_queue.empty()) {
            m_data->ready_queue.splice_back(m_data->waiting_queue);
        } else {
            m_data->locked = false;
            break;
        }
    }
}

bool runtime_strand::invoker::run_ready_handlers()
{
    // 标记当前线程正在执行
    m_data->running_thread.store(std::this_thread::get_id(), std::memory_order_release);

    while (true) {
        task_operation_base* op = nullptr;

        {
            std::lock_guard lock(m_data->mutex);
            if (m_data->ready_queue.empty()) {
                break;
            }

            op = m_data->ready_queue.front();

            // 检查是否需要切换线程池
            thread_pool* current_pool = detail::thread_pool_impl::current_pool();
            thread_pool* target_pool  = op->target_pool;

            if (target_pool && current_pool != target_pool) {
                // 需要切换线程池，转移执行权，不 pop 任务，在新线程池上继续执行
                m_data->running_thread.store(std::thread::id{}, std::memory_order_release);
                schedule_next(target_pool);
                return true;
            }

            // 在正确的线程池上，取出任务执行
            m_data->ready_queue.pop_front();
        }

        if (op) {
            try {
                op->execute();
            } catch (std::exception& e) {
                elog("Exception in runtime_strand: {}", e.what());
            } catch (...) {
                elog("Unknown exception in runtime_strand");
            }
            op->destroy();
        }
    }

    // 清除运行标记
    m_data->running_thread.store(std::thread::id{}, std::memory_order_release);
    return false;
}

void runtime_strand::invoker::schedule_next(thread_pool* target_pool)
{
    if (target_pool) {
        target_pool->get_executor().post(invoker(m_data), std::allocator<void>{});
    }
}

// ============================================================================
// runtime_strand 实现
// ============================================================================

runtime_strand::runtime_strand() : m_data(mc::make_shared<data_t>())
{
    ;
}

runtime_strand::~runtime_strand() = default;

bool runtime_strand::running_in_this_thread() const noexcept
{
    if (!m_data) {
        return false;
    }
    return m_data->running_thread.load(std::memory_order_acquire) == std::this_thread::get_id();
}

bool runtime_strand::operator==(const runtime_strand& other) const noexcept
{
    return m_data == other.m_data;
}

bool runtime_strand::operator!=(const runtime_strand& other) const noexcept
{
    return !(*this == other);
}

bool runtime_strand::can_execute_in_this_shard() const noexcept
{
    return detail::thread_pool_impl::current_pool() == m_bound_pool;
}

thread_pool::executor_type runtime_strand::get_default_executor()
{
    return get_io_context().get_executor();
}

void runtime_strand::dispatch_impl(detail::task task) const
{
    if (running_in_this_thread()) {
        if (!m_bound_pool || can_execute_in_this_shard()) {
            task();
            return;
        }
    }

    auto* op    = m_data->acquire_operation(std::move(task), m_bound_pool);
    bool  first = enqueue_op(op);
    if (first) {
        if (m_bound_pool) {
            m_bound_pool->get_executor().post(invoker(m_data), std::allocator<void>{});
        } else {
            get_default_executor().post(invoker(m_data), std::allocator<void>{});
        }
    }
}

void runtime_strand::post_impl(detail::task task) const
{
    auto* op    = m_data->acquire_operation(std::move(task), m_bound_pool);
    bool  first = enqueue_op(op);
    if (first) {
        if (m_bound_pool) {
            m_bound_pool->get_executor().post(invoker(m_data), std::allocator<void>{});
        } else {
            get_default_executor().post(invoker(m_data), std::allocator<void>{});
        }
    }
}

void runtime_strand::defer_impl(detail::task task) const
{
    auto* op    = m_data->acquire_operation(std::move(task), m_bound_pool);
    bool  first = enqueue_op(op);
    if (first) {
        if (m_bound_pool) {
            m_bound_pool->get_executor().defer(invoker(m_data), std::allocator<void>{});
        } else {
            get_default_executor().defer(invoker(m_data), std::allocator<void>{});
        }
    }
}

bool runtime_strand::enqueue_op(task_operation_base* op) const
{
    std::lock_guard lock(m_data->mutex);

    bool was_unlocked = !m_data->locked;
    if (was_unlocked) {
        m_data->locked = true;
        m_data->ready_queue.push_back(*op);
    } else {
        m_data->waiting_queue.push_back(*op);
    }
    return was_unlocked;
}

} // namespace mc::runtime
