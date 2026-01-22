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
#include <limits>
#include <mc/sync/detail/wait_context.h>
#include <thread>

/**
 * @file shared_mutex.h
 * @brief 仅占用一个32位整数空间的读写锁实现，目的是减少内存开销
 *
 * shared_mutex 是一个高性能的读写互斥锁实现，它使用自旋等待和 futex 系统调用
 * 来最小化锁竞争时的系统开销。该实现仅使用一个32位整数来存储所有状态信息，
 * 大大减少了内存占用。
 *
 * ## 主要特性
 *
 * 1. **低内存开销**: 仅使用一个32位整数存储所有状态
 * 2. **高性能**: 使用自旋等待和 futex 系统调用优化性能
 * 3. **多种锁类型**: 支持读锁、写锁和升级锁
 * 4. **锁升级**: 支持从升级锁升级到写锁
 * 5. **锁降级**: 支持从写锁降级到升级锁或读锁，升级锁降级为读锁
 * 6. **写优先**: 默认实现写优先策略，防止写线程饥饿
 * 7. **读优先**: 可选实现读优先策略，提高读并发性
 *
 * ## 状态位布局
 *
 * 32位状态字的布局如下：
 * ```
 * [ 写标志 (1 bit) | 写等待标志 (1 bit) | 升级标志 (1 bit) | 读计数 (29 bits) ]
 * ```
 *
 * - 写标志 (WRITER_BIT): 表示是否有写锁
 * - 写等待标志 (WRITER_WAITING_BIT): 表示是否有写线程在等待获取写锁
 * - 升级标志 (UPGRADE_BIT): 表示是否有升级锁
 * - 读计数 (READER_MASK): 表示当前读锁的数量
 *
 * ## 锁的互斥关系
 *
 * 锁之间的互斥关系如下：
 *
 * 1. **写锁（独占锁）**
 *    - 互斥：写锁、读锁、升级锁
 *    - 兼容：写等待位
 *    - 说明：写锁是完全独占的，不能与任何其他锁共存
 *
 * 2. **读锁（共享锁）**
 *    - 互斥：写锁、写等待位
 *    - 兼容：读锁、升级锁
 *    - 说明：多个读锁可以共存，也可以与升级锁共存，但不能与写锁和写等待位共存
 *
 * 3. **升级锁**
 *    - 互斥：写锁、升级锁、写等待位
 *    - 兼容：读锁
 *    - 说明：升级锁是独占的（只能有一个），可以与读锁共存，但不能与写锁、其他升级锁或写等待位共存
 *
 * 4. **写等待位**
 *    - 互斥：读锁、升级锁
 *    - 兼容：写锁
 *    - 说明：写等待位用于阻止新的读锁和升级锁获取，只能与写锁共存
 *
 * **兼容性矩阵**：
 * ```
 * |           | 写锁 | 读锁 | 升级锁 | 写等待位 |
 * |-----------|------|------|--------|----------|
 * | 写锁      |  ❌  |  ❌  |   ❌   |    ✅    |
 * | 读锁      |  ❌  |  ✅  |   ✅   |    ❌    |
 * | 升级锁    |  ❌  |  ✅  |   ❌   |    ❌    |
 * | 写等待位  |  ✅  |  ❌  |   ❌   |    ✅    |
 * ```
 *
 * 说明：
 * - ✅ 表示兼容（可以同时存在）
 * - ❌ 表示互斥（不能同时存在）
 * - **互斥**：不能同时获取，必须先释放一个才能获取另一个
 * - **兼容**：可以同时获取，不需要等待另一个释放
 *
 * ## 锁的优先级和语义
 *
 * 1. **升级锁的独占性**：
 *    - 升级锁遵循标准的升级锁语义，同时只能有一个线程持有升级锁
 *    - 这确保了升级操作的原子性和一致性
 *
 * 2. **写等待位的作用**：
 *    - 防止写线程饥饿：当写线程等待时，阻止新的读锁和升级锁获取
 *    - 确保写线程能够及时获得执行机会
 *    - 升级锁与写等待位互斥的设计避免了复杂的优先级冲突
 *
 * 3. **锁的层次结构**：
 *    - 写锁 > 升级锁 > 读锁（按独占性排序）
 *    - 高级别的锁与低级别的锁互斥，同级别的锁之间的关系取决于具体类型
 *
 * ## 基本用法
 *
 * ```cpp
 * mc::shared_mutex mutex;
 *
 * // 写锁操作
 * {
 *     std::lock_guard<mc::shared_mutex> lock(mutex);
 *     // 独占访问
 * }
 *
 * // 读锁操作
 * {
 *     std::shared_lock<mc::shared_mutex> lock(mutex);
 *     // 共享访问
 * }
 *
 * // 升级锁操作
 * {
 *     std::unique_lock<mc::shared_mutex> lock(mutex, std::defer_lock);
 *     mutex.lock_upgrade();
 *     // 共享访问
 *     mutex.unlock_upgrade_and_lock();  // 升级到写锁
 *     // 独占访问
 * }
 * ```
 * ## 写等待位机制
 *
 * 写等待位用于防止写线程饥饿：
 * - 当写线程等待次数超过阈值时设置
 * - 设置后阻止新的读锁获取
 * - 写线程获取锁后自动清除
 * - 写线程超时退出时自动清除
 *
 * ## 锁的降级操作
 *
 * 1. **写锁降级到升级锁**
 *    - 需要清除写位，设置升级位
 *    - 保持读计数不变
 *
 * 2. **写锁降级到读锁**
 *    - 需要清除写位
 *    - 读计数加1
 *
 * 3. **升级锁降级到读锁**
 *    - 需要清除升级位
 *    - 读计数加1
 */

