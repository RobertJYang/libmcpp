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

#include "mc/interprocess/mutex/ipc_shared_mutex.h"
#include "mc/interprocess/mutex/detail/mutex_utils.h"
#include "mc/exception.h"
#include "mc/log.h"

#include <chrono>
#include <signal.h>
#include <sys/time.h>
#include <thread>
#include <unordered_set>

namespace mc::interprocess {

using namespace detail;

ipc_shared_mutex::ipc_shared_mutex()
    : m_writer_pid(0), m_write_time(0), m_reader_count(0), m_reader_slot(INVALID_ID), m_last_free_slot(0) {
    for (size_t i = 0; i < MAX_READERS; i++) {
        m_readers[i].pid       = 0;
        m_readers[i].read_time = 0;
    }
}

ipc_shared_mutex::~ipc_shared_mutex() {
    // 如果当前进程持有读锁，释放它
    if (m_reader_slot != INVALID_ID) {
        unlock_shared();
    }
    
    // 如果当前进程持有写锁，释放它
    if (m_writer_pid == getpid()) {
        unlock();
    }
}

bool ipc_shared_mutex::try_lock_shared() {
    pid_t pid = getpid();

    // 使用互斥锁保护整个操作
    std::lock_guard<ipc_mutex> guard(m_reader_mutex);

    // 检查是否有写锁
    if (m_writer_pid != 0 && m_writer_pid != pid) {
        // 有其他进程持有写锁
        if (is_writer_alive()) {
            return false;
        }

        wlog("写锁拥有者进程${pid}已死亡，清除写锁", ("pid", m_writer_pid));
        // 清除死亡写者的锁
        m_writer_pid = 0;
        m_write_time = 0;
    }

    // 在互斥锁保护下注册读者（使用不安全版本，因为已经持有锁）
    return register_reader_unsafe();
}

void ipc_shared_mutex::lock_shared() {
    // 使用新的等待策略
    size_t attempt_count = 0;
    while (!try_lock_shared()) {
        wait_based_on_attempts(attempt_count++);
    }
}

void ipc_shared_mutex::unlock_shared() {
    std::lock_guard<ipc_mutex> lock(m_reader_mutex);
    
    // 如果没有持有读锁，直接返回
    if (m_reader_slot == INVALID_ID) {
        return;
    }
    
    // 清除读者信息
    m_readers[m_reader_slot].pid = 0;
    m_readers[m_reader_slot].read_time = 0;
    
    // 更新最后一个已知的空闲槽位
    m_last_free_slot = m_reader_slot;
    
    m_reader_slot = INVALID_ID;
    
    // 减少读者计数
    if (m_reader_count > 0) {
        m_reader_count--;
    }
}

bool ipc_shared_mutex::try_lock() {
    pid_t pid = getpid();

    // 使用互斥锁保护整个操作
    std::lock_guard<ipc_mutex> guard(m_reader_mutex);

    // 如果已经持有写锁，不允许重入，返回失败（与ipc_mutex语义一致）
    if (m_writer_pid == pid) {
        wlog("进程${pid}尝试重复获取写锁，不允许重入，返回失败", ("pid", pid));
        return false;
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

void ipc_shared_mutex::lock() {
    // 使用新的等待策略
    size_t attempt_count = 0;
    while (!try_lock()) {
        wait_based_on_attempts(attempt_count++);
    }
}

void ipc_shared_mutex::unlock() {
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

void ipc_shared_mutex::cleanup_dead_locks() {
    std::lock_guard<ipc_mutex> guard(m_reader_mutex);
    aggressive_cleanup_readers_unsafe();
}

bool ipc_shared_mutex::register_reader_unsafe() {
    pid_t pid = getpid();
    
    // 检查当前进程是否已经是读者
    if (m_reader_slot != INVALID_ID) {
        return true;  // 当前进程已经注册为读者（使用已保存的槽位）
    }
    
    // 检查当前进程是否已经是写者
    if (m_writer_pid == pid) {
        wlog("当前进程已持有写锁，不能再获取读锁");
        return false;
    }
    
    // 查找空闲槽位
    size_t free_slot = find_free_reader_slot();
    if (free_slot == INVALID_ID) {
        dlog("没有可用的读者槽位，尝试清理死亡或超时的读者");
        if (aggressive_cleanup_readers_unsafe() > 0) {
            free_slot = find_free_reader_slot();
        }
    }
    
    // 如果仍然没有空闲槽位，返回失败
    if (free_slot == INVALID_ID) {
        dlog("没有可用的读者槽位，当前读者数量已达到最大值${max_readers}", ("max_readers", MAX_READERS));
        return false;
    }
    
    // 注册读者
    m_readers[free_slot].pid = pid;
    m_readers[free_slot].read_time = get_current_time_us();
    m_reader_slot = free_slot;
    m_reader_count++;
    
    return true;
}

bool ipc_shared_mutex::is_writer_alive() const {
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
    if (write_time > 0 && now - write_time > SHARED_MUTEX_TIMEOUT_US) {
        wlog("写锁已超时${time}秒", ("time", (now - write_time) / 1000000.0));
        return false;
    }

    return true;
}

// 检查读锁是否需要清理
bool ipc_shared_mutex::should_clean_reader(size_t slot, uint64_t now) const {
    pid_t reader_pid = m_readers[slot].pid;
    pid_t current_pid = getpid();
    
    // 空槽位或当前进程占用的不需要清理
    if (reader_pid == 0 || reader_pid == current_pid) {
        return false;
    }
    
    // 检查进程是否已死亡
    if (!ipc_mutex::is_process_alive(reader_pid)) {
        wlog("读锁拥有者进程${pid}已死亡，清除读锁", ("pid", reader_pid));
        return true;
    }
    
    // 检查读锁是否超时
    if (m_readers[slot].read_time > 0 && now - m_readers[slot].read_time > SHARED_MUTEX_TIMEOUT_US) {
        wlog("读锁已超时${time}秒，清除读锁",
             ("time", (now - m_readers[slot].read_time) / 1000000.0));
        return true;
    }
    
    return false;
}

// 清理读锁槽位
void ipc_shared_mutex::clean_reader_slot(size_t slot) {
    m_readers[slot].pid = 0;
    m_readers[slot].read_time = 0;
    
    // 更新最后一个已知的空闲槽位
    m_last_free_slot = slot;
}

// 清理已经死亡或超时的所有锁
int ipc_shared_mutex::aggressive_cleanup_readers_unsafe() {
    if (m_reader_count == 0) {
        return 0;  // 没有读者，不需要清理
    }
    
    uint64_t now = get_current_time_us();
    int cleaned_count = 0;
    
    // 清理死亡或超时的读者
    for (size_t i = 0; i < MAX_READERS; i++) {
        if (should_clean_reader(i, now)) {
            clean_reader_slot(i);
            cleaned_count++;
        }
    }
    
    // 清理死亡的写者
    pid_t pid = getpid();
    if (m_writer_pid != 0 && m_writer_pid != pid && !is_writer_alive()) {
        // 写者进程已死亡或超时，清除其写锁
        wlog("写锁拥有者进程${pid}已无效，清除写锁", ("pid", m_writer_pid));
        m_writer_pid = 0;
        m_write_time = 0;
    }
    
    // 更新读者计数
    if (cleaned_count > 0) {
        m_reader_count = std::max(0, static_cast<int>(m_reader_count) - cleaned_count);
        dlog("已清理${count}个读锁", ("count", cleaned_count));
    }
    
    return cleaned_count;
}

// 查找空闲的读者槽位
size_t ipc_shared_mutex::find_free_reader_slot() const {
    // 先检查上次记录的空闲槽位
    if (m_last_free_slot < MAX_READERS && m_readers[m_last_free_slot].pid == 0) {
        return m_last_free_slot;
    }
    
    // 否则遍历查找第一个空闲槽位
    for (size_t i = 0; i < MAX_READERS; i++) {
        if (m_readers[i].pid == 0) {
            return i;
        }
    }
    return INVALID_ID;
}

} // namespace mc::interprocess 