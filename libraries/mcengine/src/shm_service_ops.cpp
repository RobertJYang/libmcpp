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

#include "shm_service_ops.h"

#include <signal.h>

#include <cerrno>
#include <cstring>
#include <new>
#include <utility>

#include "shm_object_ops.h"  // 复用 shm_byte_string_create / _destroy
#include "shm_service.h"     // shm_service_check

namespace mc::engine {

// ============================================================================
// 单 POD ops
// ============================================================================

void shm_service_refresh_crc(shm_service& svc) noexcept
{
    svc.crc32_self = shm_service_compute_crc(svc);
}

shm_service* shm_service_create(shm_allocator& alloc, std::string_view service_name,
                                std::uint32_t initial_pid, std::uint64_t initial_epoch) noexcept
{
    void* mem = alloc.allocate(sizeof(shm_service), alignof(shm_service));
    if (mem == nullptr) {
        return nullptr;
    }
    std::memset(mem, 0, sizeof(shm_service));
    auto* svc        = static_cast<shm_service*>(mem);
    svc->abi_version = shm_service_abi_version;
    svc->state       = static_cast<std::uint8_t>(shm_service_state::alive);
    svc->pid         = initial_pid;
    svc->epoch       = initial_epoch;

    auto* name_bs = shm_byte_string_create(alloc, service_name);
    if (name_bs == nullptr && !service_name.empty()) {
        alloc.deallocate(svc);
        return nullptr;
    }
    svc->service_name = name_bs;
    shm_service_refresh_crc(*svc);
    return svc;
}

void shm_service_destroy(shm_allocator& alloc, shm_service* svc) noexcept
{
    if (svc == nullptr) {
        return;
    }
    shm_byte_string_destroy(alloc, svc->service_name.get());
    alloc.deallocate(svc);
}

void shm_service_set_pid(shm_service& svc, std::uint32_t pid) noexcept
{
    svc.pid = pid;
    shm_service_refresh_crc(svc);
}

void shm_service_set_state(shm_service& svc, shm_service_state state) noexcept
{
    svc.state = static_cast<std::uint8_t>(state);
    shm_service_refresh_crc(svc);
}

std::uint64_t shm_service_increment_epoch(shm_service& svc) noexcept
{
    ++svc.epoch;
    shm_service_refresh_crc(svc);
    return svc.epoch;
}

std::string_view shm_service_name(const shm_service& svc) noexcept
{
    auto* bs = svc.service_name.get();
    return bs == nullptr ? std::string_view{} : bs->as_string_view();
}

// ============================================================================
// attach
// ============================================================================
//
// 设计要点：
//   - 已存在 → 直接接管（pid 改写 + epoch++ + CRC 刷新），返回原 svc 指针
//   - 不存在 → 创建新 svc + try_emplace 入表
//   - 罕见竞争（find 后另一进程抢先入表）：销毁本地新 svc，重新接管已有的

namespace {

// pid==0 视为已死；其余仅 ESRCH 视为已死，权限不足等保守判为存活以拒绝接管。
bool _pid_alive(std::uint32_t pid) noexcept
{
    if (pid == 0U) {
        return false;
    }
    if (::kill(static_cast<::pid_t>(pid), 0) == 0) {
        return true;
    }
    return errno != ESRCH;
}

// 从 map 中按 service_name 查找已存在的 svc，命中则更新 pid/epoch；返回 svc 或 nullptr。
shm_service* takeover_existing(shm_service_map& map, std::string_view service_name,
                               std::uint32_t current_pid) noexcept
{
    auto mp = map.find(service_name);
    if (!mp) {
        return nullptr;
    }
    shm_service* svc = mp.value->get();
    if (svc == nullptr) {
        return nullptr;
    }
    svc->pid = current_pid;
    ++svc->epoch;
    shm_service_refresh_crc(*svc);
    return svc;
}

}  // namespace

shm_service* shm_service_attach(shm_allocator& alloc, shm_service_map& map,
                                std::string_view service_name,
                                std::uint32_t    current_pid,
                                bool             force) noexcept
{
    // liveness 检查必须在 takeover_existing 改写 pid 之前完成。
    // CRC 损坏的 svc 视同无主（写入半成品 / 跨版本残留），跳过 liveness 检查直接接管。
    if (!force) {
        if (auto mp = map.find(service_name); mp) {
            shm_service* svc = mp.value->get();
            if (svc != nullptr && shm_service_check(*svc)
                && svc->state == static_cast<std::uint8_t>(shm_service_state::alive)
                && svc->pid != 0U && svc->pid != current_pid
                && _pid_alive(svc->pid)) {
                return nullptr;
            }
        }
    }

    if (auto* existing = takeover_existing(map, service_name, current_pid); existing != nullptr) {
        return existing;
    }

    shm_service* svc = shm_service_create(alloc, service_name, current_pid, /*initial_epoch=*/1U);
    if (svc == nullptr) {
        return nullptr;
    }

    auto bs = shm_byte_string::create(alloc, service_name);
    if (bs.size() == 0 && !service_name.empty()) {
        shm_service_destroy(alloc, svc);
        return nullptr;
    }

    auto [_, inserted] = map.try_emplace(std::move(bs), shm_ptr<shm_service>{svc});
    if (inserted) {
        return svc;
    }

    // 罕见竞争：本进程刚好与其他进程同时 attach，对方已先入表
    shm_service_destroy(alloc, svc);
    return takeover_existing(map, service_name, current_pid);
}

}  // namespace mc::engine