namespace mc::sync {

/**
 * @brief 一个自旋读写互斥锁，目的是减少内存开销，仅占用一个32位整数空间
 */
template <typename Policy>
class basic_shared_mutex {
    // 状态位布局: [ 写标志 (1 bit) | 写等待标志 (1 bit) | 升级标志 (1 bit) | 读计数 (29 bits) ]
    static constexpr uint32_t WRITER_BIT         = 1u << 31;
    static constexpr uint32_t WRITER_WAITING_BIT = 1u << 30;
    static constexpr uint32_t UPGRADE_BIT        = 1u << 29;

    static constexpr uint32_t READER_MASK  = ~(WRITER_BIT | WRITER_WAITING_BIT | UPGRADE_BIT);
    static constexpr uint32_t UPGRADE_MASK = WRITER_BIT | UPGRADE_BIT | WRITER_WAITING_BIT;

    // wait_mask 定义：只监视特定位的变化，减少虚假唤醒
    static constexpr uint32_t SHARED_WAIT_MASK  = WRITER_BIT | WRITER_WAITING_BIT;               // 共享锁等待：关心写位和写等待位
    static constexpr uint32_t UPGRADE_WAIT_MASK = WRITER_BIT | UPGRADE_BIT | WRITER_WAITING_BIT; // 升级锁等待：关心写位、升级位和写等待位
    static constexpr uint32_t WRITER_WAIT_MASK  = WRITER_BIT | READER_MASK | UPGRADE_BIT;        // 写锁等待：关心写位、读计数和升级位

    static constexpr uint32_t ALL = std::numeric_limits<int>::max();

    using wait_forever_type = detail::wait_forever<Policy::spin_limit, Policy::yield_limit>;
    template <typename Duration>
    using wait_for_duration_type = detail::wait_for_duration<Duration, Policy::spin_limit, Policy::yield_limit>;

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

        wait_forever_type ctx;
        acquire_writer_lock<false>(ctx);
    }

    bool try_lock() noexcept {
        uint32_t expected = 0;
        if (m_state.compare_exchange_strong(
                expected, WRITER_BIT,
                std::memory_order_acq_rel, std::memory_order_acquire)) {
            return true;
        }

        return false;
    }

    template <typename Duration>
    bool try_lock_for(const Duration& timeout) noexcept {
        if (try_lock()) {
            return true;
        }

        wait_for_duration_type<Duration> ctx(timeout);
        return acquire_writer_lock<false>(ctx);
    }

    void unlock() noexcept {
        m_state.fetch_and(~WRITER_BIT, std::memory_order_release);
        detail::futex_wake(&m_state, ALL, WRITER_BIT);
    }

    // --- 读锁 ---

    void lock_shared() noexcept {
        wait_forever_type ctx;
        acquire_shared_lock(ctx);
    }

    bool try_lock_shared() noexcept {
        uint32_t current = m_state.load(std::memory_order_acquire);
        if ((current & (WRITER_BIT | WRITER_WAITING_BIT)) != 0) {
            return false;
        }

        uint32_t expected = current;
        return m_state.compare_exchange_strong(
            expected, current + 1,
            std::memory_order_acq_rel, std::memory_order_acquire);
    }

