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

#include <chrono>
#include <mc/sync/detail/wait_op.h>

namespace mc::sync::detail {

/**
 * @brief 无限期等待的策略。
 */
struct wait_forever {
    wait_op<MC_SYNC_DEFAULT_SPIN_LIMIT> m_wait_op;

    // should_timeout 永远返回 false
    bool should_timeout() const {
        return false;
    }

    void spin() {
        m_wait_op.spin();
    }
};

/**
 * @brief 等待一段指定时间的策略。
 */
template <typename Duration>
struct wait_for_duration {
    using time_point_type = std::chrono::steady_clock::time_point;
    using duration_type   = std::chrono::steady_clock::duration;
    using wait_op_type    = wait_op<MC_SYNC_DEFAULT_SPIN_LIMIT>;

    wait_op_type    m_wait_op;
    time_point_type m_deadline;

    wait_for_duration(const Duration& duration)
        : m_deadline(std::chrono::steady_clock::now() +
                     static_cast<duration_type>(duration)) {
    }

    bool should_timeout() const {
        // 只有在自旋了一定次数后才检查时间，以优化性能。
        // 这里的 255 是一个经验值，避免频繁调用 now()。
        if ((m_wait_op.get_spin_count() & 255) == 255) {
            return std::chrono::steady_clock::now() >= m_deadline;
        }
        return false;
    }

    void spin() {
        m_wait_op.spin();
    }
};

} // namespace mc::sync::detail

#endif // MC_SYNC_WAIT_CONTEXT_H