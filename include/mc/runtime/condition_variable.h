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

#ifndef MC_RUNTIME_CONDITION_VARIABLE_H
#define MC_RUNTIME_CONDITION_VARIABLE_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <utility>

#include <boost/asio.hpp>

#include <mc/runtime/thread_pool.h>
#include <mc/sync/mutex_box.h>
#include <mc/sync/small_mutex.h>

namespace mc::runtime {
class MC_API condition_variable {
public:
    condition_variable()  = default;
    ~condition_variable() = default;

    condition_variable(const condition_variable&)            = delete;
    condition_variable& operator=(const condition_variable&) = delete;

    // 等待条件变量
    template <typename Lock>
    void wait(Lock& lock);
    template <typename Lock, typename Predicate>
    void wait(Lock& lock, Predicate pred);

    // 等待一段时间
    template <typename Lock, typename Rep, typename Period>
    std::cv_status wait_for(Lock&                                     lock,
                            const std::chrono::duration<Rep, Period>& rel_time);
    template <typename Lock, typename Rep, typename Period, typename Predicate>
    bool wait_for(Lock&                                     lock,
                  const std::chrono::duration<Rep, Period>& rel_time,
                  Predicate                                 pred);

    // 等待直到某个时间点
    template <typename Lock, typename Clock, typename Duration>
    std::cv_status wait_until(Lock&                                           lock,
                              const std::chrono::time_point<Clock, Duration>& abs_time);
    template <typename Lock, typename Clock, typename Duration, typename Predicate>
    bool wait_until(Lock&                                           lock,
                    const std::chrono::time_point<Clock, Duration>& abs_time,
                    Predicate                                       pred);

    // 通知一个等待者
    void notify_one() noexcept;

    // 通知所有等待者
    void notify_all() noexcept;

private:
    struct WaiterNode;
    struct WaiterList {
        WaiterNode* head = nullptr;
        WaiterNode* tail = nullptr;
    };

    struct WaiterNode {
        thread_pool::shard_t* shard;
        std::atomic<bool>     notified{false};
        WaiterNode*           next = nullptr;
        WaiterNode*           prev = nullptr;

        MC_API void link(WaiterList& list);
        MC_API void unlink(WaiterList& list);
    };

    std::condition_variable_any                m_cv;
    mc::mutex_box<WaiterList, mc::small_mutex> m_waiter_list;

    void add_waiter(WaiterNode* node);
    void remove_waiter(WaiterNode* node);
    void report_recursion_depth_exceeded();

    class waiter_cleanup {
    public:
        waiter_cleanup(condition_variable* cv, WaiterNode* node) noexcept
            : m_cv(cv), m_node(node)
        {
        }
        waiter_cleanup(const waiter_cleanup&)            = delete;
        waiter_cleanup& operator=(const waiter_cleanup&) = delete;

        ~waiter_cleanup() noexcept
        {
            remove();
        }

        void remove() noexcept
        {
            if (!m_active) {
                return;
            }
            m_active = false;
            m_cv->remove_waiter(m_node);
        }

    private:
        condition_variable* m_cv;
        WaiterNode*         m_node;
        bool                m_active{true};
    };

    template <typename Lock>
    void wait_on_worker(Lock& lock, thread_pool::shard_t* shard);

