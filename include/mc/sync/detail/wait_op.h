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

/**
 * @brief 实现一个混合等待策略，结合了自旋和线程 yield
 *
 * 在锁的竞争初期会进行一定次数的 CPU-pause 自旋，这对于短期的竞争非常高效。
 * 如果竞争持续会切换到 std::this_thread::yield()，让出 CPU 时间片给其他线程，
 * 避免浪费 CPU 资源
 *
 * @tparam SpinLimit 自旋的最大次数，超过后将切换到线程 yield
 */
template <uint32_t SpinLimit = MC_SYNC_DEFAULT_SPIN_LIMIT>
class wait_op {
public:
    wait_op() noexcept = default;

    /**
     * @brief 执行一次等待操作
     * 根据当前自旋次数决定是执行 cpu_pause 还是线程 yield
     */
    void spin() noexcept {
        m_spin_count++;
        if (m_spin_count <= SpinLimit) {
            cpu_pause();
        } else {
            std::this_thread::yield();
        }
    }

    /**
     * @brief 重置自旋计数器
     * 当成功获取锁之后，或者在新的锁尝试之前，应该调用此方法
     */
    void reset() noexcept {
        m_spin_count = 0;
    }

    /**
     * @brief 获取当前的自旋次数。
     */
    uint32_t get_spin_count() const noexcept {
        return m_spin_count;
    }

private:
    uint32_t m_spin_count{0};
};

} // namespace mc::sync::detail

#endif // MC_SYNC_WAIT_OP_H