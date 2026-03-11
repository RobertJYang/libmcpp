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

#include <boost/asio.hpp>

#include <mc/log.h>
#include <mc/runtime.h>
#include <mc/runtime/runtime_context.h>
#include <mc/runtime/runtime_strand.h>

namespace mc::runtime {

// ============================================================================
// data_t 实现
// ============================================================================

runtime_strand::data_t::~data_t()
{
    destory_queue(&waiting_queue);
    destory_queue(&ready_queue);
}

void runtime_strand::data_t::destory_queue(task_queue* q)
{
    while (!q->empty()) {
        task_operation_base* op = q->front();
        q->pop_front();
        op->destroy();
    }
}

// ============================================================================
// invoker 实现
// ============================================================================

runtime_strand::invoker::invoker(mc::shared_ptr<data_t> data)
    : m_data(data)
{
}

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
            if (m_data->shutdown || m_data->ready_queue.empty()) {
                break;
            }

            op = m_data->ready_queue.front();

            // 检查是否需要切换线程池
            auto*        current_shard = thread_pool::get_current_shard();
            thread_pool* current_pool  = current_shard ? &current_shard->pool : nullptr;
            thread_pool* target_pool   = op->target_pool;

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

runtime_strand::runtime_strand()
    : m_data(mc::make_shared<data_t>())
{
    ;
}

runtime_strand::~runtime_strand()
{
    if (!m_data) {
        return;
    }

    std::lock_guard lock(m_data->mutex);
    m_data->shutdown = true;
}

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

execution_context& runtime_strand::context() const
{
    if (m_bound_pool) {
        return *m_bound_pool;
    }
    return get_runtime_context().io();
}

thread_pool::executor_type runtime_strand::get_default_executor()
{
    return get_io_executor();
}

bool runtime_strand::enqueue_op(task_operation_base* op) const
{
    std::lock_guard lock(m_data->mutex);

    if (m_data->shutdown) {
        op->destroy();
        return false;
    }

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
