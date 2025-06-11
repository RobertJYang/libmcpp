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
#ifndef MC_SYNC_SHARED_MUTEX_H
#define MC_SYNC_SHARED_MUTEX_H

#include <atomic>
#include <mc/sync/detail/wait_context.h>
#include <thread>

namespace mc::sync {

/**
 * @brief 写优先策略
 *
 * write_limit = 0 表示写优先：写线程立即设置等待位阻止新的读锁获取。
 */
struct writer_priority_policy {
    static constexpr uint32_t spin_limit  = MC_SYNC_DEFAULT_SPIN_LIMIT;
    static constexpr uint32_t write_limit = 0; // 写优先：立即阻止读线程
};

/**
 * @brief 读优先策略
 *
 * write_limit > 0 表示读优先：写线程在指定次数的尝试后才设置等待位阻止新的读锁。
 * - 较小的值：更快阻止读线程，降低写线程饥饿风险，但影响读并发性
 * - 较大的值：更好的读并发性，但写线程饥饿风险更高
 */
struct reader_priority_policy {
    static constexpr uint32_t spin_limit  = MC_SYNC_DEFAULT_SPIN_LIMIT;
    static constexpr uint32_t write_limit = 100; // 读优先：100次尝试后阻止读线程
};

/**
 * @brief 一个自旋读写互斥锁，目的是减少内存开销，仅占用一个32位整数空间
 */
template <typename Policy = writer_priority_policy>
class basic_shared_mutex {
    // 状态位布局: [ 写标志 (1 bit) | 写等待标志 (1 bit) | 升级标志 (1 bit) | 读计数 (29 bits) ]
    static constexpr uint32_t WRITER_BIT         = 1u << 31;
    static constexpr uint32_t WRITER_WAITING_BIT = 1u << 30;
    static constexpr uint32_t UPGRADE_BIT        = 1u << 29;

    static constexpr uint32_t READER_MASK  = ~(WRITER_BIT | WRITER_WAITING_BIT | UPGRADE_BIT);
    static constexpr uint32_t UPGRADE_MASK = WRITER_BIT | UPGRADE_BIT | WRITER_WAITING_BIT;

public:
    basic_shared_mutex() noexcept : m_state(0) {
    }
    ~basic_shared_mutex() = default;

    basic_shared_mutex(const basic_shared_mutex&)            = delete;
    basic_shared_mutex& operator=(const basic_shared_mutex&) = delete;

    // --- 写锁 ---

    void lock() noexcept {
        if (try_lock()) {
            return;
        }

        detail::wait_forever ctx;
        acquire_writer_lock_impl<false>(ctx);
    }

    bool try_lock() noexcept {
        uint32_t expected = 0;
        if (m_state.compare_exchange_strong(
                expected, WRITER_BIT,
                std::memory_order_acquire, std::memory_order_relaxed)) {
            return true;
        }

        return false;
    }

    template <typename Duration>
    bool try_lock_for(const Duration& timeout) noexcept {
        if (try_lock()) {
            return true;
        }

        detail::wait_for_duration<Duration> ctx(timeout);
        return acquire_writer_lock_impl<false>(ctx);
    }

    void unlock() noexcept {
        // 解锁操作只需原子地移除写位，写等待位由正在等待的写线程处理
        m_state.fetch_and(~WRITER_BIT, std::memory_order_release);
    }

    // --- 读锁 ---

    void lock_shared() noexcept {
        detail::wait_forever ctx;
        lock_shared_impl(ctx);
    }

    bool try_lock_shared() noexcept {
        uint32_t current = m_state.load(std::memory_order_relaxed);
        if ((current & (WRITER_BIT | WRITER_WAITING_BIT)) != 0) {
            return false;
        }

        uint32_t expected = current;
        return m_state.compare_exchange_strong(
            expected, current + 1,
            std::memory_order_acquire, std::memory_order_relaxed);
    }

    template <typename Duration>
    bool try_lock_shared_for(const Duration& timeout) noexcept {
        detail::wait_for_duration<Duration> ctx(timeout);
        return lock_shared_impl(ctx);
    }

    void unlock_shared() noexcept {
        m_state.fetch_sub(1, std::memory_order_release);
    }

    // --- 升级锁 ---

    void lock_upgrade() noexcept {
        detail::wait_forever ctx;
        lock_upgrade_impl(ctx);
    }

    bool try_lock_upgrade() noexcept {
        uint32_t current = m_state.load(std::memory_order_acquire);
        if (current & UPGRADE_MASK) {
            return false;
        }

        return m_state.compare_exchange_strong(
            current, current | UPGRADE_BIT,
            std::memory_order_acq_rel, std::memory_order_relaxed);
    }

    template <typename Duration>
    bool try_lock_upgrade_for(const Duration& timeout) noexcept {
        detail::wait_for_duration<Duration> ctx(timeout);
        return lock_upgrade_impl(ctx);
    }

    void unlock_upgrade() noexcept {
        m_state.fetch_and(~UPGRADE_BIT, std::memory_order_release);
    }

    // --- 升级锁升级操作 ---

    void unlock_upgrade_and_lock() noexcept {
        if (try_unlock_upgrade_and_lock()) {
            return;
        }

        detail::wait_forever ctx;
        acquire_writer_lock_impl<true>(ctx);
    }

    bool try_unlock_upgrade_and_lock() noexcept {
        // 判断是否持有升级锁，不是则直接失败
        uint32_t current = m_state.load(std::memory_order_acquire);
        if (!(current & UPGRADE_BIT)) {
            return false;
        }

        // 判断是否有其他互斥的操作，如果没有尝试立即升级
        if ((current & (WRITER_BIT | WRITER_WAITING_BIT | READER_MASK)) == 0) {
            uint32_t new_state = (current & ~UPGRADE_BIT) | WRITER_BIT;
            if (m_state.compare_exchange_strong(
                    current, new_state,
                    std::memory_order_acq_rel, std::memory_order_relaxed)) {
                return true;
            }
        }

        return false;
    }

