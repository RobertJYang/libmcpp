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

#include "mc/interprocess/mutex/ipc_mutex.h"
#include "mc/interprocess/mutex/detail/mutex_utils.h"
#include "mc/exception.h"
#include "mc/log.h"

#include <chrono>
#include <signal.h>
#include <sys/time.h>
#include <thread>

namespace mc::interprocess {

using namespace detail;

ipc_mutex::ipc_mutex()
{
    // 初始化锁数据
    m_owner_pid.store(0, std::memory_order_relaxed);
    m_lock_time.store(0, std::memory_order_relaxed);
}

ipc_mutex::~ipc_mutex()
{
    // 如果当前进程持有锁，释放它
    pid_t pid = getpid();
    if (m_owner_pid.load(std::memory_order_acquire) == pid) {
        unlock();
    }
}

bool ipc_mutex::try_lock()
{
    pid_t pid = getpid();

    // 如果已经持有锁，返回失败，与std::mutex语义一致
    if (m_owner_pid.load(std::memory_order_acquire) == pid) {
        wlog("进程${pid}尝试重复获取互斥锁，不允许重入，返回失败", ("pid", pid));
        return false;
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

void ipc_mutex::lock()
{
    // 使用新的等待策略
    size_t attempt_count = 0;
    while (!try_lock()) {
        wait_based_on_attempts(attempt_count++);
    }
}

bool ipc_mutex::try_lock_for(std::chrono::milliseconds timeout_ms)
{
    auto   start         = std::chrono::steady_clock::now();
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

void ipc_mutex::unlock()
{
    pid_t pid = getpid();

    // 如果当前进程不持有锁，直接返回
    if (m_owner_pid.load(std::memory_order_acquire) != pid) {
        return;
    }

    // 释放锁
    m_owner_pid.store(0, std::memory_order_release);
    m_lock_time.store(0, std::memory_order_release);
}

bool ipc_mutex::is_process_alive(pid_t pid)
{
    if (pid <= 0) {
        return false;
    }

    // 尝试发送空信号检查进程存在性
    return kill(pid, 0) == 0 || errno != ESRCH;
}

ipc_mutex::preempt_condition ipc_mutex::can_preempt() const
{
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

} // namespace mc::interprocess