    template <typename Duration>
    bool try_lock_shared_for(const Duration& timeout) noexcept {
        wait_for_duration_type<Duration> ctx(timeout);
        return acquire_shared_lock(ctx);
    }

    void unlock_shared() noexcept {
        uint32_t old_state = m_state.fetch_sub(1, std::memory_order_release);
        if ((old_state & READER_MASK) == 1) {
            detail::futex_wake(&m_state, ALL, WRITER_BIT);
        }
    }

    // --- 升级锁 ---

    void lock_upgrade() noexcept {
        wait_forever_type ctx;
        acquire_upgrade_lock(ctx);
    }

    bool try_lock_upgrade() noexcept {
        uint32_t current = m_state.load(std::memory_order_acquire);
        if (current & UPGRADE_MASK) {
            return false;
        }

        return m_state.compare_exchange_strong(
            current, current | UPGRADE_BIT,
            std::memory_order_acq_rel, std::memory_order_acquire);
    }

    template <typename Duration>
    bool try_lock_upgrade_for(const Duration& timeout) noexcept {
        wait_for_duration_type<Duration> ctx(timeout);
        return acquire_upgrade_lock(ctx);
    }

    void unlock_upgrade() noexcept {
        m_state.fetch_and(~UPGRADE_BIT, std::memory_order_release);
        detail::futex_wake(&m_state, ALL, UPGRADE_BIT);
    }

    // --- 升级锁升级操作 ---

    void unlock_upgrade_and_lock() noexcept {
        if (try_unlock_upgrade_and_lock()) {
            return;
        }

        wait_forever_type ctx;
        acquire_writer_lock<true>(ctx);
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
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                // 升级成功，唤醒其他等待升级锁的线程
                detail::futex_wake(&m_state, ALL, UPGRADE_BIT);
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

        wait_for_duration_type<Duration> ctx(timeout);
        return acquire_writer_lock<true>(ctx);
    }

    // --- 锁降级操作 ---

