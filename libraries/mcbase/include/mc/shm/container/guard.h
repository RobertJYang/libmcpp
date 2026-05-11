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

#ifndef MC_SHM_CONTAINER_GUARD_H
#define MC_SHM_CONTAINER_GUARD_H

#include <mc/common.h>
#include <mc/shm/ipc_mutex.h>

// 共享内存容器的 RAII 锁 guard
//
// 职责：
//  1. 构造时 lock_ex 获取 container_mutex（类似 allocator 的 arena_guard）
//  2. 若 lock_ex 返回 recovered，自动调用容器注册的 recover 回调
//     （处理 in-flight journal + fsck 扫描）
//  3. 析构释放锁
//
// 与 arena_guard 区别：allocator 只有一份内置 journal；容器数量不定，
// 每个容器有自己的 journal，因此采用回调 / 重载点让各容器注入恢复逻辑

namespace mc::shm::container {

// ---- 恢复函数指针 ----
//
// 参数 user_data：指向具体容器实例（避免模板膨胀，用 type erasure）
// 持锁条件下被调用，保证串行；实现方负责：
//   1) 读 container_journal 做 redo / rollback
//   2) fsck_scan 扫描节点并 isolate 损坏项
using recover_fn = void (*)(void* user_data) noexcept;

class MC_API container_guard {
public:
    // 构造：获取 mutex，若 recovered 则调用 recover 回调
    container_guard(ipc_mutex& mutex, recover_fn recover, void* user_data) noexcept;

    // 析构：释放 mutex
    ~container_guard() noexcept;

    container_guard(const container_guard&)            = delete;
    container_guard& operator=(const container_guard&) = delete;
    container_guard(container_guard&&)                 = delete;
    container_guard& operator=(container_guard&&)      = delete;

    // 构造期间 lock_ex 是否返回了 recovered（即是否触发过 recover）
    bool did_recover() const noexcept { return m_recovered; }

private:
    ipc_mutex* m_mutex;
    bool       m_recovered;
};

} // namespace mc::shm::container

#endif // MC_SHM_CONTAINER_GUARD_H
