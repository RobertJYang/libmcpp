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

/**
 * @file quark_provider.cpp
 * @brief active_provider 选择 + MC_QUARK 链接段批量注册
 */

#include "include/quark_provider.h"

#include <atomic>
#include <memory>
#include <mutex>

#include <mc/log/log.h>
#include <mc/quark.h>

#if defined(MC_QUARK_STORAGE_SHARED_MEMORY)
#include <mc/shm/default_runtime.h>
#include "include/shm_quark_provider.h"
#else
#include "include/process_quark_provider.h"
#endif

#ifndef MC_QUARK_INITIAL_BUCKETS
#define MC_QUARK_INITIAL_BUCKETS 8192U
#endif

#ifndef MC_QUARK_MAX_COUNT
#define MC_QUARK_MAX_COUNT 8192U
#endif

#ifndef MC_QUARK_SHM_ARENA_SIZE
#define MC_QUARK_SHM_ARENA_SIZE (256U * 1024U)
#endif

#ifndef MC_QUARK_MAX_LITERAL_LENGTH
#define MC_QUARK_MAX_LITERAL_LENGTH 256U
#endif

namespace mc::quark_detail {

quark_provider::~quark_provider() = default;

namespace {

quark_provider* _build_default_provider()
{
#if defined(MC_QUARK_STORAGE_SHARED_MEMORY)
    auto& runtime = mc::shm::default_runtime();
    auto* shm_p = new shm_quark_provider(runtime,
                                         static_cast<std::size_t>(MC_QUARK_INITIAL_BUCKETS),
                                         static_cast<std::size_t>(MC_QUARK_MAX_COUNT),
                                         static_cast<std::size_t>(MC_QUARK_SHM_ARENA_SIZE));
    if (!shm_p->is_valid()) {
        elog("mc::quark shm backend unavailable");
    }
    return shm_p;
#else
    return new process_quark_provider(static_cast<std::size_t>(MC_QUARK_INITIAL_BUCKETS),
                                      static_cast<std::size_t>(MC_QUARK_MAX_COUNT));
#endif
}

} // namespace

quark_provider& active_provider() noexcept
{
    static std::unique_ptr<quark_provider> g_provider(_build_default_provider());
    return *g_provider;
}

} // namespace mc::quark_detail

namespace mc::detail {

quark::id_type intern_trusted_literal(mc::string_view value) noexcept
{
    // 超长字面量返回 invalid_id
    if (value.size() > static_cast<std::size_t>(MC_QUARK_MAX_LITERAL_LENGTH)) {
        return mc::quark::invalid_id;
    }
    return mc::quark_detail::active_provider().intern(value);
}

quark::id_type slow_intern_slot(quark_slot& slot) noexcept
{
    const auto id = intern_trusted_literal(
        mc::string_view(slot.literal, static_cast<std::size_t>(slot.length)));
    slot.id.store(id, std::memory_order_release);
    return id;
}

} // namespace mc::detail

// GCC/Clang 在自定义节存在时自动生成 __start_/__stop_ 边界符号；
// weak 使得无任何 MC_QUARK 调用点时仍能链接通过。
#if defined(__linux__) && (defined(__GNUC__) || defined(__clang__))

extern "C" {
extern mc::detail::quark_slot __start_mc_quark_slots[] __attribute__((weak));
extern mc::detail::quark_slot __stop_mc_quark_slots[] __attribute__((weak));
}

namespace mc::quark_detail {
namespace {

/** @brief 启动期批量 intern 段内所有 slot */
__attribute__((constructor(150))) void initialize_quark_slots() noexcept
{
    if (__start_mc_quark_slots == nullptr || __stop_mc_quark_slots == nullptr) {
        return;
    }
    if (__start_mc_quark_slots == __stop_mc_quark_slots) {
        return;
    }

    (void)active_provider();
    for (auto* slot = __start_mc_quark_slots; slot < __stop_mc_quark_slots; ++slot) {
        if (slot->id.load(std::memory_order_relaxed) != mc::quark::invalid_id) {
            continue;
        }
        const auto id = mc::detail::intern_trusted_literal(
            mc::string_view(slot->literal, static_cast<std::size_t>(slot->length)));
        slot->id.store(id, std::memory_order_release);
    }
}

} // namespace
} // namespace mc::quark_detail

#endif // __linux__ && (GCC || Clang)
