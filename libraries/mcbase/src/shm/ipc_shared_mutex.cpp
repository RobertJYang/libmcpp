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

#include <mc/shm/ipc_shared_mutex.h>

#include <algorithm>
#include <thread>

#include <mc/shm/detail/clock.h>
#include <mc/shm/detail/process_identity.h>
#include <mc/shm/detail/wait_backend.h>

namespace mc::shm {
namespace {

constexpr auto retry_wait_slice = std::chrono::milliseconds(2);

} // namespace

ipc_shared_mutex::ipc_shared_mutex() noexcept = default;

lock_acquire_state ipc_shared_mutex::try_lock_shared_ex() noexcept
{
    return do_try_lock_shared();
}

bool ipc_shared_mutex::try_lock_shared() noexcept
{
    return try_lock_shared_ex() != lock_acquire_state::failed;
}

lock_acquire_state ipc_shared_mutex::lock_shared_ex() noexcept
{
    while (true) {
        const auto state = try_lock_shared_ex();
        if (state != lock_acquire_state::failed) {
            return state;
        }
        const auto sequence = m_wait_sequence.load(std::memory_order_acquire);
        detail::wait_backend::wait(m_wait_sequence, sequence, retry_wait_slice);
    }
}

void ipc_shared_mutex::lock_shared() noexcept
{
    (void)lock_shared_ex();
}

void ipc_shared_mutex::unlock_shared() noexcept
{
    const auto      current_tag = detail::current_process_identity().tag();
    const auto      current_tid = detail::current_thread_id_u64();
    ipc_mutex_guard guard(m_control_mutex);

    const auto slot = find_reader_slot_unsafe(current_tag, current_tid);
    if (slot == max_readers) {
        return;
    }

    m_readers[slot].lease_deadline_ns.store(0, std::memory_order_release);
    m_readers[slot].owner_thread_id.store(0, std::memory_order_release);
    m_readers[slot].owner_tag.store(0, std::memory_order_release);
    notify_waiters();
}

bool ipc_shared_mutex::refresh_shared_lease() noexcept
{
    const auto      current_tag = detail::current_process_identity().tag();
    const auto      current_tid = detail::current_thread_id_u64();
    ipc_mutex_guard guard(m_control_mutex);

    const auto slot = find_reader_slot_unsafe(current_tag, current_tid);
    if (slot == max_readers) {
        return false;
    }

    m_readers[slot].lease_deadline_ns.store(detail::monotonic_now_ns() + m_lease_timeout_ns,
                                            std::memory_order_release);
    return true;
}

lock_acquire_state ipc_shared_mutex::try_lock_ex() noexcept
{
    return do_try_lock_exclusive();
}

bool ipc_shared_mutex::try_lock() noexcept
{
    return try_lock_ex() != lock_acquire_state::failed;
}

lock_acquire_state ipc_shared_mutex::lock_ex() noexcept
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

void ipc_shared_mutex::lock() noexcept
{
    (void)lock_ex();
}

lock_acquire_state ipc_shared_mutex::try_lock_for_ex(std::chrono::milliseconds timeout) noexcept
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

bool ipc_shared_mutex::try_lock_for(std::chrono::milliseconds timeout) noexcept
{
    return try_lock_for_ex(timeout) != lock_acquire_state::failed;
}

void ipc_shared_mutex::unlock() noexcept
{
    const auto      current_tag = detail::current_process_identity().tag();
    const auto      current_tid = detail::current_thread_id_u64();
    ipc_mutex_guard guard(m_control_mutex);

    if (m_writer_tag.load(std::memory_order_acquire) != current_tag) {
        return;
    }
    if (m_writer_thread_id.load(std::memory_order_acquire) != current_tid) {
        return;
    }

    m_writer_deadline_ns.store(0, std::memory_order_release);
    m_writer_thread_id.store(0, std::memory_order_release);
    m_writer_tag.store(0, std::memory_order_release);
    notify_waiters();
}

bool ipc_shared_mutex::refresh_exclusive_lease() noexcept
{
    const auto      current_tag = detail::current_process_identity().tag();
    const auto      current_tid = detail::current_thread_id_u64();
    ipc_mutex_guard guard(m_control_mutex);

    if (m_writer_tag.load(std::memory_order_acquire) != current_tag) {
        return false;
    }
    if (m_writer_thread_id.load(std::memory_order_acquire) != current_tid) {
        return false;
    }

    m_writer_deadline_ns.store(detail::monotonic_now_ns() + m_lease_timeout_ns, std::memory_order_release);
    return true;
}

std::size_t ipc_shared_mutex::cleanup_dead_holders() noexcept
{
    ipc_mutex_guard guard(m_control_mutex);
    const auto      cleaned = cleanup_dead_holders_unsafe();
    if (cleaned > 0) {
        notify_waiters();
    }
    return cleaned;
}

lock_acquire_state ipc_shared_mutex::do_try_lock_exclusive() noexcept
{
    const auto current_tag = detail::current_process_identity().tag();
    const auto current_tid = detail::current_thread_id_u64();
    const auto guard_state = m_control_mutex.try_lock_ex();
    if (guard_state == lock_acquire_state::failed) {
        return lock_acquire_state::failed;
    }
    const auto cleaned = cleanup_dead_holders_unsafe();
    (void)cleaned;

    const auto writer_tag = m_writer_tag.load(std::memory_order_acquire);
    const auto writer_tid = m_writer_thread_id.load(std::memory_order_acquire);
    if (writer_tag != 0 && !(writer_tag == current_tag && writer_tid == current_tid)) {
        m_control_mutex.unlock();
        return lock_acquire_state::failed;
    }

    if (has_blocking_reader_unsafe(current_tag, current_tid)) {
        m_control_mutex.unlock();
        return lock_acquire_state::failed;
    }

    m_writer_tag.store(current_tag, std::memory_order_release);
    m_writer_thread_id.store(current_tid, std::memory_order_release);
    m_writer_deadline_ns.store(detail::monotonic_now_ns() + m_lease_timeout_ns, std::memory_order_release);
    notify_waiters();
    m_control_mutex.unlock();
    return guard_state == lock_acquire_state::recovered ? lock_acquire_state::recovered : lock_acquire_state::acquired;
}

lock_acquire_state ipc_shared_mutex::do_try_lock_shared() noexcept
{
    const auto current_tag = detail::current_process_identity().tag();
    const auto current_tid = detail::current_thread_id_u64();
    const auto guard_state = m_control_mutex.try_lock_ex();
    if (guard_state == lock_acquire_state::failed) {
        return lock_acquire_state::failed;
    }
    const auto cleaned = cleanup_dead_holders_unsafe();
    (void)cleaned;

    if (writer_is_active_unsafe(current_tag, current_tid)) {
        m_control_mutex.unlock();
        return lock_acquire_state::failed;
    }

    auto slot = find_reader_slot_unsafe(current_tag, current_tid);
    if (slot == max_readers) {
        slot = find_free_reader_slot_unsafe();
        if (slot == max_readers) {
            m_control_mutex.unlock();
            return lock_acquire_state::failed;
        }
    }

    m_readers[slot].owner_tag.store(current_tag, std::memory_order_release);
    m_readers[slot].owner_thread_id.store(current_tid, std::memory_order_release);
    m_readers[slot].lease_deadline_ns.store(detail::monotonic_now_ns() + m_lease_timeout_ns,
                                            std::memory_order_release);
    notify_waiters();
    m_control_mutex.unlock();
    return guard_state == lock_acquire_state::recovered ? lock_acquire_state::recovered : lock_acquire_state::acquired;
}

std::size_t ipc_shared_mutex::cleanup_dead_holders_unsafe() noexcept
{
    std::size_t cleaned = 0;

    const auto writer_tag      = m_writer_tag.load(std::memory_order_acquire);
    const auto writer_deadline = m_writer_deadline_ns.load(std::memory_order_acquire);
    if (tag_is_stale(writer_tag, writer_deadline)) {
        m_writer_deadline_ns.store(0, std::memory_order_release);
        m_writer_thread_id.store(0, std::memory_order_release);
        m_writer_tag.store(0, std::memory_order_release);
        ++cleaned;
    }

    for (auto& reader : m_readers) {
        const auto tag      = reader.owner_tag.load(std::memory_order_acquire);
        const auto deadline = reader.lease_deadline_ns.load(std::memory_order_acquire);
        if (tag_is_stale(tag, deadline)) {
            reader.lease_deadline_ns.store(0, std::memory_order_release);
            reader.owner_thread_id.store(0, std::memory_order_release);
            reader.owner_tag.store(0, std::memory_order_release);
            ++cleaned;
        }
    }

    return cleaned;
}

bool ipc_shared_mutex::writer_is_active_unsafe(std::uint64_t current_tag,
                                               std::uint64_t current_thread_id) const noexcept
{
    const auto writer_tag = m_writer_tag.load(std::memory_order_acquire);
    if (writer_tag == 0) {
        return false;
    }
    // 同一线程持 writer 时，本线程再拿 shared 视为兼容（写锁可降级读）。
    const auto writer_tid = m_writer_thread_id.load(std::memory_order_acquire);
    if (writer_tag == current_tag && writer_tid == current_thread_id) {
        return false;
    }

    return !tag_is_stale(writer_tag, m_writer_deadline_ns.load(std::memory_order_acquire));
}

bool ipc_shared_mutex::tag_is_stale(std::uint64_t tag, std::uint64_t deadline_ns) const noexcept
{
    if (tag == 0) {
        return false;
    }

    const auto owner = detail::unpack_process_identity(tag);
    if (!ipc_mutex::is_process_alive(owner.pid)) {
        return true;
    }

    return deadline_ns != 0 && detail::monotonic_now_ns() >= deadline_ns;
}

std::size_t ipc_shared_mutex::find_reader_slot_unsafe(std::uint64_t current_tag,
                                                     std::uint64_t current_thread_id) const noexcept
{
    for (std::size_t i = 0; i < max_readers; ++i) {
        if (m_readers[i].owner_tag.load(std::memory_order_acquire) == current_tag &&
            m_readers[i].owner_thread_id.load(std::memory_order_acquire) == current_thread_id) {
            return i;
        }
    }
    return max_readers;
}

std::size_t ipc_shared_mutex::find_free_reader_slot_unsafe() const noexcept
{
    for (std::size_t i = 0; i < max_readers; ++i) {
        if (m_readers[i].owner_tag.load(std::memory_order_acquire) == 0) {
            return i;
        }
    }
    return max_readers;
}

bool ipc_shared_mutex::has_blocking_reader_unsafe(std::uint64_t current_tag,
                                                  std::uint64_t current_thread_id) const noexcept
{
    for (const auto& reader : m_readers) {
        const auto reader_tag = reader.owner_tag.load(std::memory_order_acquire);
        if (reader_tag == 0) {
            continue;
        }
        const auto reader_tid = reader.owner_thread_id.load(std::memory_order_acquire);
        if (reader_tag == current_tag && reader_tid == current_thread_id) {
            continue;
        }
        return true;
    }
    return false;
}

void ipc_shared_mutex::notify_waiters() noexcept
{
    m_wait_sequence.fetch_add(1, std::memory_order_acq_rel);
    detail::wait_backend::notify_all(m_wait_sequence);
}

} // namespace mc::shm
