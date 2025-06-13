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

#ifndef MC_SYNC_SMALL_MUTEX_H
#define MC_SYNC_SMALL_MUTEX_H

#include <mc/common.h>
#include <mc/sync/detail/wait_context.h>

#include <atomic>
#include <limits>

namespace mc::sync {

/**
 * @file small_mutex.h
 * @brief 仅占用一个32位整数空间的独占锁实现，目的是减少内存开销
 *
 * 使用 2 位存储锁状态，剩余 30 位可供用户存储数据。
 * 适用于对内存开销有一定约束但不极端的场景。
 *
 * 锁状态位布局: [ 用户数据 (30 bits) | 等待位 (1 bit) | 持有位 (1 bit) ]
 *
 * ## 主要特性
 *
 * 1. **低内存开销**: 仅使用一个32位整数存储锁状态和用户数据
 * 2. **高性能**: 使用三阶段等待策略（自旋 -> yield -> futex）
 * 3. **用户数据存储**: 剩余30位可存储用户数据
 * 4. **支持超时**: 支持带超时的锁获取操作
 * 5. **优化唤醒**: 使用 wait/wake mask 机制减少虚假唤醒
 *
 * ## 三阶段等待策略
 *
 * 1. **自旋阶段**: 使用 CPU pause 指令进行轻量级等待
 * 2. **Yield 阶段**: 主动让出 CPU 时间片
 * 3. **Futex 阶段**: 进入内核态等待，最小化 CPU 使用
 *
 * @tparam Policy 等待策略，定义自旋和 yield 的限制
 */
template <typename Policy>
class basic_small_mutex {
private:
    static constexpr uint32_t LOCK_BIT = 0x00000001; // 第 0 位：锁是否被持有
    static constexpr uint32_t WAIT_BIT = 0x00000002; // 第 1 位：是否有等待线程

    static constexpr uint32_t LOCK_MASK  = 0x00000003; // 锁使用的位掩码
    static constexpr uint32_t DATA_MASK  = 0xFFFFFFFC; // 数据使用的位掩码
    static constexpr unsigned DATA_SHIFT = 2;          // 数据位移位数

    // wait_mask 定义：只监视锁相关位的变化，减少虚假唤醒
    static constexpr uint32_t LOCK_WAIT_MASK = LOCK_BIT | WAIT_BIT;

    static constexpr uint32_t ALL = std::numeric_limits<int>::max();

    using wait_forever_type = detail::wait_forever<Policy::spin_limit, Policy::yield_limit>;
    template <typename Duration>
    using wait_for_duration_type = detail::wait_for_duration<Duration, Policy::spin_limit, Policy::yield_limit>;

public:
    basic_small_mutex() = default;

    basic_small_mutex(const basic_small_mutex& other)
        : m_lock_word{other.m_lock_word.load()} {
    }

    basic_small_mutex& operator=(const basic_small_mutex& other) {
        if (this != &other) {
            m_lock_word.store(other.m_lock_word.load());
        }
        return *this;
    }

    basic_small_mutex(basic_small_mutex&& other) noexcept
        : m_lock_word{other.m_lock_word.load()} {
        other.m_lock_word.store(0);
    }

    basic_small_mutex& operator=(basic_small_mutex&& other) noexcept {
        if (this != &other) {
            m_lock_word.store(other.m_lock_word.load());
            other.m_lock_word.store(0);
        }
        return *this;
    }

    /**
     * @brief 原子地加载存储在锁未使用位中的数据
     */
    uint32_t load_data(std::memory_order order = std::memory_order_seq_cst) const noexcept {
        return decode_data(m_lock_word.load(order));
    }

    /**
     * @brief 原子地存储数据到锁的未使用位中
     *
     * 由于 2 位被锁使用，提供的值的最低 2 位将被忽略。
     */
    void store_data(uint32_t value, std::memory_order order = std::memory_order_seq_cst) noexcept {
        uint32_t old_word = m_lock_word.load(std::memory_order_relaxed);
        while (true) {
            uint32_t new_word = (old_word & LOCK_MASK) | encode_data(value);
            if (m_lock_word.compare_exchange_weak(
                    old_word, new_word, order, std::memory_order_relaxed)) {
                break;
            }
        }
    }

    /**
     * @brief 获取锁
     */
    void lock() noexcept {
        uint32_t old_word;
        if (try_lock_impl(old_word)) {
            return;
        }

        // 不行再进入抢占流程
        wait_forever_type wait_ctx;
        acquire_lock(wait_ctx, old_word);
    }

    /**
     * @brief 尝试获取锁
     *
     * @return 如果成功获取锁返回 true，否则返回 false
     */
    bool try_lock() noexcept {
        uint32_t old_word;
        return try_lock_impl(old_word);
    }