    // 从升级锁降级到读锁
    void unlock_upgrade_and_lock_shared() noexcept {
        uint32_t expected = m_state.load(std::memory_order_acquire);

        // 必须持有升级锁才能降级
        if (!(expected & UPGRADE_BIT)) {
            return; // 没有升级锁，直接返回
        }

        while (expected & UPGRADE_BIT) {
            uint32_t new_state = (expected & ~UPGRADE_BIT) + 1;
            if (m_state.compare_exchange_weak(
                    expected, new_state,
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                // 降级成功，唤醒等待升级锁的线程
                detail::futex_wake(&m_state, ALL, UPGRADE_BIT);
                return;
            }
        }
    }

    // 从写锁降级到升级锁
    void unlock_and_lock_upgrade() noexcept {
        uint32_t expected = m_state.load(std::memory_order_acquire);

        // 必须持有写锁才能降级
        if (!(expected & WRITER_BIT)) {
            return; // 没有写锁，直接返回
        }

        while (expected & WRITER_BIT) {
            uint32_t new_state = (expected & ~WRITER_BIT) | UPGRADE_BIT;
            if (m_state.compare_exchange_weak(
                    expected, new_state,
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                detail::futex_wake(&m_state, ALL, WRITER_BIT);
                return;
            }
        }
    }

    // 从写锁降级到读锁
    void unlock_and_lock_shared() noexcept {
        uint32_t expected = m_state.load(std::memory_order_acquire);

        // 必须持有写锁才能降级
        if (!(expected & WRITER_BIT)) {
            return; // 没有写锁，直接返回
        }

        while (expected & WRITER_BIT) {
            uint32_t new_state = (expected & ~WRITER_BIT) + 1;
            if (m_state.compare_exchange_weak(
                    expected, new_state,
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                detail::futex_wake(&m_state, ALL, WRITER_BIT);
                return;
            }
        }
    }

private:
    // 写锁抢占逻辑
    template <bool IsUpgrade, typename Context>
    bool acquire_writer_lock(Context& ctx) noexcept {
        constexpr uint32_t wait_mask =
            IsUpgrade ? (WRITER_BIT | READER_MASK) : (WRITER_BIT | READER_MASK | UPGRADE_BIT);
        constexpr uint32_t clear_mask =
            IsUpgrade ? (UPGRADE_BIT | WRITER_WAITING_BIT) : WRITER_WAITING_BIT;

        uint32_t count   = 0;
        uint32_t current = m_state.load(std::memory_order_acquire);

        while (!ctx.should_timeout()) {
            // 如果是升级锁，检查升级锁是否还存在
            if constexpr (IsUpgrade) {
                if (!(current & UPGRADE_BIT)) {
                    break; // 升级锁已丢失，退出循环
                }
            }

            // 设置写等待位
            if (!(current & WRITER_WAITING_BIT) &&
                (count >= Policy::write_limit) &&
                !m_state.compare_exchange_weak(
                    current, current | WRITER_WAITING_BIT,
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                continue;
            }

            // 每次循环都应该计数，确保能达到阈值
            if constexpr (Policy::write_limit != 0) {
                count++;
            }

            // 检查需要等待的条件：写锁 或 读锁
            // 注意：对于升级锁升级，不等待自己的升级锁
            if ((current & wait_mask) != 0) {
                ctx.wait(&m_state, current, WRITER_WAIT_MASK);
                continue;
            }

            // 清除指定位，设置写位
            uint32_t new_state = (current & ~clear_mask) | WRITER_BIT;

            if (m_state.compare_exchange_strong(
                    current, new_state,
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                return true; // 成功获取写锁
            }
        }

        // 超时失败，清理WRITER_WAITING_BIT避免读线程被永久阻塞
        m_state.fetch_and(~WRITER_WAITING_BIT, std::memory_order_acq_rel);
        detail::futex_wake(&m_state, ALL, WRITER_BIT);

        return false;
    }

    template <typename Context>
    bool acquire_shared_lock(Context& ctx) noexcept {
        uint32_t current = m_state.load(std::memory_order_acquire);

        while (!ctx.should_timeout()) {
            if (current & (WRITER_BIT | WRITER_WAITING_BIT)) {
                ctx.wait(&m_state, current, SHARED_WAIT_MASK);
                continue;
            }

            uint32_t new_state = current + 1;
            if (m_state.compare_exchange_strong(
                    current, new_state,
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                return true;
            }
        }

        return false;
    }

    template <typename Context>
    bool acquire_upgrade_lock(Context& ctx) noexcept {
        uint32_t current = m_state.load(std::memory_order_acquire);
        while (!ctx.should_timeout()) {
            if (current & UPGRADE_MASK) {
                ctx.wait(&m_state, current, UPGRADE_WAIT_MASK);
                continue;
            }

            if (m_state.compare_exchange_strong(
                    current, current | UPGRADE_BIT,
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                return true;
            }
        }

        return false;
    }

    alignas(4) std::atomic<uint32_t> m_state;
};

/**
 * @brief 写优先策略
 *
 * write_limit = 0 表示写优先：写线程立即设置等待位阻止新的读锁获取。
 */
struct writer_priority_policy {
    static constexpr uint32_t spin_limit  = MC_SYNC_DEFAULT_SPIN_LIMIT;
    static constexpr uint32_t yield_limit = MC_SYNC_DEFAULT_YIELD_LIMIT;
    static constexpr uint32_t write_limit = 0; // 写优先：立即阻止读线程
};

// 默认的 shared_mutex 是写优先的
using shared_mutex = basic_shared_mutex<writer_priority_policy>;

/**
 * @brief 读优先策略
 *
 * write_limit > 0 表示读优先：写线程在指定次数的尝试后才设置等待位阻止新的读锁。
 * - 较小的值：更快阻止读线程，降低写线程饥饿风险，但影响读并发性
 * - 较大的值：更好的读并发性，但写线程饥饿风险更高
 */
struct reader_priority_policy {
    static constexpr uint32_t spin_limit  = MC_SYNC_DEFAULT_SPIN_LIMIT;
    static constexpr uint32_t yield_limit = MC_SYNC_DEFAULT_YIELD_LIMIT;
    static constexpr uint32_t write_limit = 100; // 读优先：100次尝试后阻止读线程
};

// 读优先的 shared_mutex
using reader_priority_shared_mutex = basic_shared_mutex<reader_priority_policy>;

} // namespace mc::sync

namespace mc {

// 导出到 mc 命名空间
using sync::reader_priority_shared_mutex;
using sync::shared_mutex;

} // namespace mc

#endif // MC_SYNC_SHARED_MUTEX_H