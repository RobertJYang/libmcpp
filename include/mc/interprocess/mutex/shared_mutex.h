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
 * @file shared_mutex.h
 * @brief 进程间读写锁，支持多线程和多进程同步
 */
#ifndef MC_INTERPROCESS_MUTEX_SHARED_MUTEX_H
#define MC_INTERPROCESS_MUTEX_SHARED_MUTEX_H

#include "mc/interprocess/mutex/ipc_shared_mutex.h"
#include <mutex>
#include <shared_mutex>
#include <system_error>
#include <thread>

namespace mc::interprocess {

namespace detail {
inline uint64_t thread_id_to_int(const std::thread::id& id) {
    return *reinterpret_cast<const uint64_t*>(&id);
}
} // namespace detail

/**
 * @brief 进程间读写锁类，支持多线程和多进程同步
 *
 * 此类提供读写锁功能，可用于多个进程间的同步。
 * 同时支持在单进程多线程环境下的使用。
 */
class shared_mutex {
public:
    /**
     * @brief 构造函数
     *
     * @param ipc_rw_mtx 共享内存中的IPC读写互斥锁引用
     */
    explicit shared_mutex(ipc_shared_mutex& ipc_rw_mtx)
        : m_ipc_mutex(ipc_rw_mtx), m_reader_count(0), m_writer_thread_id(0) {
    }

    ~shared_mutex() {
        // 获取保护锁，确保安全析构
        std::lock_guard<std::mutex> guard(m_sync_mutex);

        // 如果有读锁，释放它
        if (m_reader_count > 0) {
            m_ipc_mutex.unlock_shared();
        }

        // 如果当前线程持有写锁，释放它
        if (m_writer_thread_id == detail::thread_id_to_int(std::this_thread::get_id())) {
            m_ipc_mutex.unlock();
        }
    }

    // 写锁相关方法 (排他锁)
    bool try_lock() {
        // 检查当前线程是否已经持有写锁
        int current_thread = detail::thread_id_to_int(std::this_thread::get_id());
        if (m_writer_thread_id == current_thread) {
            return false; // 不允许递归锁定
        }

        // 尝试获取线程互斥锁
        if (!m_thread_mutex.try_lock()) {
            return false;
        }

        // 尝试获取IPC写锁
        if (!m_ipc_mutex.try_lock()) {
            m_thread_mutex.unlock();
            return false;
        }

        // 设置写线程ID
        m_writer_thread_id = current_thread;

        return true;
    }

    void lock() {
        // 检查当前线程是否已经持有写锁
        int current_thread = detail::thread_id_to_int(std::this_thread::get_id());
        if (m_writer_thread_id == current_thread) {
            throw std::system_error(std::make_error_code(std::errc::resource_deadlock_would_occur),
                                    "尝试递归获取写锁");
        }

        // 获取线程互斥锁
        m_thread_mutex.lock();

        // 尝试获取IPC写锁
        try {
            m_ipc_mutex.lock();
        } catch (...) {
            m_thread_mutex.unlock();
            throw;
        }

        // 设置写线程ID
        m_writer_thread_id = current_thread;
    }

    void unlock() {
        // 检查当前线程是否持有写锁
        int current_thread = detail::thread_id_to_int(std::this_thread::get_id());
        if (m_writer_thread_id != current_thread) {
            throw std::system_error(std::make_error_code(std::errc::operation_not_permitted),
                                    "当前线程未持有写锁");
        }

        // 释放写锁
        m_writer_thread_id = 0;
        m_ipc_mutex.unlock();
        m_thread_mutex.unlock();
    }

    // 读锁相关方法 (共享锁)
    bool try_lock_shared() {
        // 尝试获取线程读锁
        if (!m_thread_mutex.try_lock_shared()) {
            return false;
        }

        // 获取保护锁，确保读写操作互斥
        std::unique_lock<std::mutex> guard(m_sync_mutex);

        // 如果是第一个读者，需要尝试获取IPC读锁
        if (m_reader_count == 0) {
            if (!m_ipc_mutex.try_lock_shared()) {
                m_thread_mutex.unlock_shared();
                return false;
            }
        }

        // 增加读者计数
        m_reader_count++;

        return true;
    }

    void lock_shared() {
        // 获取线程读锁
        m_thread_mutex.lock_shared();

        // 获取保护锁，确保读写操作互斥
        std::unique_lock<std::mutex> guard(m_sync_mutex);

        // 如果是第一个读者，需要获取IPC读锁
        if (m_reader_count == 0) {
            try {
                m_ipc_mutex.lock_shared();
            } catch (...) {
                m_thread_mutex.unlock_shared();
                throw;
            }
        }

        // 增加读者计数
        m_reader_count++;
    }

    void unlock_shared() {
        // 获取保护锁，确保读写操作互斥
        std::unique_lock<std::mutex> guard(m_sync_mutex);

        // 检查是否有读锁
        if (m_reader_count == 0) {
            throw std::system_error(std::make_error_code(std::errc::operation_not_permitted),
                                    "没有持有读锁");
        }

        // 减少读者计数
        m_reader_count--;

        // 如果是最后一个读者，释放IPC读锁
        if (m_reader_count == 0) {
            m_ipc_mutex.unlock_shared();
        }

        // 释放线程读锁
        m_thread_mutex.unlock_shared();
    }

private:
    mutable std::mutex m_sync_mutex;   // 保护锁，确保多线程操作的原子性
    std::shared_mutex  m_thread_mutex; // 线程读写锁
    ipc_shared_mutex&  m_ipc_mutex;    // 进程间读写锁，引用共享内存中的读写互斥锁

    unsigned int m_reader_count;     // 当前持有读锁的线程数
    int          m_writer_thread_id; // 当前持有写锁的线程ID
};

} // namespace mc::interprocess

#endif // MC_INTERPROCESS_MUTEX_SHARED_MUTEX_H