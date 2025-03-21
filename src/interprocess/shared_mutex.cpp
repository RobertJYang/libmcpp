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

#include "mc/interprocess/shared_mutex.h"
#include "mc/exception.h"
#include "mc/log.h"

#include <chrono>
#include <signal.h>
#include <sys/time.h>
#include <thread>

namespace mc {
namespace interprocess {

//=============================================================================
// ipc_mutex 实现
//=============================================================================

ipc_mutex::ipc_mutex() {
    // 初始化锁数据
    m_owner_pid.store(0, std::memory_order_relaxed);
    m_lock_time.store(0, std::memory_order_relaxed);
}

ipc_mutex::~ipc_mutex() {
    // 如果当前进程持有锁，释放它
    pid_t pid = getpid();
    if (m_owner_pid.load(std::memory_order_acquire) == pid) {
        unlock();
    }
}

bool ipc_mutex::try_lock() {
    pid_t pid = getpid();

    // 如果已经持有锁，直接返回成功
    if (m_owner_pid.load(std::memory_order_acquire) == pid) {
        return true;
    }

    // 检查锁是否可以被抢占
    auto condition = can_preempt();
    if (condition == preempt_condition::none) {
        return false;
    }

    // 尝试获取锁
    pid_t expected = 0;
    if (condition == preempt_condition::not_owned) {
        // 锁空闲，直接尝试获取
        if (!m_owner_pid.compare_exchange_strong(expected, pid, std::memory_order_acq_rel)) {
            return false;
        }
    } else {
        // 锁需要被抢占（持有者已死亡或锁超时）
        // 获取当前锁的拥有者
        expected = m_owner_pid.load(std::memory_order_acquire);
        
        // 使用compare_exchange确保只有一个进程能成功抢占锁
        if (!m_owner_pid.compare_exchange_strong(expected, pid, std::memory_order_acq_rel)) {
            // 另一个进程已经抢占了锁
            return false;
        }
        
        // 在此处可能需要记录日志，表明锁被抢占
        ilog("锁被进程${new_pid}抢占，原拥有者：${old_pid}", ("new_pid", pid)("old_pid", expected));
    }

    // 设置锁定时间
    m_lock_time.store(get_current_time_us(), std::memory_order_release);

    return true;
}

void ipc_mutex::lock() {
    // 使用新的等待策略
    size_t attempt_count = 0;
    while (!try_lock()) {
        wait_based_on_attempts(attempt_count++);
    }
}

bool ipc_mutex::try_lock_for(std::chrono::milliseconds timeout_ms) {
    auto start = std::chrono::steady_clock::now();
    size_t attempt_count = 0;

    // 尝试获取锁，直到超时
    while (!try_lock()) {
        // 检查是否超时
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - start) >= timeout_ms) {
            return false;
        }

        // 使用新的等待策略
        wait_based_on_attempts(attempt_count++);
    }

    return true;
}

void ipc_mutex::unlock() {
    pid_t pid = getpid();

    // 如果当前进程不持有锁，直接返回
    if (m_owner_pid.load(std::memory_order_acquire) != pid) {
        return;
    }

    // 释放锁
    m_owner_pid.store(0, std::memory_order_release);
    m_lock_time.store(0, std::memory_order_release);
}

bool ipc_mutex::is_process_alive(pid_t pid) {
    if (pid <= 0) {
        return false;
    }

    // 尝试发送空信号检查进程存在性
    return kill(pid, 0) == 0 || errno != ESRCH;
}

ipc_mutex::preempt_condition ipc_mutex::can_preempt() const {
    // 获取当前锁的状态
    pid_t    owner     = m_owner_pid.load(std::memory_order_acquire);
    uint64_t lock_time = m_lock_time.load(std::memory_order_acquire);
    uint64_t now       = get_current_time_us();

    // 检查锁是否空闲
    if (owner == 0) {
        return preempt_condition::not_owned;
    }

    // 检查拥有者进程是否存活
    if (!is_process_alive(owner)) {
        ilog("锁拥有者进程${pid}已死亡，抢占锁", ("pid", owner));
        return preempt_condition::owner_dead;
    }

    // 检查锁是否超时
    if (now > lock_time && (now - lock_time) > MC_SHARED_MUTEX_TIMEOUT_US) {
        ilog("锁已超时(${duration}μs)，被进程${pid}持有，抢占锁",
             ("duration", now - lock_time)("pid", owner));
        return preempt_condition::timeout;
    }

    // 锁已被正常持有，不能抢占
    return preempt_condition::none;
}

//=============================================================================
// ipc_rw_mutex 实现
//=============================================================================