    /**
     * @brief 带超时的锁获取
     *
     * @tparam Duration 超时时间类型
     * @param timeout 超时时间
     * @return 如果在超时前获取锁返回 true，否则返回 false
     */
    template <typename Duration>
    bool try_lock_for(const Duration& timeout) noexcept {
        uint32_t old_word;
        if (try_lock_impl(old_word)) {
            return true;
        }

        // 不行再进入抢占流程
        wait_for_duration_type<Duration> wait_ctx(timeout);
        return acquire_lock(wait_ctx, old_word);
    }

    /**
     * @brief 释放锁
     */
    void unlock() noexcept {
        uint32_t new_word;
        uint32_t old_word = m_lock_word.load(std::memory_order_relaxed);
        do {
            if (!(old_word & LOCK_BIT)) {
                break;
            }

            new_word = old_word & ~(LOCK_BIT | WAIT_BIT);
        } while (!m_lock_word.compare_exchange_weak(
            old_word, new_word, std::memory_order_release, std::memory_order_relaxed));

        // 如果有等待线程，唤醒它们
        if (old_word & WAIT_BIT) {
            detail::futex_wake(&m_lock_word, ALL, LOCK_WAIT_MASK);
        }
    }

    /**
     * @brief 检查锁当前是否被持有
     */
    bool is_locked() const noexcept {
        return (m_lock_word.load(std::memory_order_relaxed) & LOCK_BIT) != 0;
    }

    /**
     * @brief 获取底层的原子 word，用于高级操作
     *
     * 注意：直接操作返回的原子变量需要确保正确处理锁状态位
     */
    std::atomic<uint32_t>& get_atomic_word() noexcept {
        return m_lock_word;
    }

    const std::atomic<uint32_t>& get_atomic_word() const noexcept {
        return m_lock_word;
    }

private:
    /**
     * @brief 从 word 中解码用户数据
     */
    static constexpr uint32_t decode_data(uint32_t lock_word) noexcept {
        return lock_word >> DATA_SHIFT;
    }

    /**
     * @brief 将用户数据编码到 word 中
     */
    static constexpr uint32_t encode_data(uint32_t data) noexcept {
        return (data << DATA_SHIFT) & DATA_MASK;
    }

    bool try_lock_impl(uint32_t& old_word) noexcept {
        old_word = m_lock_word.load(std::memory_order_relaxed);
        if (old_word & LOCK_BIT) {
            return false;
        }

        return m_lock_word.compare_exchange_strong(
            old_word, old_word | LOCK_BIT,
            std::memory_order_acquire, std::memory_order_relaxed);
    }

    /**
     * @brief 抢占锁实现
     */
    template <typename Context>
    bool acquire_lock(Context& ctx, uint32_t old_word) noexcept {
        while (!ctx.should_timeout()) {
            // 设置等待位
            if ((old_word & WAIT_BIT) == 0 &&
                !m_lock_word.compare_exchange_weak(
                    old_word, old_word | WAIT_BIT,
                    std::memory_order_relaxed, std::memory_order_relaxed)) {
                continue;
            }

            // 锁被持有，等待
            if (old_word & LOCK_BIT) {
                ctx.wait(&m_lock_word, old_word, LOCK_WAIT_MASK);
                continue;
            }

            // 尝试获取锁
            if (m_lock_word.compare_exchange_weak(
                    old_word, old_word | LOCK_BIT,
                    std::memory_order_acquire, std::memory_order_relaxed)) {
                return true;
            }
        }

        return false;
    }

private:
    alignas(4) std::atomic<uint32_t> m_lock_word{0};
};

/**
 * @brief 标准小锁策略
 */
struct standard_small_mutex_policy {
    static constexpr uint32_t spin_limit  = MC_SYNC_DEFAULT_SPIN_LIMIT;
    static constexpr uint32_t yield_limit = MC_SYNC_DEFAULT_YIELD_LIMIT;
};

/**
 * @brief 自旋小锁策略（更多自旋，适用于低竞争场景）
 */
struct spin_small_mutex_policy {
    static constexpr uint32_t spin_limit  = 8000;
    static constexpr uint32_t yield_limit = 200;
};

using small_mutex = basic_small_mutex<standard_small_mutex_policy>;
using spin_mutex  = basic_small_mutex<spin_small_mutex_policy>;

} // namespace mc::sync

namespace mc {

// 导出到 mc 命名空间
using mc::sync::small_mutex;
using mc::sync::spin_mutex;

} // namespace mc

#endif // MC_SYNC_SMALL_MUTEX_H