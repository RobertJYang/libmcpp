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
// shared_mutex 实现
//=============================================================================

shared_mutex::shared_mutex() {
    // 初始化锁数据
    m_owner_pid.store(0, std::memory_order_relaxed);
    m_lock_time.store(0, std::memory_order_relaxed);
}

shared_mutex::~shared_mutex() {
    // 如果当前进程持有锁，释放它
    if (owns_lock()) {
        unlock();
    }
}

bool shared_mutex::try_lock() {
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

void shared_mutex::lock() {
    // 使用新的等待策略
    size_t attempt_count = 0;
    while (!try_lock()) {
        wait_based_on_attempts(attempt_count++);
    }
}

bool shared_mutex::try_lock_for(std::chrono::milliseconds timeout_ms) {
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

void shared_mutex::unlock() {
    pid_t pid = getpid();

    // 如果当前进程不持有锁，直接返回
    if (m_owner_pid.load(std::memory_order_acquire) != pid) {
        return;
    }

    // 释放锁
    m_owner_pid.store(0, std::memory_order_release);
    m_lock_time.store(0, std::memory_order_release);
}

bool shared_mutex::owns_lock() const {
    return m_owner_pid.load(std::memory_order_acquire) == getpid();
}

bool shared_mutex::is_process_alive(pid_t pid) {
    if (pid <= 0) {
        return false;
    }

    // 尝试发送空信号检查进程存在性
    return kill(pid, 0) == 0 || errno != ESRCH;
}

shared_mutex::preempt_condition shared_mutex::can_preempt() const {
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
    if (lock_time > 0 && now - lock_time > MC_SHARED_MUTEX_TIMEOUT_US) {
        ilog("锁已超时${time}秒，抢占锁", ("time", (now - lock_time) / 1000000.0));
        return preempt_condition::timeout;
    }

    // 不能抢占
    return preempt_condition::none;
}

uint64_t shared_mutex::get_current_time_us() {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return static_cast<uint64_t>(tv.tv_sec) * 1000000 + tv.tv_usec;
}

//=============================================================================
// shared_rw_mutex 实现
//=============================================================================

shared_rw_mutex::shared_rw_mutex()
    : m_writer_pid(0), m_write_time(0), m_reader_count(0), m_reader_slot(INVALID_ID) {
    // 初始化读者数组
    for (size_t i = 0; i < MC_SHARED_MUTEX_MAX_READERS; i++) {
        m_readers[i].pid       = 0;
        m_readers[i].read_time = 0;
    }
}

shared_rw_mutex::~shared_rw_mutex() {
    // 如果当前进程持有读锁，释放它
    unregister_reader();

    // 如果当前进程持有写锁，释放它
    std::lock_guard<shared_mutex> guard(m_reader_mutex);
    if (m_writer_pid == getpid()) {
        // 在锁保护下释放写锁
        m_writer_pid = 0;
        m_write_time = 0;
    }
}

bool shared_rw_mutex::try_lock_shared() {
    pid_t pid = getpid();

    // 使用互斥锁保护整个操作
    std::lock_guard<shared_mutex> guard(m_reader_mutex);

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

void shared_rw_mutex::lock_shared() {
    // 使用新的等待策略
    size_t attempt_count = 0;
    while (!try_lock_shared()) {
        wait_based_on_attempts(attempt_count++);
    }
}

void shared_rw_mutex::unlock_shared() {
    // 使用互斥锁保护读者注销操作
    std::lock_guard<shared_mutex> guard(m_reader_mutex);
    unregister_reader();
}

bool shared_rw_mutex::try_lock() {
    pid_t pid = getpid();

    // 使用互斥锁保护整个操作
    std::lock_guard<shared_mutex> guard(m_reader_mutex);

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

    // 积极清理死亡的读者
    int cleaned = aggressive_cleanup_readers_unsafe();
    if (cleaned > 0) {
        ilog("写进程${pid}清理了${count}个死亡/过期读者", ("pid", pid)("count", cleaned));
    }

    // 检查是否有读者
    if (m_reader_count > 0) {
        return false;
    }

    // 设置写锁
    m_writer_pid = pid;
    m_write_time = get_current_time_us();
    return true;
}

void shared_rw_mutex::lock() {
    // 使用新的等待策略
    size_t attempt_count = 0;
    while (!try_lock()) {
        wait_based_on_attempts(attempt_count++);
    }
}

void shared_rw_mutex::unlock() {
    pid_t pid = getpid();

    // 使用互斥锁保护写锁释放操作
    std::lock_guard<shared_mutex> guard(m_reader_mutex);

    // 如果当前进程不持有写锁，直接返回
    if (m_writer_pid != pid) {
        return;
    }

    // 释放写锁
    m_writer_pid = 0;
    m_write_time = 0;
}

void shared_rw_mutex::cleanup_dead_locks() {
    // 使用互斥锁保护所有锁清理操作
    std::lock_guard<shared_mutex> guard(m_reader_mutex);

    // 清理死亡的读者
    int cleaned_readers = 0;
    for (size_t i = 0; i < MC_SHARED_MUTEX_MAX_READERS; i++) {
        pid_t reader_pid = m_readers[i].pid;
        if (reader_pid != 0 && reader_pid != getpid() &&
            !shared_mutex::is_process_alive(reader_pid)) {
            // 读者进程已死亡，清除其读锁
            ilog("读锁拥有者进程${pid}已死亡，清除读锁", ("pid", reader_pid));
            m_readers[i].pid       = 0;
            m_readers[i].read_time = 0;
            m_reader_count--;
            cleaned_readers++;
        }
    }

    if (cleaned_readers > 0) {
        ilog("清理了${count}个死亡读者的读锁", ("count", cleaned_readers));
    }

    // 清理死亡的写者
    if (m_writer_pid != 0 && !is_writer_alive()) {
        pid_t dead_writer = m_writer_pid;
        // 清除写锁
        m_writer_pid = 0;
        m_write_time = 0;
        ilog("写锁拥有者进程${pid}已死亡，已清除写锁", ("pid", dead_writer));
    }
}

bool shared_rw_mutex::register_reader() {
    pid_t pid = getpid();

    // 注意：此函数假设调用者已经持有m_reader_mutex锁

    // 再次检查是否有写锁
    if (m_writer_pid != 0) {
        return false;
    }

    // 检查是否已经注册
    if (m_reader_slot != INVALID_ID) {
        // 已经有一个槽位，检查是否有效
        if (m_readers[m_reader_slot].pid == pid) {
            // 已经注册，直接返回成功
            return true;
        }
        // 槽位无效，重置
        m_reader_slot = INVALID_ID;
    }

    // 查找现有槽位（可能在其他地方被注册）
    for (size_t i = 0; i < MC_SHARED_MUTEX_MAX_READERS; i++) {
        if (m_readers[i].pid == pid) {
            m_reader_slot = i;
            return true;
        }
    }

    // 查找空闲槽位并注册
    for (size_t i = 0; i < MC_SHARED_MUTEX_MAX_READERS; i++) {
        if (m_readers[i].pid == 0) {
            m_reader_slot = i;
            m_readers[i].pid = pid;
            m_readers[i].read_time = get_current_time_us();
            m_reader_count++;
            ilog("进程${pid}注册在槽位${slot}，当前读者数量${count}", 
                 ("pid", pid)("slot", i)("count", m_reader_count));
            return true;
        }
    }

    wlog("进程${pid}无法注册读锁，所有槽位已满", ("pid", pid));
    return false;
}

void shared_rw_mutex::unregister_reader() {
    pid_t pid = getpid();

    // 注意：此函数假设调用者已经持有m_reader_mutex锁

    // 首先检查当前槽位是否有效
    if (m_reader_slot == INVALID_ID) {
        return;
    }

    if (m_readers[m_reader_slot].pid == pid) {
        // 清除当前槽位信息
        m_readers[m_reader_slot].pid       = 0;
        m_readers[m_reader_slot].read_time = 0;
        m_reader_count--;
        ilog("进程${pid}从槽位${slot}注销，当前读者数量${count}",
             ("pid", pid)("slot", m_reader_slot)("count", m_reader_count));
        m_reader_slot = INVALID_ID;
        return;
    }

    // 如果当前槽位无效或不匹配，搜索所有槽位
    for (size_t i = 0; i < MC_SHARED_MUTEX_MAX_READERS; i++) {
        if (m_readers[i].pid == pid) {
            m_readers[i].pid       = 0;
            m_readers[i].read_time = 0;
            m_reader_count--;
            ilog("进程${pid}从槽位${slot}注销，当前读者数量${count}",
                 ("pid", pid)("slot", i)("count", m_reader_count));
            return;
        }
    }

    wlog("进程${pid}尝试注销读锁，但未找到对应槽位", ("pid", pid));
}

bool shared_rw_mutex::is_writer_alive() const {
    // 注意：此函数假设调用者已经持有m_reader_mutex锁，或者在不需要准确性的场合调用

    // 获取当前值
    pid_t writer_pid = m_writer_pid; // 局部复制，避免竞态条件

    if (writer_pid == 0) {
        return false;
    }

    // 检查写锁拥有者是否存活
    if (!shared_mutex::is_process_alive(writer_pid)) {
        return false;
    }

    // 检查写锁是否超时
    uint64_t write_time = m_write_time; // 局部复制，避免竞态条件
    uint64_t now        = get_current_time_us();
    if (write_time > 0 && now - write_time > MC_SHARED_MUTEX_TIMEOUT_US) {
        ilog("写锁已超时${time}秒", ("time", (now - write_time) / 1000000.0));
        return false;
    }

    return true;
}

uint64_t shared_rw_mutex::get_current_time_us() {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return static_cast<uint64_t>(tv.tv_sec) * 1000000 + tv.tv_usec;
}

// 重命名为aggressive_cleanup_readers_unsafe，表示需要在互斥锁保护下调用
int shared_rw_mutex::aggressive_cleanup_readers_unsafe() {
    int      cleaned_count = 0;
    pid_t    pid           = getpid();
    uint64_t now           = get_current_time_us();

    // 注意：此函数假设调用者已经持有m_reader_mutex锁

    for (size_t i = 0; i < MC_SHARED_MUTEX_MAX_READERS; i++) {
        pid_t reader_pid = m_readers[i].pid;
        if (reader_pid != 0 && reader_pid != pid) {
            bool should_clean = false;

            // 检查进程是否存活
            if (!shared_mutex::is_process_alive(reader_pid)) {
                should_clean = true;
                ilog("读锁拥有者进程${pid}已死亡，清除读锁", ("pid", reader_pid));
            } else if (m_readers[i].read_time > 0 &&
                       now - m_readers[i].read_time > MC_SHARED_MUTEX_TIMEOUT_US) {
                // 检查读锁是否超时
                should_clean = true;
                ilog("读锁拥有者进程${pid}的锁已超时${time}秒，清除",
                     ("pid", reader_pid)("time", (now - m_readers[i].read_time) / 1000000.0));
            }

            if (should_clean) {
                // 清除读锁
                m_readers[i].pid       = 0;
                m_readers[i].read_time = 0;
                m_reader_count--;
                cleaned_count++;
            }
        }
    }

    return cleaned_count;
}

// 提供一个外部可调用的版本，自动获取互斥锁
int shared_rw_mutex::aggressive_cleanup_readers() {
    std::lock_guard<shared_mutex> guard(m_reader_mutex);
    return aggressive_cleanup_readers_unsafe();
}

} // namespace interprocess
} // namespace mc