ipc_rw_mutex::ipc_rw_mutex()
    : m_writer_pid(0), m_write_time(0), m_reader_count(0), m_reader_slot(INVALID_ID) {
    // 初始化读者数组
    for (size_t i = 0; i < MC_SHARED_MUTEX_MAX_READERS; i++) {
        m_readers[i].pid       = 0;
        m_readers[i].read_time = 0;
    }
}

ipc_rw_mutex::~ipc_rw_mutex() {
    // 如果当前进程持有读锁，释放它
    unregister_reader();

    // 如果当前进程持有写锁，释放它
    std::lock_guard<ipc_mutex> guard(m_reader_mutex);
    if (m_writer_pid == getpid()) {
        // 在锁保护下释放写锁
        m_writer_pid = 0;
        m_write_time = 0;
    }
}

bool ipc_rw_mutex::try_lock_shared() {
    pid_t pid = getpid();

    // 使用互斥锁保护整个操作
    std::lock_guard<ipc_mutex> guard(m_reader_mutex);

    // 检查是否有写锁
    if (m_writer_pid != 0 && m_writer_pid != pid) {
        // 有其他进程持有写锁
        if (is_writer_alive()) {
            return false;
        }

        ilog("写锁拥有者进程${pid}已死亡，清除写锁", ("pid", m_writer_pid));
        // 清除死亡写者的锁
        m_writer_pid = 0;
        m_write_time = 0;
    }

    // 在互斥锁保护下注册读者
    return register_reader();
}

void ipc_rw_mutex::lock_shared() {
    // 使用新的等待策略
    size_t attempt_count = 0;
    while (!try_lock_shared()) {
        wait_based_on_attempts(attempt_count++);
    }
}

void ipc_rw_mutex::unlock_shared() {
    // 使用互斥锁保护读者注销操作
    std::lock_guard<ipc_mutex> guard(m_reader_mutex);
    unregister_reader();
}

bool ipc_rw_mutex::try_lock() {
    pid_t pid = getpid();

    // 使用互斥锁保护整个操作
    std::lock_guard<ipc_mutex> guard(m_reader_mutex);

    // 如果已经持有写锁，直接返回成功
    if (m_writer_pid == pid) {
        return true;
    }

    // 检查是否有其他写锁拥有者
    if (m_writer_pid != 0) {
        if (is_writer_alive()) {
            return false;
        }

        ilog("写锁拥有者进程${pid}已死亡，清除写锁", ("pid", m_writer_pid));
        // 清除死亡写者的锁
        m_writer_pid = 0;
        m_write_time = 0;
    }

    // 检查是否有任何活跃的读者
    if (m_reader_count > 0) {
        // 清理死亡或超时的读者
        aggressive_cleanup_readers_unsafe();
        
        // 如果还有活跃读者，不能获取写锁
        if (m_reader_count > 0) {
            return false;
        }
    }

    // 没有活跃读者，设置写锁
    m_writer_pid = pid;
    m_write_time = get_current_time_us();

    return true;
}

void ipc_rw_mutex::lock() {
    // 使用新的等待策略
    size_t attempt_count = 0;
    while (!try_lock()) {
        wait_based_on_attempts(attempt_count++);
    }
}

void ipc_rw_mutex::unlock() {
    pid_t pid = getpid();

    // 使用互斥锁保护写锁释放操作
    std::lock_guard<ipc_mutex> guard(m_reader_mutex);

    // 如果当前进程不持有写锁，直接返回
    if (m_writer_pid != pid) {
        return;
    }

    // 释放写锁
    m_writer_pid = 0;
    m_write_time = 0;
}

void ipc_rw_mutex::cleanup_dead_locks() {
    // 使用互斥锁保护所有锁清理操作
    std::lock_guard<ipc_mutex> guard(m_reader_mutex);

    // 清理死亡的读者
    for (size_t i = 0; i < MC_SHARED_MUTEX_MAX_READERS; i++) {
        pid_t reader_pid = m_readers[i].pid;
        if (reader_pid != 0 && reader_pid != getpid() &&
            !ipc_mutex::is_process_alive(reader_pid)) {
            // 读者进程已死亡，清除其读锁
            ilog("读锁拥有者进程${pid}已死亡，清除读锁", ("pid", reader_pid));
            m_readers[i].pid = 0;
            m_readers[i].read_time = 0;
            if (m_reader_count > 0) {
                m_reader_count--;
            }
        }
    }

    // 清理死亡的写者
    if (m_writer_pid != 0 && m_writer_pid != getpid() && !is_writer_alive()) {
        // 写者进程已死亡，清除其写锁
        ilog("写锁拥有者进程${pid}已死亡，清除写锁", ("pid", m_writer_pid));
        m_writer_pid = 0;
        m_write_time = 0;
    }
}

