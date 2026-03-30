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

#include <mc/runtime/condition_variable.h>
#include <mc/runtime/thread_pool.h>

#include "runtime/include/thread_pool_impl.h"

namespace mc::runtime {

struct condition_variable::WaiterNode {
    thread_pool*               pool;
    detail::thread_pool_shard* shard;
    std::atomic<bool>          notified{false};
    WaiterNode*                next = nullptr;
    WaiterNode*                prev = nullptr;

    void link(WaiterList& list)
    {
        if (!list.head) {
            list.head = this;
            list.tail = this;
        } else {
            list.tail->next = this;
            this->prev      = list.tail;
            list.tail       = this;
        }
    }

    void unlink(WaiterList& list)
    {
        if (this->prev) {
            this->prev->next = this->next;
        } else if (list.head == this) {
            list.head = this->next;
        }

        if (this->next) {
            this->next->prev = this->prev;
        } else if (list.tail == this) {
            list.tail = this->prev;
        }

        this->next = nullptr;
        this->prev = nullptr;
    }
};

bool condition_variable::running_on_worker_thread() noexcept
{
    return detail::thread_pool_impl::current_shard() != nullptr;
}

void condition_variable::add_waiter(WaiterNode* node)
{
    auto lock = m_waiter_list.lock();
    node->link(*lock);
}

void condition_variable::remove_waiter(WaiterNode* node)
{
    auto lock = m_waiter_list.lock();
    node->unlink(*lock);
}

void condition_variable::notify_one() noexcept
{
    auto lock = m_waiter_list.lock();
    if (lock->head) {
        auto* node = lock->head;
        node->unlink(*lock);

        node->notified.store(true, std::memory_order_release);
        detail::thread_pool_impl::notify_shard(node->shard);
        m_cv.notify_one();
    }
}

void condition_variable::notify_all() noexcept
{
    auto  lock = m_waiter_list.lock();
    auto* node = lock->head;
    lock->head = nullptr;
    lock->tail = nullptr;
    while (node) {
        auto* next = node->next;
        node->next = nullptr;
        node->prev = nullptr;
        node->notified.store(true, std::memory_order_release);
        detail::thread_pool_impl::notify_shard(node->shard);
        node = next;
    }
    m_cv.notify_all();
}

void condition_variable::report_recursion_depth_exceeded()
{}

void condition_variable::wait_on_worker(lock_adapter lock)
{
    auto* shard = detail::thread_pool_impl::current_shard();
    auto* pool  = detail::thread_pool_impl::current_pool();
    if (!shard || !pool) {
        return;
    }

    struct adapter_lock {
        explicit adapter_lock(lock_adapter adapter_) : adapter(adapter_)
        {}

        void lock()
        {
            adapter.lock(adapter.context);
        }

        void unlock()
        {
            adapter.unlock(adapter.context);
        }

        lock_adapter adapter;
    } adapted(lock);

    WaiterNode     node{pool, shard, false};
    waiter_cleanup cleanup(this, &node);
    add_waiter(&node);

    detail::thread_pool_scoped_recursion_guard recursion_guard(shard);
    if (!recursion_guard.can_schedule()) {
        report_recursion_depth_exceeded();
        m_cv.wait(adapted, [&] {
            return node.notified.load(std::memory_order_acquire);
        });
        cleanup.remove();
        return;
    }

    lock.unlock(lock.context);
    detail::thread_pool_impl::poll_until(*pool, shard, [&node]() {
        return node.notified.load(std::memory_order_acquire);
    });
    lock.lock(lock.context);
    cleanup.remove();
}

std::cv_status condition_variable::wait_until_on_worker(lock_adapter lock, deadline_adapter deadline)
{
    auto* shard = detail::thread_pool_impl::current_shard();
    auto* pool  = detail::thread_pool_impl::current_pool();
    if (!shard || !pool) {
        return std::cv_status::no_timeout;
    }

    struct adapter_lock {
        explicit adapter_lock(lock_adapter adapter_) : adapter(adapter_)
        {}

        void lock()
        {
            adapter.lock(adapter.context);
        }

        void unlock()
        {
            adapter.unlock(adapter.context);
        }

        lock_adapter adapter;
    } adapted(lock);

    WaiterNode     node{pool, shard, false};
    waiter_cleanup cleanup(this, &node);
    add_waiter(&node);

    detail::thread_pool_scoped_recursion_guard recursion_guard(shard);
    if (!recursion_guard.can_schedule()) {
        report_recursion_depth_exceeded();
        bool timed_out = false;
        if (!m_cv.wait_until(adapted, deadline.wakeup_time, [&] {
            return node.notified.load(std::memory_order_acquire);
        })) {
            timed_out = true;
        }

        cleanup.remove();
        return timed_out ? std::cv_status::timeout : std::cv_status::no_timeout;
    }

    lock.unlock(lock.context);
    auto wakeup = detail::thread_pool_impl::schedule_shard_wakeup(shard, deadline.wakeup_time);
    detail::thread_pool_impl::poll_until(*pool, shard, [&]() {
        return node.notified.load(std::memory_order_acquire) || deadline.expired(deadline.context);
    });

    bool notified = node.notified.load(std::memory_order_acquire);
    detail::thread_pool_impl::cancel_shard_wakeup(wakeup);
    lock.lock(lock.context);
    cleanup.remove();
    return notified ? std::cv_status::no_timeout : std::cv_status::timeout;
}

} // namespace mc::runtime
