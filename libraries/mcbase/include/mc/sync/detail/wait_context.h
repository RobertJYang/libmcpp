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
#ifndef MC_SYNC_WAIT_CONTEXT_H
#define MC_SYNC_WAIT_CONTEXT_H

#include <mc/sync/detail/futex.h>
#include <mc/sync/detail/wait_op.h>

#include <atomic>
#include <chrono>
#include <thread>

namespace mc::sync::detail {

/**
 * @brief 永久等待上下文
 * 提供无超时限制的等待策略
 */
template <uint32_t SpinLimit, uint32_t YieldLimit>
class wait_forever {
public:
    wait_forever() noexcept = default;

    /**
     * @brief 在原子变量上执行等待操作
     */
    template <typename T>
    void wait(const std::atomic<T>* addr, T& expected) noexcept
    {
        m_wait_op.wait(addr, expected);
    }

    /**
     * @brief 在原子变量上执行等待操作（带 wait_mask）
     */
    template <typename T>
    void wait(const std::atomic<T>* addr, T& expected, uint32_t wait_mask) noexcept
    {
        m_wait_op.wait(addr, expected, std::chrono::nanoseconds::max(), wait_mask);
    }

    /**
     * @brief 执行轻量自旋操作，不进入 futex 等待
     */
    void spin() noexcept
    {
        m_wait_op.spin();
    }

    /**
     * @brief 检查是否应该超时（永远不超时）
     */
    constexpr bool should_timeout() const noexcept
    {
        return false;
    }

private:
    wait_op<SpinLimit, YieldLimit> m_wait_op;
};

/**
 * @brief 带超时的等待上下文
 */
template <typename Duration, uint32_t SpinLimit, uint32_t YieldLimit>
class wait_for_duration {
public:
    explicit wait_for_duration(const Duration& timeout) noexcept
        : m_timeout(timeout), m_start_time(std::chrono::steady_clock::now())
    {}

    /**
     * @brief 在原子变量上执行等待操作
     */
    template <typename T>
    void wait(const std::atomic<T>* addr, T& expected) noexcept
    {
        if (should_timeout()) {
            return;
        }

        if (!m_wait_op.is_in_futex_phase()) {
            m_wait_op.wait(addr, expected);
        } else {
            auto elapsed   = std::chrono::steady_clock::now() - m_start_time;
            auto remaining = m_timeout - elapsed;

            if (remaining <= Duration::zero()) {
                return; // 已经超时
            }

            auto remaining_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                static_cast<std::chrono::steady_clock::duration>(remaining));
            m_wait_op.wait(addr, expected, remaining_ns);
        }
    }

    /**
     * @brief 在原子变量上执行等待操作（带 wait_mask）
     */
    template <typename T>
    void wait(const std::atomic<T>* addr, T& expected, uint32_t wait_mask) noexcept
    {
        if (should_timeout()) {
            return;
        }

        if (!m_wait_op.is_in_futex_phase()) {
            m_wait_op.wait(addr, expected);
        } else {
            auto elapsed   = std::chrono::steady_clock::now() - m_start_time;
            auto remaining = m_timeout - elapsed;

            if (remaining <= Duration::zero()) {
                return; // 已经超时
            }

            auto remaining_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                static_cast<std::chrono::steady_clock::duration>(remaining));
            m_wait_op.wait(addr, expected, remaining_ns, wait_mask);
        }
    }

    /**
     * @brief 执行轻量自旋操作，不进入 futex 等待
     */
    void spin() noexcept
    {
        m_wait_op.spin();
    }

    /**
     * @brief 检查是否应该超时
     */
    bool should_timeout() const noexcept
    {
        auto elapsed = std::chrono::steady_clock::now() - m_start_time;
        return elapsed >= m_timeout;
    }

private:
    Duration                              m_timeout;
    std::chrono::steady_clock::time_point m_start_time;
    wait_op<SpinLimit, YieldLimit>        m_wait_op;
};

/**
 * @brief futex 唤醒辅助函数
 */
template <typename T>
void futex_wake(const std::atomic<T>* addr, int count = 1) noexcept
{
    futex::wake(addr, count);
}

/**
 * @brief futex 唤醒辅助函数（带 wake_mask）
 */
template <typename T>
void futex_wake(const std::atomic<T>* addr, int count, uint32_t wake_mask) noexcept
{
    futex::wake(addr, count, wake_mask);
}

} // namespace mc::sync::detail

#endif // MC_SYNC_WAIT_CONTEXT_H