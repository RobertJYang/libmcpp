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

#ifndef MC_SHM_SRC_DETAIL_REGION_REGISTRY_H
#define MC_SHM_SRC_DETAIL_REGION_REGISTRY_H

#include <cstddef>
#include <cstdint>
#include <optional>

namespace mc::shm::detail {

// 进程级 region registry：记录本进程已打开 shm_region 的 user area 范围
//
// 每条记录一个连续地址区间 [user_base, user_base + user_size)，
// 对应一个 shm_allocator 所管理的 arena。
//
// 用途：给定位于 user area 的任意指针，找到其所属 region 的 user_base + user_size，
// 从而在运行时重建 shm_allocator({user_base, user_size})。
//
// 典型调用者：mc::shm::string 的析构 / destroy()——避免 string 对象内存放
// shm_allocator* 裸指针，以便跨进程安全。
//
// 线程安全：读多写少。注册/注销发生在 shm_region 生命周期，频率极低。
// 查询发生在 string::destroy 等路径，内部走 std::shared_mutex 读锁。
//
// 复杂度：N 为本进程当前打开的 region 数，一般 < 10。
// 查询 O(log N)（sorted vector 二分）；注册/注销 O(N)（保持有序插入）。

struct region_info {
    void*       user_base = nullptr;
    std::size_t user_size = 0;
};

// 注册一个 region。user_base / user_size 分别是 shm_allocator 的 base 和 size
// （即 shm_region::user_base() / user_size()）。
// 重复注册同一 base 会覆盖旧记录。
void register_region(void* user_base, std::size_t user_size) noexcept;

// 注销 region。传 user_base 必须和 register 时一致。
// 未注册的 base 会被静默忽略。
void unregister_region(void* user_base) noexcept;

// 给定任意指针，查所属 region。ptr 可以指向 user area 的任何位置（包括分配块内部）。
// 不在任何已注册 region 中则返回 nullopt。
std::optional<region_info> find_region_for(const void* ptr) noexcept;

// 仅限测试 / 诊断：返回当前注册的 region 数量
std::size_t registered_region_count() noexcept;

} // namespace mc::shm::detail

#endif // MC_SHM_SRC_DETAIL_REGION_REGISTRY_H
