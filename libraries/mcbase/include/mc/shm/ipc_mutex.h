/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef MC_SHM_IPC_MUTEX_H
#define MC_SHM_IPC_MUTEX_H

#include <atomic>
#include <chrono>
#include <cstdint>

#include <mc/common.h>

namespace mc::shm {

enum class lock_acquire_state : std::uint32_t {
    failed = 0,
    acquired = 1,
    recovered = 2,
};

class MC_API ipc_mutex {
public:
    ipc_mutex() noexcept;

    lock_acquire_state try_lock_ex() noexcept;
    bool               try_lock() noexcept;

    lock_acquire_state lock_ex() noexcept;
    void               lock() noexcept;

    lock_acquire_state try_lock_for_ex(std::chrono::milliseconds timeout) noexcept;
    bool               try_lock_for(std::chrono::milliseconds timeout) noexcept;

    void unlock() noexcept;

    bool refresh_lease() noexcept;
    bool is_locked_by_current_process() const noexcept;
    bool is_locked_by_current_thread() const noexcept;

    static bool is_process_alive(std::uint32_t pid) noexcept;

private:
    bool can_preempt(std::uint64_t owner_tag, std::uint64_t lease_deadline_ns) const noexcept;
    void set_owner(std::uint64_t owner_tag, std::uint64_t owner_thread_id) noexcept;

    std::atomic<std::uint64_t> m_owner_tag{0};
    std::atomic<std::uint64_t> m_owner_thread_id{0};
    std::atomic<std::uint64_t> m_lease_deadline_ns{0};
    std::atomic<std::uint32_t> m_wait_sequence{0};
    std::uint64_t              m_lease_timeout_ns{2ULL * 1000ULL * 1000ULL * 1000ULL};
};

class ipc_mutex_guard {
public:
    explicit ipc_mutex_guard(ipc_mutex& mutex) noexcept : m_mutex(&mutex)
    {
        m_mutex->lock();
    }

    ~ipc_mutex_guard()
    {
        if (m_mutex != nullptr) {
            m_mutex->unlock();
        }
    }

    ipc_mutex_guard(const ipc_mutex_guard&) = delete;
    ipc_mutex_guard& operator=(const ipc_mutex_guard&) = delete;

private:
    ipc_mutex* m_mutex;
};

} // namespace mc::shm

#endif // MC_SHM_IPC_MUTEX_H