bool ipc_rw_mutex::register_reader() {
    pid_t pid = getpid();

    // 如果已经是读者，直接返回成功
    if (m_reader_slot != INVALID_ID && m_readers[m_reader_slot].pid == pid) {
        return true;
    }

    // 查找空闲槽位
    size_t free_slot = INVALID_ID;
    for (size_t i = 0; i < MC_SHARED_MUTEX_MAX_READERS; i++) {
        if (m_readers[i].pid == 0) {
            free_slot = i;
            break;
        }
    }

    // 如果没有空闲槽位，清理死亡或超时的读者
    if (free_slot == INVALID_ID) {
        ilog("没有可用的读者槽位，尝试清理死亡或超时的读者");
        int cleaned = aggressive_cleanup_readers_unsafe();
        
        if (cleaned > 0) {
            // 再次查找空闲槽位
            for (size_t i = 0; i < MC_SHARED_MUTEX_MAX_READERS; i++) {
                if (m_readers[i].pid == 0) {
                    free_slot = i;
                    break;
                }
            }
        }
    }

    // 如果仍然没有空闲槽位，返回失败
    if (free_slot == INVALID_ID) {
        elog("无法获取读锁：已达到最大读者数量${max}", ("max", MC_SHARED_MUTEX_MAX_READERS));
        return false;
    }

    // 注册读者
    m_readers[free_slot].pid = pid;
    m_readers[free_slot].read_time = get_current_time_us();
    m_reader_slot = free_slot;
    m_reader_count++;

    return true;
}

void ipc_rw_mutex::unregister_reader() {
    pid_t pid = getpid();

    // 如果当前进程不是读者，直接返回
    if (m_reader_slot == INVALID_ID || m_readers[m_reader_slot].pid != pid) {
        return;
    }

    // 释放读者槽位
    m_readers[m_reader_slot].pid = 0;
    m_readers[m_reader_slot].read_time = 0;
    if (m_reader_count > 0) {
        m_reader_count--;
    }
    m_reader_slot = INVALID_ID;
}

bool ipc_rw_mutex::is_writer_alive() const {
    // 注意：此函数假设调用者已经持有m_reader_mutex锁，或者在不需要准确性的场合调用

    // 快速检查：无写锁
    pid_t writer_pid = m_writer_pid;
    if (writer_pid == 0) {
        return false;
    }

    // 检查写锁拥有者是否存活
    if (!ipc_mutex::is_process_alive(writer_pid)) {
        return false;
    }

    // 检查写锁是否超时
    uint64_t write_time = m_write_time;
    uint64_t now = get_current_time_us();
    if (write_time > 0 && now - write_time > MC_SHARED_MUTEX_TIMEOUT_US) {
        ilog("写锁已超时${time}秒", ("time", (now - write_time) / 1000000.0));
        return false;
    }

    return true;
}

int ipc_rw_mutex::aggressive_cleanup_readers_unsafe() {
    int      cleaned_count = 0;
    pid_t    pid           = getpid();
    uint64_t now           = get_current_time_us();

    for (size_t i = 0; i < MC_SHARED_MUTEX_MAX_READERS; i++) {
        pid_t reader_pid = m_readers[i].pid;
        if (reader_pid != 0 && reader_pid != pid) {
            bool should_clean = false;

            // 检查进程是否存活
            if (!ipc_mutex::is_process_alive(reader_pid)) {
                should_clean = true;
                ilog("读锁拥有者进程${pid}已死亡，清除读锁", ("pid", reader_pid));
            }
            // 检查读锁是否超时
            else if (m_readers[i].read_time > 0 && now - m_readers[i].read_time > MC_SHARED_MUTEX_TIMEOUT_US) {
                should_clean = true;
                ilog("读锁已超时${time}秒，清除读锁",
                     ("time", (now - m_readers[i].read_time) / 1000000.0));
            }

            if (should_clean) {
                // 清除读者
                m_readers[i].pid = 0;
                m_readers[i].read_time = 0;
                if (m_reader_count > 0) {
                    m_reader_count--;
                }
                cleaned_count++;
            }
        }
    }

    return cleaned_count;
}

// 提供一个外部可调用的版本，自动获取互斥锁
int ipc_rw_mutex::aggressive_cleanup_readers() {
    std::lock_guard<ipc_mutex> guard(m_reader_mutex);
    return aggressive_cleanup_readers_unsafe();
}

} // namespace interprocess
} // namespace mc