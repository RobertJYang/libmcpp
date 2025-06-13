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
#ifndef MC_SYNC_WAIT_OP_H
#define MC_SYNC_WAIT_OP_H

#include <mc/sync/detail/futex.h>

#include <chrono>
#include <thread>

namespace mc::sync::detail {

/**
 * @brief 在自旋循环中暂停 CPU，以降低功耗并提高性能
 */
inline void cpu_pause() noexcept {
#if defined(__i386__) || defined(__x86_64__)
    __asm__ __volatile__("pause"); // x86/x86_64 上使用 'pause' 指令
#elif defined(__aarch64__)
    __asm__ __volatile__("yield"); // aarch64 上使用 'yield' 指令
#else
    // 对于其他体系结构，这是一个空操作。
#endif
}

#ifndef MC_SYNC_DEFAULT_SPIN_LIMIT
#define MC_SYNC_DEFAULT_SPIN_LIMIT 1000
#endif

#ifndef MC_SYNC_DEFAULT_YIELD_LIMIT
#define MC_SYNC_DEFAULT_YIELD_LIMIT 100
#endif

/**
 * @brief 实现三阶段混合等待策略：自旋 -> yield -> futex
 *
 * 1. 自旋阶段使用简单的 CPU pause
 * 2. Yield 阶段减少次数，添加抢占检测
 * 3. 更快进入 futex 等待以减少 CPU 浪费
 *
 * @tparam SpinLimit 自旋的最大次数，超过后将切换到线程 yield
 * @tparam YieldLimit yield 的最大次数，超过后将切换到 futex 等待
 */
template <uint32_t SpinLimit  = MC_SYNC_DEFAULT_SPIN_LIMIT,
          uint32_t YieldLimit = MC_SYNC_DEFAULT_YIELD_LIMIT>
class wait_op {
    static constexpr uint32_t spin_threshold  = SpinLimit;
    static constexpr uint32_t yield_threshold = SpinLimit + YieldLimit;

public:
    wait_op() noexcept = default;

    /**
     * @brief 在原子变量上执行等待操作
     * 根据当前状态选择最合适的等待策略
     *
     * @param addr 要等待的原子变量地址
     * @param expected 期望的当前值
     * @param timeout 超时时间（可选）
     * @param wait_mask 等待掩码（仅在 futex 阶段生效）
     */
    template <typename T>
    void wait(const std::atomic<T>* addr, T& expected,
              std::chrono::nanoseconds timeout   = std::chrono::nanoseconds::max(),
              uint32_t                 wait_mask = FUTEX_BITSET_MATCH_ANY) noexcept {
        if (m_wait_count <= spin_threshold) {
            cpu_pause();
            expected = addr->load(std::memory_order_relaxed);
        } else if (m_wait_count <= yield_threshold) {
            std::this_thread::yield();
            expected = addr->load(std::memory_order_relaxed);
        } else {
            futex::wait(addr, expected, timeout, wait_mask);
        }
        m_wait_count++;
    }

    /**
     * @brief 执行轻量自旋操作，不进入 futex 等待
     */
    void spin() noexcept {
        if (m_wait_count <= spin_threshold) {
            cpu_pause();
        } else {
            std::this_thread::yield();
        }
        m_wait_count++;
    }

    /**
     * @brief 获取当前的等待次数
     */
    uint32_t get_wait_count() const noexcept {
        return m_wait_count;
    }

    /**
     * @brief 重置等待状态
     */
    void reset() noexcept {
        m_wait_count = 0;
    }

    /**
     * @brief 检查是否已进入或即将进入 futex 等待阶段
     */
    bool is_in_futex_phase() const noexcept {
        return m_wait_count > yield_threshold;
    }

private:
    uint32_t m_wait_count{0};
};

} // namespace mc::sync::detail

#endif // MC_SYNC_WAIT_OP_H