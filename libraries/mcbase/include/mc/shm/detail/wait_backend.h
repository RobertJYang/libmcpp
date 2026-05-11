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

#ifndef MC_SHM_DETAIL_WAIT_BACKEND_H
#define MC_SHM_DETAIL_WAIT_BACKEND_H

#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>

#ifdef __linux__
#include <cerrno>
#include <climits>
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

namespace mc::shm::detail {

class wait_backend {
public:
    static bool wait(std::atomic<std::uint32_t>& word, std::uint32_t expected,
                     std::chrono::nanoseconds timeout) noexcept
    {
#ifdef __linux__
        struct timespec ts {
            static_cast<time_t>(timeout.count() / 1000000000LL),
            static_cast<long>(timeout.count() % 1000000000LL),
        };

        const auto rc = syscall(SYS_futex, reinterpret_cast<std::uint32_t*>(&word), FUTEX_WAIT_BITSET,
                                expected, &ts, nullptr, FUTEX_BITSET_MATCH_ANY);
        if (rc == 0) {
            return true;
        }

        if (errno == EAGAIN) {
            return true;
        }

        return errno != ETIMEDOUT;
#else
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        std::size_t spin_count = 0;
        while (word.load(std::memory_order_acquire) == expected) {
            if (std::chrono::steady_clock::now() >= deadline) {
                return false;
            }

            if (spin_count < 32) {
                ++spin_count;
                std::this_thread::yield();
            } else {
                std::this_thread::sleep_for(std::chrono::microseconds(200));
            }
        }
        return true;
#endif
    }

    static void notify_one(std::atomic<std::uint32_t>& word) noexcept
    {
#ifdef __linux__
        syscall(SYS_futex, reinterpret_cast<std::uint32_t*>(&word), FUTEX_WAKE, 1, nullptr, nullptr, 0);
#else
        (void)word;
#endif
    }

    static void notify_all(std::atomic<std::uint32_t>& word) noexcept
    {
#ifdef __linux__
        syscall(SYS_futex, reinterpret_cast<std::uint32_t*>(&word), FUTEX_WAKE, INT_MAX, nullptr, nullptr, 0);
#else
        (void)word;
#endif
    }
};

} // namespace mc::shm::detail

#endif // MC_SHM_DETAIL_WAIT_BACKEND_H
