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
 * @file mutex.h
 * @brief 进程间互斥锁，支持多线程和多进程同步
 */
#ifndef MC_INTERPROCESS_MUTEX_MUTEX_H
#define MC_INTERPROCESS_MUTEX_MUTEX_H

#include <chrono>
#include <mutex>

#include <mc/interprocess/mutex/ipc_mutex.h>

namespace mc::interprocess {

/**
 * 进程间互斥锁类，支持多线程和多进程同步
 *
 * 此类提供基本的互斥锁功能，可用于多个进程间的同步。
 * 同时支持在单进程多线程环境下的使用。
 */
class mutex {
public:
    /**
     * @brief 构造函数
     *
     * @param ipc_mtx 共享内存中的IPC互斥锁引用
     */
    explicit mutex(ipc_mutex& ipc_mtx) : m_ipc_mutex(ipc_mtx)
    {}

    ~mutex() = default;

    bool try_lock()
    {
        if (!m_thread_mutex.try_lock()) {
            return false;
        }

        if (!m_ipc_mutex.try_lock()) {
            m_thread_mutex.unlock();
            return false;
        }

        return true;
    }

    template <class Rep, class Period>
    bool try_lock_for(const std::chrono::duration<Rep, Period>& rel_time)
    {
        // 记录函数进入时间
        auto start_time = std::chrono::steady_clock::now();

        // 先尝试获取线程锁
        if (!m_thread_mutex.try_lock_for(rel_time)) {
            return false;
        }

        // 计算获取线程锁消耗的时间
        auto thread_lock_time    = std::chrono::steady_clock::now();
        auto thread_lock_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(thread_lock_time - start_time);

        // 计算剩余时间用于获取ipc锁
        auto remaining_time = rel_time - thread_lock_elapsed;
        if (remaining_time.count() <= 0) {
            // 没有剩余时间了，释放线程锁并返回失败
            m_thread_mutex.unlock();
            return false;
        }

        // 尝试在剩余时间内获取ipc锁
        auto timeout_ms = std::chrono::duration_cast<std::chrono::milliseconds>(remaining_time);
        if (!m_ipc_mutex.try_lock_for(timeout_ms)) {
            // 获取ipc锁失败，释放线程锁
            m_thread_mutex.unlock();
            return false;
        }

        // 成功获取两个锁
        return true;
    }

    void lock()
    {
        // 先获取线程锁，再获取进程锁
        m_thread_mutex.lock();
        try {
            m_ipc_mutex.lock();
        } catch (...) {
            m_thread_mutex.unlock();
            throw;
        }
    }

    void unlock()
    {
        // 先释放进程锁，再释放线程锁
        m_ipc_mutex.unlock();
        m_thread_mutex.unlock();
    }

private:
    std::timed_mutex m_thread_mutex; // 线程同步，使用timed_mutex支持超时
    ipc_mutex&       m_ipc_mutex;    // 进程间同步，引用共享内存中的互斥锁
};

} // namespace mc::interprocess

#endif // MC_INTERPROCESS_MUTEX_MUTEX_H