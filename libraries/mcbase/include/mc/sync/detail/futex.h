/*
 * Copyright (c) 2024 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#ifndef MC_SYNC_DETAIL_FUTEX_H
#define MC_SYNC_DETAIL_FUTEX_H

#include <mc/common.h>

#include <atomic>
#include <chrono>
#include <thread>

#ifdef __linux__
#include <cerrno>
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>

#define FUTEX_WAIT_BITSET_PRIVATE (FUTEX_WAIT_BITSET | FUTEX_PRIVATE_FLAG)
#define FUTEX_WAKE_BITSET_PRIVATE (FUTEX_WAKE_BITSET | FUTEX_PRIVATE_FLAG)

#endif

#ifndef FUTEX_BITSET_MATCH_ANY
#define FUTEX_BITSET_MATCH_ANY 0xffffffff
#endif

namespace mc::sync::detail {

/**
 * @brief 跨平台 futex 封装
 *
 * 在 Linux 上使用真正的 futex 系统调用
 * 在其他平台上回退到基本的自旋等待
 */
class futex {
public:
    /**
     * @brief 等待原子变量值变化
     *
     * @param addr 原子变量地址
     * @param expected 期望的当前值
     * @param timeout 超时时间（可选）
     * @param wait_mask 等待掩码，只有 wait_mask 指定的位发生变化时才唤醒（默认 0xFFFFFFFF 表示监视所有位）
     * @return int 表示等待的结果
     */
    template <typename T>
    static int wait(const std::atomic<T>* addr, T& expected,
                    std::chrono::nanoseconds timeout   = std::chrono::nanoseconds::max(),
                    uint32_t                 wait_mask = FUTEX_BITSET_MATCH_ANY) noexcept
    {
#ifdef __linux__
        if constexpr (sizeof(T) == 4) {
            if (reinterpret_cast<uintptr_t>(addr) % 4 == 0) {
                return wait_linux(reinterpret_cast<const std::atomic<uint32_t>*>(addr),
                                  static_cast<uint32_t&>(expected), timeout, wait_mask);
            }
        }
#endif
        // 对于非 linux 平台、非 32 位类型以及未 4 字节对齐的地址，回退到基本等待策略
        MC_UNUSED(wait_mask);
        return wait_fallback(addr, expected, timeout);
    }

    /**
     * @brief 唤醒等待的线程
     *
     * @param addr 原子变量地址
     * @param count 要唤醒的线程数量
     * @param wake_mask 唤醒掩码，只唤醒使用匹配 wait_mask 的线程（默认 0xFFFFFFFF 表示唤醒所有线程）
     * @return 实际唤醒的线程数量
     */
    template <typename T>
    static int wake(const std::atomic<T>* addr, int count = 1, uint32_t wake_mask = FUTEX_BITSET_MATCH_ANY) noexcept
    {
        MC_UNUSED(addr);
        MC_UNUSED(wake_mask);
#ifdef __linux__
        if constexpr (sizeof(T) == 4) {
            if (reinterpret_cast<uintptr_t>(addr) % 4 == 0) { // 4 字节对齐
                return wake_linux(reinterpret_cast<const std::atomic<uint32_t>*>(addr), count, wake_mask);
            }
        }
#endif
        // 对于非 linux 平台、非 32 位类型以及未 4 字节对齐的地址，回退模式
        return count;
    }

private:
#ifdef __linux__
    static int wait_linux(const std::atomic<uint32_t>* addr, uint32_t& expected, std::chrono::nanoseconds timeout,
                          uint32_t wait_mask) noexcept;
    static int wake_linux(const std::atomic<uint32_t>* addr, int count, uint32_t wake_mask) noexcept;
#endif
    template <typename T>
    static int wait_fallback(const std::atomic<T>* addr, T& expected, std::chrono::nanoseconds timeout) noexcept;
};

#ifdef __linux__
inline int futex::wait_linux(const std::atomic<uint32_t>* addr, uint32_t& expected, std::chrono::nanoseconds timeout,
                             uint32_t wait_mask) noexcept
{
    struct timespec  ts;
    struct timespec* timeout_ptr = nullptr;

    if (timeout != std::chrono::nanoseconds::max()) {
        // 使用 CLOCK_MONOTONIC 时间，避免系统时间调整的影响
        auto now = std::chrono::steady_clock::now();
        auto end = now + timeout;

        auto end_sec  = std::chrono::duration_cast<std::chrono::seconds>(end.time_since_epoch());
        auto end_nsec = std::chrono::duration_cast<std::chrono::nanoseconds>(end.time_since_epoch() - end_sec);

        ts.tv_sec   = end_sec.count();
        ts.tv_nsec  = end_nsec.count();
        timeout_ptr = &ts;
    }

    // 使用 FUTEX_CLOCK_REALTIME 标志，让内核使用绝对时间
    const auto rv = syscall(SYS_futex, addr, FUTEX_WAIT_BITSET_PRIVATE | FUTEX_CLOCK_REALTIME, expected, timeout_ptr,
                            nullptr, wait_mask);

    auto current = addr->load(std::memory_order_acquire);
    expected     = current;

    return rv;
}

inline int futex::wake_linux(const std::atomic<uint32_t>* addr, int count, uint32_t wake_mask) noexcept
{
    return syscall(SYS_futex, addr, FUTEX_WAKE_BITSET_PRIVATE, count, nullptr, nullptr, wake_mask);
}

#endif

template <typename T>
int futex::wait_fallback(const std::atomic<T>* addr, T& expected, std::chrono::nanoseconds timeout) noexcept
{
    auto start_time = std::chrono::steady_clock::now();
    while (true) {
        auto current = addr->load(std::memory_order_acquire);
        if (current != expected) {
            expected = current;
            return 0;
        }

        if (timeout != std::chrono::nanoseconds::max()) {
            auto elapsed = std::chrono::steady_clock::now() - start_time;
            if (elapsed >= timeout) {
                expected = current;
                return -1;
            }
        }

        // futex 是 wait_op 的第三阶段，使用 sleep 来避免自旋
        // TODO:: 后续可以优化成用户态的条件等待机制
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

} // namespace mc::sync::detail

#endif // MC_SYNC_DETAIL_FUTEX_H