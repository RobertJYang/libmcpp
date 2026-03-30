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
#include <mutex>
#include <utility>

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
    std::cv_status wait_for(Lock& lock, const std::chrono::duration<Rep, Period>& rel_time);
    template <typename Lock, typename Rep, typename Period, typename Predicate>
    bool wait_for(Lock& lock, const std::chrono::duration<Rep, Period>& rel_time, Predicate pred);

    // 等待直到某个时间点
    template <typename Lock, typename Clock, typename Duration>
    std::cv_status wait_until(Lock& lock, const std::chrono::time_point<Clock, Duration>& abs_time);
    template <typename Lock, typename Clock, typename Duration, typename Predicate>
    bool wait_until(Lock& lock, const std::chrono::time_point<Clock, Duration>& abs_time, Predicate pred);

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

    struct lock_adapter {
        void* context         = nullptr;
        void (*lock)(void*)   = nullptr;
        void (*unlock)(void*) = nullptr;
    };

    struct deadline_adapter {
        void* context                                     = nullptr;
        bool (*expired)(void*)                            = nullptr;
        std::chrono::steady_clock::time_point wakeup_time = std::chrono::steady_clock::time_point::min();
    };

    std::condition_variable_any                m_cv;
    mc::mutex_box<WaiterList, mc::small_mutex> m_waiter_list;

    static bool running_on_worker_thread() noexcept;

    void add_waiter(WaiterNode* node);
    void remove_waiter(WaiterNode* node);
    void report_recursion_depth_exceeded();

    class waiter_cleanup {
    public:
        waiter_cleanup(condition_variable* cv, WaiterNode* node) noexcept : m_cv(cv), m_node(node)
        {}
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
    static lock_adapter make_lock_adapter(Lock& lock)
    {
        return {
            &lock,
            [](void* context) {
            static_cast<Lock*>(context)->lock();
        },
            [](void* context) {
            static_cast<Lock*>(context)->unlock();
        },
        };
    }

    template <typename Clock, typename Duration>
    static deadline_adapter make_deadline_adapter(const std::chrono::time_point<Clock, Duration>& abs_time)
    {
        auto wakeup_time = std::chrono::steady_clock::now();
        if (auto now = Clock::now(); abs_time > now) {
            wakeup_time += std::chrono::duration_cast<std::chrono::steady_clock::duration>(abs_time - now);
        }

        return {
            const_cast<std::chrono::time_point<Clock, Duration>*>(&abs_time),
            [](void* context) {
            return Clock::now() >= *static_cast<const std::chrono::time_point<Clock, Duration>*>(context);
        },
            wakeup_time,
        };
    }

    void           wait_on_worker(lock_adapter lock);
    std::cv_status wait_until_on_worker(lock_adapter lock, deadline_adapter deadline);
};

template <typename Lock>
void condition_variable::wait(Lock& lock)
{
    if (running_on_worker_thread()) {
        wait_on_worker(make_lock_adapter(lock));
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
std::cv_status condition_variable::wait_for(Lock& lock, const std::chrono::duration<Rep, Period>& rel_time)
{
    return wait_until(lock, std::chrono::steady_clock::now() + rel_time);
}

template <typename Lock, typename Rep, typename Period, typename Predicate>
bool condition_variable::wait_for(Lock& lock, const std::chrono::duration<Rep, Period>& rel_time, Predicate pred)
{
    return wait_until(lock, std::chrono::steady_clock::now() + rel_time, std::move(pred));
}

template <typename Lock, typename Clock, typename Duration>
std::cv_status condition_variable::wait_until(Lock& lock, const std::chrono::time_point<Clock, Duration>& abs_time)
{
    if (running_on_worker_thread()) {
        return wait_until_on_worker(make_lock_adapter(lock), make_deadline_adapter(abs_time));
    } else {
        return m_cv.wait_until(lock, abs_time);
    }
}

template <typename Lock, typename Clock, typename Duration, typename Predicate>
bool condition_variable::wait_until(Lock& lock, const std::chrono::time_point<Clock, Duration>& abs_time,
                                    Predicate pred)
{
    while (!pred()) {
        if (wait_until(lock, abs_time) == std::cv_status::timeout) {
            return pred();
        }
    }
    return true;
}
} // namespace mc::runtime

#endif // MC_RUNTIME_CONDITION_VARIABLE_H