    template <typename Duration>
    bool try_unlock_upgrade_and_lock_for(const Duration& timeout) noexcept {
        if (try_unlock_upgrade_and_lock()) {
            return true;
        }

        detail::wait_for_duration<Duration> ctx(timeout);
        return acquire_writer_lock_impl<true>(ctx);
    }

    // --- 锁降级操作 ---

    // 从升级锁降级到读锁
    void unlock_upgrade_and_lock_shared() noexcept {
        uint32_t expected = m_state.load(std::memory_order_acquire);
        while (expected & UPGRADE_BIT) {
            uint32_t new_state = (expected & ~UPGRADE_BIT) + 1;
            if (m_state.compare_exchange_weak(
                    expected, new_state,
                    std::memory_order_acq_rel, std::memory_order_relaxed)) {
                return;
            }
        }
    }

    // 从写锁降级到升级锁
    void unlock_and_lock_upgrade() noexcept {
        uint32_t expected = m_state.load(std::memory_order_acquire);
        while (expected & WRITER_BIT) {
            uint32_t new_state = (expected & ~WRITER_BIT) | UPGRADE_BIT;
            if (m_state.compare_exchange_weak(
                    expected, new_state,
                    std::memory_order_acq_rel, std::memory_order_relaxed)) {
                return;
            }
        }
    }

    // 从写锁降级到读锁
    void unlock_and_lock_shared() noexcept {
        uint32_t expected = m_state.load(std::memory_order_acquire);
        while (expected & WRITER_BIT) {
            uint32_t new_state = (expected & ~WRITER_BIT) + 1;
            if (m_state.compare_exchange_weak(
                    expected, new_state,
                    std::memory_order_acq_rel, std::memory_order_relaxed)) {
                return;
            }
        }
    }

private:
    // 写锁抢占逻辑
    template <bool IsUpgradeLock, typename Context>
    bool acquire_writer_lock_impl(Context& ctx) noexcept {
        constexpr uint32_t wait_mask  = IsUpgradeLock ? WRITER_BIT : (WRITER_BIT | UPGRADE_BIT);
        constexpr uint32_t clear_mask = IsUpgradeLock ? (UPGRADE_BIT | WRITER_WAITING_BIT) : WRITER_WAITING_BIT;
        constexpr uint32_t threshold  = Policy::write_limit;

        uint32_t attempt_count = 0;
        while (!ctx.should_timeout()) {
            uint32_t current = m_state.load(std::memory_order_acquire);

            // 如果是升级锁，检查升级锁是否还存在
            if constexpr (IsUpgradeLock) {
                if (!(current & UPGRADE_BIT)) {
                    break; // 升级锁已丢失，退出循环
                }
            }

            // 检查需要等待的条件
            if (current & wait_mask) {
                ctx.spin();
                continue;
            }

            // 设置写等待位
            if (!(current & WRITER_WAITING_BIT) &&
                (attempt_count >= threshold) &&
                !m_state.compare_exchange_weak(
                    current, current | WRITER_WAITING_BIT,
                    std::memory_order_relaxed, std::memory_order_relaxed)) {
                ctx.spin();
                continue;
            }

            // 每次循环都应该计数，确保能达到阈值
            if constexpr (Policy::write_limit != 0) {
                attempt_count++;
            }

            // 如果还有读锁，继续等待
            if (current & READER_MASK) {
                ctx.spin();
                continue;
            }

            // 清除指定位，设置写位
            uint32_t new_state = (current & ~clear_mask) | WRITER_BIT;
            if (m_state.compare_exchange_weak(
                    current, new_state,
                    std::memory_order_acquire, std::memory_order_relaxed)) {
                return true; // 成功获取写锁
            }

            ctx.spin();
        }

        // 超时失败，清理WRITER_WAITING_BIT避免读线程被永久阻塞
        m_state.fetch_and(~WRITER_WAITING_BIT, std::memory_order_relaxed);
        return false;
    }

    template <typename Context>
    bool lock_shared_impl(Context& ctx) noexcept {
        while (!ctx.should_timeout()) {
            uint32_t current = m_state.load(std::memory_order_acquire);
            if (current & (WRITER_BIT | WRITER_WAITING_BIT)) {
                ctx.spin();
                continue;
            }

            if (m_state.compare_exchange_weak(
                    current, current + 1,
                    std::memory_order_acq_rel, std::memory_order_relaxed)) {
                return true;
            }
        }

        return false;
    }

    template <typename Context>
    bool lock_upgrade_impl(Context& ctx) noexcept {
        while (!ctx.should_timeout()) {
            uint32_t current = m_state.load(std::memory_order_acquire);
            if (current & UPGRADE_MASK) {
                ctx.spin();
                continue;
            }

            if (m_state.compare_exchange_weak(
                    current, current | UPGRADE_BIT,
                    std::memory_order_acq_rel, std::memory_order_relaxed)) {
                return true;
            }
        }

        return false;
    }

    std::atomic<uint32_t> m_state;
};

// 默认的 shared_mutex 是写优先的
using shared_mutex = basic_shared_mutex<writer_priority_policy>;

// 读优先的 shared_mutex
using reader_priority_shared_mutex = basic_shared_mutex<reader_priority_policy>;

} // namespace mc::sync

namespace mc {

// 导出到 mc 命名空间
using sync::reader_priority_shared_mutex;
using sync::shared_mutex;

} // namespace mc

#endif // MC_SYNC_SHARED_MUTEX_H