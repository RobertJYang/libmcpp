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

#ifndef MC_SHM_IPC_SHARED_MUTEX_H
#define MC_SHM_IPC_SHARED_MUTEX_H

#include <atomic>
#include <chrono>
#include <cstdint>

#include <mc/common.h>
#include <mc/shm/ipc_mutex.h>

namespace mc::shm {

class MC_API ipc_shared_mutex {
public:
    static constexpr std::size_t max_readers = 32;

    ipc_shared_mutex() noexcept;

    lock_acquire_state try_lock_shared_ex() noexcept;
    bool               try_lock_shared() noexcept;
    lock_acquire_state lock_shared_ex() noexcept;
    void               lock_shared() noexcept;
    void               unlock_shared() noexcept;
    bool               refresh_shared_lease() noexcept;

    lock_acquire_state try_lock_ex() noexcept;
    bool               try_lock() noexcept;
    lock_acquire_state lock_ex() noexcept;
    void               lock() noexcept;
    lock_acquire_state try_lock_for_ex(std::chrono::milliseconds timeout) noexcept;
    bool               try_lock_for(std::chrono::milliseconds timeout) noexcept;
    void               unlock() noexcept;
    bool               refresh_exclusive_lease() noexcept;

    std::size_t cleanup_dead_holders() noexcept;

private:
    struct reader_slot {
        std::atomic<std::uint64_t> owner_tag{0};
        std::atomic<std::uint64_t> owner_thread_id{0};
        std::atomic<std::uint64_t> lease_deadline_ns{0};
    };

    lock_acquire_state do_try_lock_exclusive() noexcept;
    lock_acquire_state do_try_lock_shared() noexcept;
    std::size_t        cleanup_dead_holders_unsafe() noexcept;
    bool               writer_is_active_unsafe(std::uint64_t current_tag, std::uint64_t current_thread_id) const noexcept;
    bool               tag_is_stale(std::uint64_t tag, std::uint64_t deadline_ns) const noexcept;
    std::size_t        find_reader_slot_unsafe(std::uint64_t current_tag, std::uint64_t current_thread_id) const noexcept;
    std::size_t        find_free_reader_slot_unsafe() const noexcept;
    bool               has_blocking_reader_unsafe(std::uint64_t current_tag, std::uint64_t current_thread_id) const noexcept;
    void               notify_waiters() noexcept;

    ipc_mutex                  m_control_mutex{};
    std::atomic<std::uint64_t> m_writer_tag{0};
    std::atomic<std::uint64_t> m_writer_thread_id{0};
    std::atomic<std::uint64_t> m_writer_deadline_ns{0};
    std::atomic<std::uint32_t> m_wait_sequence{0};
    reader_slot                m_readers[max_readers]{};
    std::uint64_t              m_lease_timeout_ns{2ULL * 1000ULL * 1000ULL * 1000ULL};
};

class ipc_shared_lock_guard {
public:
    explicit ipc_shared_lock_guard(ipc_shared_mutex& mutex) noexcept : m_mutex(&mutex)
    {
        m_mutex->lock_shared();
    }

    ~ipc_shared_lock_guard()
    {
        if (m_mutex != nullptr) {
            m_mutex->unlock_shared();
        }
    }

    ipc_shared_lock_guard(const ipc_shared_lock_guard&) = delete;
    ipc_shared_lock_guard& operator=(const ipc_shared_lock_guard&) = delete;

private:
    ipc_shared_mutex* m_mutex;
};

class ipc_unique_lock_guard {
public:
    explicit ipc_unique_lock_guard(ipc_shared_mutex& mutex) noexcept : m_mutex(&mutex)
    {
        m_mutex->lock();
    }

    ~ipc_unique_lock_guard()
    {
        if (m_mutex != nullptr) {
            m_mutex->unlock();
        }
    }

    ipc_unique_lock_guard(const ipc_unique_lock_guard&) = delete;
    ipc_unique_lock_guard& operator=(const ipc_unique_lock_guard&) = delete;

private:
    ipc_shared_mutex* m_mutex;
};

} // namespace mc::shm

#endif // MC_SHM_IPC_SHARED_MUTEX_H