    template <typename Lock, typename Clock, typename Duration>
    std::cv_status wait_until_on_worker(
        Lock&                                           lock,
        thread_pool::shard_t*                           shard,
        const std::chrono::time_point<Clock, Duration>& abs_time);
};

template <typename Lock>
void condition_variable::wait(Lock& lock)
{
    if (auto* shard = thread_pool::get_current_shard()) {
        wait_on_worker(lock, shard);
    } else {
        m_cv.wait(lock);
    }
}

template <typename Lock, typename Predicate>
void condition_variable::wait(Lock& lock, Predicate pred)
{
    while (!pred()) {
        wait(lock);
    }
}

template <typename Lock, typename Rep, typename Period>
std::cv_status condition_variable::wait_for(
    Lock&                                     lock,
    const std::chrono::duration<Rep, Period>& rel_time)
{
    return wait_until(lock, std::chrono::steady_clock::now() + rel_time);
}

template <typename Lock, typename Rep, typename Period, typename Predicate>
bool condition_variable::wait_for(
    Lock&                                     lock,
    const std::chrono::duration<Rep, Period>& rel_time,
    Predicate                                 pred)
{
    return wait_until(lock, std::chrono::steady_clock::now() + rel_time, std::move(pred));
}

template <typename Lock, typename Clock, typename Duration>
std::cv_status condition_variable::wait_until(
    Lock&                                           lock,
    const std::chrono::time_point<Clock, Duration>& abs_time)
{
    if (auto* shard = thread_pool::get_current_shard()) {
        return wait_until_on_worker(lock, shard, abs_time);
    } else {
        return m_cv.wait_until(lock, abs_time);
    }
}

template <typename Lock, typename Clock, typename Duration, typename Predicate>
bool condition_variable::wait_until(
    Lock&                                           lock,
    const std::chrono::time_point<Clock, Duration>& abs_time,
    Predicate                                       pred)
{
    while (!pred()) {
        if (wait_until(lock, abs_time) == std::cv_status::timeout) {
            return pred();
        }
    }
    return true;
}

template <typename Lock, typename Clock, typename Duration>
std::cv_status condition_variable::wait_until_on_worker(Lock&                                           lock,
                                                        thread_pool::shard_t*                           shard,
                                                        const std::chrono::time_point<Clock, Duration>& abs_time)
{
    WaiterNode node{shard, false};
    add_waiter(&node);
    waiter_cleanup cleanup(this, &node);

    // 检查递归深度
    thread_pool::scoped_recursion_guard recursion_guard(shard);
    if (!recursion_guard.can_schedule()) {
        report_recursion_depth_exceeded();
        // 如果嵌套太深回退到传统的条件变量等待
        bool timed_out = false;
        if (m_cv.wait_until(lock, abs_time, [&] {
            return node.notified.load(std::memory_order_acquire);
        })) {
            // 条件满足（被通知）
        } else {
            // 超时
            timed_out = true;
        }

        cleanup.remove();
        return timed_out ? std::cv_status::timeout : std::cv_status::no_timeout;
    }

    lock.unlock();

    // 使用定时器来触发超时
    boost::asio::basic_waitable_timer<Clock> timer(shard->ctx->get_executor(), abs_time);
    bool                                     timed_out = false;
    timer.async_wait([&](const boost::system::error_code& ec) {
        if (!ec) {
            timed_out = true;
        }
    });

    shard->pool.poll_until(shard, [&]() {
        return node.notified.load(std::memory_order_acquire) || timed_out || Clock::now() >= abs_time;
    });

    if (!node.notified.load(std::memory_order_acquire)) {
        timed_out = true;
    }

    timer.cancel();
    lock.lock();
    cleanup.remove();
    return timed_out ? std::cv_status::timeout : std::cv_status::no_timeout;
}

template <typename Lock>
void condition_variable::wait_on_worker(Lock& lock, thread_pool::shard_t* shard)
{
    WaiterNode node{shard, false};
    add_waiter(&node);
    waiter_cleanup cleanup(this, &node);

    // 检查递归深度
    thread_pool::scoped_recursion_guard recursion_guard(shard);
    if (!recursion_guard.can_schedule()) {
        report_recursion_depth_exceeded();
        // 如果嵌套太深，我们回退到传统的条件变量等待
        // 注意：notify_one 会同时设置 node.notified 并调用 m_cv.notify_one()
        // 所以我们可以安全地等待 m_cv
        m_cv.wait(lock, [&] {
            return node.notified.load(std::memory_order_acquire);
        });

        cleanup.remove();
        return;
    }

    lock.unlock();

    shard->pool.poll_until(shard, [&node]() {
        return node.notified.load(std::memory_order_acquire);
    });

    lock.lock();
    cleanup.remove();
}
} // namespace mc::runtime

#endif // MC_RUNTIME_CONDITION_VARIABLE_H
