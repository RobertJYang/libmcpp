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
 * @brief 跨进程互斥锁，支持进程崩溃时自动解锁
 */
#ifndef MC_INTERPROCESS_SHARED_MUTEX_H
#define MC_INTERPROCESS_SHARED_MUTEX_H

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <unistd.h>
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <sys/time.h>
#include <system_error>
#include <unordered_set>

#include "mc/interprocess/shared_memory.h"

namespace mc::interprocess {

// 公共常量和工具函数部分
// 读写锁的最大读者数量，可通过编译参数覆盖
#ifndef MC_SHARED_MUTEX_MAX_READERS
#define MC_SHARED_MUTEX_MAX_READERS 5
#endif

// 锁超时时间（微秒），可通过编译参数覆盖
#ifndef MC_SHARED_MUTEX_TIMEOUT_US
#define MC_SHARED_MUTEX_TIMEOUT_US 10000000  // 默认10秒
#endif

constexpr size_t INVALID_ID = static_cast<size_t>(-1);

/**
 * 获取当前时间（微秒）
 * @return 当前时间（微秒）
 */
inline uint64_t get_current_time_us() {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return static_cast<uint64_t>(tv.tv_sec) * 1000000 + tv.tv_usec;
}

/**
 * 基于策略等待的通用函数
 * 
 * @param max_spin_attempts 最大忙等次数，超过后使用yield
 * @param max_yield_attempts 最大yield次数，超过后使用sleep
 * @param sleep_duration_ms sleep持续时间（毫秒）
 */
inline void wait_based_on_attempts(size_t attempt_count, 
                                   size_t max_spin_attempts = 1000, 
                                   size_t max_yield_attempts = 5000,
                                   unsigned int sleep_duration_ms = 1) {
    if (attempt_count < max_spin_attempts) {
        // 忙等 - 什么都不做，继续循环
        return;
    } else if (attempt_count < max_spin_attempts + max_yield_attempts) {
        // 让出CPU
        std::this_thread::yield();
    } else {
        // 睡眠
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_duration_ms));
    }
}

// 将线程ID转换为整数表示
namespace detail {
    inline uint64_t thread_id_to_int(const std::thread::id& id) {
        return *reinterpret_cast<const uint64_t*>(&id);
    }
}

/**
 * @brief 跨进程互斥锁类
 * 
 * 该锁使用共享内存中的原子变量实现，支持进程崩溃时自动解锁。
 * 锁使用一个进程ID和时间戳的组合来标识当前持有锁的进程。
 * 如果持有锁的进程崩溃，其他进程可以检测到并自动接管锁。
 * 
 * 注意：该类的实例必须放置在共享内存中才能正常工作
 */
class ipc_mutex {
public:
    /**
     * @brief 构造函数
     */
    ipc_mutex();

    /**
     * @brief 析构函数
     * 
     * 如果当前进程持有锁，会自动释放
     */
    ~ipc_mutex();

    /**
     * @brief 尝试获取锁
     * @return 如果成功获取锁返回true，否则返回false
     */
    bool try_lock();

    /**
     * @brief 阻塞等待获取锁
     */
    void lock();

    /**
     * @brief 尝试获取锁，带超时
     * @param timeout_ms 超时时间（毫秒）
     * @return 如果成功获取锁返回true，否则返回false
     */
    bool try_lock_for(std::chrono::milliseconds timeout_ms);

    /**
     * @brief 释放锁
     */
    void unlock();

    /**
     * @brief 检查进程是否仍然活跃
     * @param pid 进程ID
     * @return 如果进程仍然活跃则返回true，否则返回false
     */
    static bool is_process_alive(pid_t pid);

private:
    // 抢占锁的条件
    enum class preempt_condition {
        none,            // 不能抢占
        not_owned,       // 没有进程持有锁
        owner_dead,      // 拥有者进程已死亡
        timeout          // 锁超时
    };

    // 数据成员，直接存储在共享内存中
    std::atomic<pid_t> m_owner_pid;     // 当前持有锁的进程ID
    std::atomic<uint64_t> m_lock_time;  // 锁定的时间戳（微秒）
    
    // 检查锁是否可以被抢占
    preempt_condition can_preempt() const;
};

/**
 * @brief 跨进程读写锁类
 * 
 * 该锁使用共享内存中的原子变量实现，支持进程崩溃时自动解锁。
 * 支持多个读者和一个写者的读写锁模式。
 * 
 * 注意：该类的实例必须放置在共享内存中才能正常工作
 */
class ipc_rw_mutex {
public:
    /**
     * @brief 构造函数
     */
    ipc_rw_mutex();

    /**
     * @brief 析构函数
     * 
     * 如果当前进程持有锁，会自动释放
     */
    ~ipc_rw_mutex();

