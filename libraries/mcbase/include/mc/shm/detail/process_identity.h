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

#ifndef MC_SHM_DETAIL_PROCESS_IDENTITY_H
#define MC_SHM_DETAIL_PROCESS_IDENTITY_H

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <thread>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

namespace mc::shm::detail {

struct process_identity {
    std::uint32_t pid        = 0;
    std::uint32_t generation = 0;

    constexpr std::uint64_t tag() const noexcept
    {
        return (static_cast<std::uint64_t>(generation) << 32) | static_cast<std::uint64_t>(pid);
    }
};

inline std::uint32_t current_process_pid() noexcept
{
#ifdef _WIN32
    return static_cast<std::uint32_t>(_getpid());
#else
    return static_cast<std::uint32_t>(::getpid());
#endif
}

inline std::uint32_t current_process_generation() noexcept
{
    static const std::uint32_t generation = []() noexcept {
        const auto now = static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
        return static_cast<std::uint32_t>((now >> 4) ^ (now >> 32) ^ current_process_pid());
    }();

    return generation == 0 ? 1u : generation;
}

inline process_identity current_process_identity() noexcept
{
    return process_identity{current_process_pid(), current_process_generation()};
}

inline process_identity unpack_process_identity(std::uint64_t tag) noexcept
{
    return process_identity{static_cast<std::uint32_t>(tag & 0xffffffffULL), static_cast<std::uint32_t>(tag >> 32)};
}

// 返回本线程的 64 位身份标识，保证同进程内线程间唯一。
// 跨进程之间可能冲突，因此该值仅用于区分同一进程内的持锁线程，
// 必须配合 process_identity::tag() 使用。
inline std::uint64_t current_thread_id_u64() noexcept
{
    static std::atomic<std::uint64_t> next_id{1};
    thread_local const std::uint64_t  id = next_id.fetch_add(1, std::memory_order_relaxed);
    return id;
}

} // namespace mc::shm::detail

#endif // MC_SHM_DETAIL_PROCESS_IDENTITY_H
