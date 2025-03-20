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


#include "mc/interprocess/shared_memory.h"

namespace mc {
namespace interprocess {

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

/**
 * @brief 跨进程互斥锁类
 * 
 * 该锁使用共享内存中的原子变量实现，支持进程崩溃时自动解锁。
 * 锁使用一个进程ID和时间戳的组合来标识当前持有锁的进程。
 * 如果持有锁的进程崩溃，其他进程可以检测到并自动接管锁。
 * 
 * 注意：该类的实例必须放置在共享内存中才能正常工作
 */
class shared_mutex {
public:
    /**
     * @brief 构造函数
     */
    shared_mutex();

    /**
     * @brief 析构函数
     * 
     * 如果当前进程持有锁，会自动释放
     */
    ~shared_mutex();

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
     * @brief 检查当前进程是否持有锁
     * @return 如果当前进程持有锁返回true，否则返回false
     */
    bool owns_lock() const;

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
    
    // 获取当前时间（微秒）
    static uint64_t get_current_time_us();
};

/**
 * @brief 跨进程读写锁类
 * 
 * 该锁使用共享内存中的原子变量实现，支持进程崩溃时自动解锁。
 * 支持多个读者和一个写者的读写锁模式。
 * 
 * 注意：该类的实例必须放置在共享内存中才能正常工作
 */
class shared_rw_mutex {
public:
    /**
     * @brief 构造函数
     */
    shared_rw_mutex();

    /**
     * @brief 析构函数
     * 
     * 如果当前进程持有锁，会自动释放
     */
    ~shared_rw_mutex();

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
    shared_mutex m_reader_mutex;    ///< 保护读者操作的互斥锁
    
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
    static uint64_t get_current_time_us();
};

} // namespace interprocess
} // namespace mc

#endif // MC_INTERPROCESS_SHARED_MUTEX_H 