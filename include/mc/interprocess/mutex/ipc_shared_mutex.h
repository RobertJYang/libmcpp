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
 * @file ipc_shared_mutex.h
 * @brief 跨进程读写锁，支持进程崩溃时自动解锁
 */
#ifndef MC_INTERPROCESS_MUTEX_IPC_SHARED_MUTEX_H
#define MC_INTERPROCESS_MUTEX_IPC_SHARED_MUTEX_H

#include <mc/interprocess/mutex/ipc_mutex.h>

namespace mc::interprocess {

// 最大读者数量，可在构建时通过-DMC_SHARED_MUTEX_MAX_READERS=N配置
#ifndef MC_SHARED_MUTEX_MAX_READERS
#define MC_SHARED_MUTEX_MAX_READERS 32
#endif

// 使用宏定义的值
constexpr uint32_t MAX_READERS = MC_SHARED_MUTEX_MAX_READERS;

// 锁超时时间（微秒）
constexpr uint64_t SHARED_MUTEX_TIMEOUT_US = MC_SHARED_MUTEX_TIMEOUT_US;

constexpr size_t INVALID_ID = static_cast<size_t>(-1);

/**
 * @brief 跨进程读写锁类
 *
 * 该锁使用共享内存中的原子变量实现，支持进程崩溃时自动解锁。
 * 支持多个读者和一个写者的读写锁模式。
 *
 * 注意：该类的实例必须放置在共享内存中才能正常工作
 */
class MC_API ipc_shared_mutex {
public:
    /**
     * @brief 构造函数
     */
    ipc_shared_mutex();

    /**
     * @brief 析构函数
     *
     * 如果当前进程持有锁，会自动释放
     */
    ~ipc_shared_mutex();

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

private:
    ipc_mutex m_reader_mutex; ///< 保护读者操作的互斥锁

    pid_t    m_writer_pid; ///< 当前持有写锁的进程ID
    uint64_t m_write_time; ///< 写锁的获取时间（微秒）

    size_t m_reader_count;   ///< 当前读者数量
    size_t m_reader_slot;    ///< 当前进程的读者槽位
    size_t m_last_free_slot; ///< 最后一个已知的空闲槽位，用于加速分配

    // 读者信息结构
    struct reader_info {
        pid_t    pid;       ///< 读者进程ID
        uint64_t read_time; ///< 读锁的获取时间（微秒）
    };

    reader_info m_readers[MAX_READERS]; ///< 读者数组，支持多个并发读者

    // 内部方法
    bool register_reader();
    bool register_reader_unsafe();            ///< 不安全版本，假定调用者已持有m_reader_mutex锁
    int  aggressive_cleanup_readers_unsafe(); ///< 不安全版本，假定调用者已持有m_reader_mutex锁
    bool is_writer_alive() const;

    /// 查找空闲的读者槽位
    /// @return 找到的槽位索引，如果没有找到则返回INVALID_ID
    size_t find_free_reader_slot() const;

    /// 检查读锁是否需要清理
    /// @param slot 读锁槽位
    /// @param now 当前时间（微秒）
    /// @return 是否需要清理
    bool should_clean_reader(size_t slot, uint64_t now) const;

    /// 清理读锁槽位
    /// @param slot 要清理的槽位
    void clean_reader_slot(size_t slot);
};

} // namespace mc::interprocess

#endif // MC_INTERPROCESS_MUTEX_IPC_SHARED_MUTEX_H