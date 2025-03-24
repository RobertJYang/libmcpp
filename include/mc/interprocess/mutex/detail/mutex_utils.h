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

/**
 * @file mutex_utils.h
 * @brief 互斥锁相关的工具函数
 */
#ifndef MC_INTERPROCESS_MUTEX_DETAIL_MUTEX_UTILS_H
#define MC_INTERPROCESS_MUTEX_DETAIL_MUTEX_UTILS_H

#include <chrono>
#include <thread>
#include <sys/time.h>
#include <algorithm>

namespace mc::interprocess::detail {

/**
 * @brief 获取当前时间（微秒）
 * @return 当前时间戳（微秒）
 */
inline uint64_t get_current_time_us() {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return static_cast<uint64_t>(tv.tv_sec) * 1000000 + tv.tv_usec;
}

/**
 * @brief 根据尝试次数计算等待时间
 * @param attempt_count 尝试次数
 */
inline void wait_based_on_attempts(size_t attempt_count) {
    // 使用指数退避策略，但设置上限
    constexpr size_t max_sleep_ms = 100;  // 最大等待100毫秒
    constexpr size_t base_sleep_ms = 1;   // 基础等待1毫秒

    // 计算当前应该等待的时间（指数增长）
    size_t sleep_ms = base_sleep_ms << std::min(attempt_count, size_t(6));  // 最多左移6位
    sleep_ms = std::min(sleep_ms, max_sleep_ms);  // 确保不超过最大等待时间

    // 休眠指定时间
    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
}

} // namespace mc::interprocess::detail

#endif // MC_INTERPROCESS_MUTEX_DETAIL_MUTEX_UTILS_H 