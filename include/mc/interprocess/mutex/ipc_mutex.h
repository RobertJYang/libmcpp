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
 * @file ipc_mutex.h
 * @brief 跨进程互斥锁，支持进程崩溃时自动解锁
 */
#ifndef MC_INTERPROCESS_MUTEX_IPC_MUTEX_H
#define MC_INTERPROCESS_MUTEX_IPC_MUTEX_H

#include <atomic>
#include <chrono>
#include <unistd.h>
#include <sys/time.h>

namespace mc::interprocess {

// 锁超时时间（微秒），可通过编译参数覆盖
#ifndef MC_SHARED_MUTEX_TIMEOUT_US
#define MC_SHARED_MUTEX_TIMEOUT_US 10000000  // 默认10秒
#endif

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

} // namespace mc::interprocess

#endif // MC_INTERPROCESS_MUTEX_IPC_MUTEX_H 