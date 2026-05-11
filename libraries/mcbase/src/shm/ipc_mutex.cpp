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

#include <mc/shm/ipc_mutex.h>

#include <thread>

#include <mc/shm/detail/clock.h>
#include <mc/shm/detail/process_identity.h>
#include <mc/shm/detail/process_monitor.h>
#include <mc/shm/detail/wait_backend.h>

namespace mc::shm {
namespace {

constexpr auto retry_wait_slice = std::chrono::milliseconds(2);

} // namespace

ipc_mutex::ipc_mutex() noexcept = default;

lock_acquire_state ipc_mutex::try_lock_ex() noexcept
{
    const auto my_tag = detail::current_process_identity().tag();
    const auto my_tid = detail::current_thread_id_u64();

    auto owner_tag = m_owner_tag.load(std::memory_order_acquire);
    if (owner_tag == my_tag) {
        // 同一进程已经持有，检查是否同一线程（禁止重入）或其他线程（需等待）。
        return lock_acquire_state::failed;
    }

    if (owner_tag == 0) {
        std::uint64_t expected = 0;
        if (m_owner_tag.compare_exchange_strong(expected, my_tag, std::memory_order_acq_rel,
                                                std::memory_order_acquire)) {
            set_owner(my_tag, my_tid);
            return lock_acquire_state::acquired;
        }
        return lock_acquire_state::failed;
    }

    const auto deadline_ns = m_lease_deadline_ns.load(std::memory_order_acquire);
    if (!can_preempt(owner_tag, deadline_ns)) {
        return lock_acquire_state::failed;
    }

    auto expected = owner_tag;
    if (m_owner_tag.compare_exchange_strong(expected, my_tag, std::memory_order_acq_rel,
                                            std::memory_order_acquire)) {
        set_owner(my_tag, my_tid);
        return lock_acquire_state::recovered;
    }

    return lock_acquire_state::failed;
}

bool ipc_mutex::try_lock() noexcept
{
    return try_lock_ex() != lock_acquire_state::failed;
}

lock_acquire_state ipc_mutex::lock_ex() noexcept
{
    while (true) {
        const auto state = try_lock_ex();
        if (state != lock_acquire_state::failed) {
            return state;
        }

        const auto sequence = m_wait_sequence.load(std::memory_order_acquire);
        detail::wait_backend::wait(m_wait_sequence, sequence, retry_wait_slice);
    }
}

void ipc_mutex::lock() noexcept
{
    (void)lock_ex();
}

lock_acquire_state ipc_mutex::try_lock_for_ex(std::chrono::milliseconds timeout) noexcept
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (true) {
        const auto state = try_lock_ex();
        if (state != lock_acquire_state::failed) {
            return state;
        }

        if (std::chrono::steady_clock::now() >= deadline) {
            return lock_acquire_state::failed;
        }

        const auto sequence = m_wait_sequence.load(std::memory_order_acquire);
        detail::wait_backend::wait(m_wait_sequence, sequence, retry_wait_slice);
    }
}

bool ipc_mutex::try_lock_for(std::chrono::milliseconds timeout) noexcept
{
    return try_lock_for_ex(timeout) != lock_acquire_state::failed;
}

void ipc_mutex::unlock() noexcept
{
    const auto my_tag = detail::current_process_identity().tag();
    const auto my_tid = detail::current_thread_id_u64();
    if (m_owner_tag.load(std::memory_order_acquire) != my_tag) {
        return;
    }
    if (m_owner_thread_id.load(std::memory_order_acquire) != my_tid) {
        // 同进程但不同线程误调，忽略以防把别的线程的锁释放掉。
        return;
    }

    m_lease_deadline_ns.store(0, std::memory_order_release);
    m_owner_thread_id.store(0, std::memory_order_release);
    m_owner_tag.store(0, std::memory_order_release);
    m_wait_sequence.fetch_add(1, std::memory_order_acq_rel);
    detail::wait_backend::notify_all(m_wait_sequence);
}

bool ipc_mutex::refresh_lease() noexcept
{
    const auto my_tag = detail::current_process_identity().tag();
    const auto my_tid = detail::current_thread_id_u64();
    if (m_owner_tag.load(std::memory_order_acquire) != my_tag) {
        return false;
    }
    if (m_owner_thread_id.load(std::memory_order_acquire) != my_tid) {
        return false;
    }

    m_lease_deadline_ns.store(detail::monotonic_now_ns() + m_lease_timeout_ns, std::memory_order_release);
    return true;
}

bool ipc_mutex::is_locked_by_current_process() const noexcept
{
    return m_owner_tag.load(std::memory_order_acquire) == detail::current_process_identity().tag();
}

bool ipc_mutex::is_locked_by_current_thread() const noexcept
{
    return is_locked_by_current_process() &&
           m_owner_thread_id.load(std::memory_order_acquire) == detail::current_thread_id_u64();
}

bool ipc_mutex::is_process_alive(std::uint32_t pid) noexcept
{
    return detail::is_process_alive(static_cast<pid_t>(pid));
}

bool ipc_mutex::can_preempt(std::uint64_t owner_tag, std::uint64_t lease_deadline_ns) const noexcept
{
    if (owner_tag == 0) {
        return true;
    }

    const auto owner = detail::unpack_process_identity(owner_tag);
    if (!is_process_alive(owner.pid)) {
        return true;
    }

    return lease_deadline_ns != 0 && detail::monotonic_now_ns() >= lease_deadline_ns;
}

void ipc_mutex::set_owner(std::uint64_t owner_tag, std::uint64_t owner_thread_id) noexcept
{
    m_owner_thread_id.store(owner_thread_id, std::memory_order_release);
    m_owner_tag.store(owner_tag, std::memory_order_release);
    m_lease_deadline_ns.store(detail::monotonic_now_ns() + m_lease_timeout_ns, std::memory_order_release);
    m_wait_sequence.fetch_add(1, std::memory_order_acq_rel);
    detail::wait_backend::notify_all(m_wait_sequence);
}

} // namespace mc::shm