    /**
     * @brief 尝试获取读锁
     * @return 如果成功获取锁返回true，否则返回false
     */
    bool try_lock_shared();

    /**
     * @brief 阻塞等待获取读锁
     */
    void lock_shared();

    /**
     * @brief 释放读锁
     */
    void unlock_shared();

    /**
     * @brief 尝试获取写锁
     * @return 如果成功获取锁返回true，否则返回false
     */
    bool try_lock();

    /**
     * @brief 阻塞等待获取写锁
     */
    void lock();

    /**
     * @brief 释放写锁
     */
    void unlock();

    /**
     * @brief 清理死亡进程的锁
     * 
     * 扫描所有持有锁的进程，清理已经死亡的进程的锁
     */
    void cleanup_dead_locks();
    
    /**
     * @brief 清理死亡或超时的读者
     * 
     * 扫描所有持有读锁的进程，清理已经死亡或超时的读者
     * @return 清理的读者数量
     */
    int aggressive_cleanup_readers();

private:
    ipc_mutex m_reader_mutex;    ///< 保护读者操作的互斥锁
    
    // 将原子类型改为普通类型，由m_reader_mutex保护
    pid_t m_writer_pid;             ///< 当前持有写锁的进程ID
    uint64_t m_write_time;          ///< 写锁的获取时间（微秒）
    
    size_t m_reader_count;          ///< 当前读者数量
    size_t m_reader_slot;           ///< 当前进程的读者槽位

    // 读者信息结构
    struct reader_info {
        pid_t pid;                  ///< 读者进程ID
        uint64_t read_time;         ///< 读锁的获取时间（微秒）
    };
    
    reader_info m_readers[MC_SHARED_MUTEX_MAX_READERS];  ///< 读者数组，支持多个并发读者

    // 内部方法
    bool register_reader();
    void unregister_reader();
    bool is_writer_alive() const;
    // 在已持有互斥锁的情况下清理死亡读者
    int aggressive_cleanup_readers_unsafe();
};

/**
 * @brief 进程间读写锁类，支持多线程和多进程同步
 * 
 * 此类提供读写锁功能，可用于多个进程间的同步。
 * 同时支持在单进程多线程环境下的使用。
 */
class shared_rw_mutex {
public:
    /**
     * @brief 构造函数
     * 
     * @param ipc_rw_mtx 共享内存中的IPC读写互斥锁引用
     */
    explicit shared_rw_mutex(ipc_rw_mutex& ipc_rw_mtx) 
        : m_ipc_mutex(ipc_rw_mtx), m_reader_count(0), m_writer_thread_id(0) {}
    
    ~shared_rw_mutex() {
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
            return false;  // 不允许递归锁定
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
    mutable std::mutex m_sync_mutex;    // 保护锁，确保多线程操作的原子性
    std::shared_mutex m_thread_mutex;   // 线程读写锁
    ipc_rw_mutex& m_ipc_mutex;         // 进程间读写锁，引用共享内存中的读写互斥锁
    
    unsigned int m_reader_count;        // 当前持有读锁的线程数
    int m_writer_thread_id;             // 当前持有写锁的线程ID
};

/**
 * 进程间互斥锁类，支持多线程和多进程同步
 * 
 * 此类提供基本的互斥锁功能，可用于多个进程间的同步。
 * 同时支持在单进程多线程环境下的使用。
 */
class shared_mutex {
public:
    /**
     * @brief 构造函数
     * 
     * @param ipc_mtx 共享内存中的IPC互斥锁引用
     */
    explicit shared_mutex(ipc_mutex& ipc_mtx) : m_ipc_mutex(ipc_mtx) {}
    
    ~shared_mutex() = default;

    bool try_lock() {
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
    bool try_lock_for(const std::chrono::duration<Rep, Period>& rel_time) {
        // 记录函数进入时间
        auto start_time = std::chrono::steady_clock::now();
        
        // 先尝试获取线程锁
        if (!m_thread_mutex.try_lock_for(rel_time)) {
            return false;
        }
        
        // 计算获取线程锁消耗的时间
        auto thread_lock_time = std::chrono::steady_clock::now();
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

    void lock() {
        // 先获取线程锁，再获取进程锁
        m_thread_mutex.lock();
        try {
            m_ipc_mutex.lock();
        } catch (...) {
            m_thread_mutex.unlock();
            throw;
        }
    }

    void unlock() {
        // 先释放进程锁，再释放线程锁
        m_ipc_mutex.unlock();
        m_thread_mutex.unlock();
    }

private:
    std::timed_mutex m_thread_mutex;  // 线程同步，使用timed_mutex支持超时
    ipc_mutex& m_ipc_mutex;          // 进程间同步，引用共享内存中的互斥锁
};

} // namespace mc::interprocess

#endif // MC_INTERPROCESS_SHARED_MUTEX_H 