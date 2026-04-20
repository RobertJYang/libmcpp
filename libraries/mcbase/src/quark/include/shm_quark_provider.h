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
 * @file shm_quark_provider.h
 * @brief 共享内存后端的 quark provider
 */
#ifndef MC_SRC_QUARK_SHM_QUARK_PROVIDER_H
#define MC_SRC_QUARK_SHM_QUARK_PROVIDER_H

#include <atomic>
#include <cstddef>
#include <cstdint>

#include <mc/intrusive/hook.h>
#include <mc/intrusive/offset_ptr.h>
#include <mc/intrusive/unordered_set.h>
#include <mc/quark.h>
#include <mc/shm/allocator.h>
#include <mc/shm/ipc_shared_mutex.h>
#include <mc/shm/shm_runtime.h>
#include <mc/string_view.h>

#include "quark_provider.h"
#include "quark_record.h"

namespace mc::quark_detail {

/** @brief SHM 中的 quark control 块 */
struct shm_quark_control {
    static constexpr std::uint32_t magic_value      = 0x5155524BU; // 'QURK'
    static constexpr std::uint32_t version_value    = 1U;

    std::atomic<std::uint32_t> magic{0U};
    std::atomic<std::uint32_t> version{0U};
    std::atomic<std::uint32_t> initialized{0U};
    std::atomic<std::uint32_t> next_id{2U}; // 0=invalid, 1=empty
    std::atomic<std::uint64_t> live_count{0U};

    std::uint32_t              bucket_count{0U};
    std::uint32_t              soft_max_count{0U};
    std::uint32_t              records_capacity{0U};
    std::atomic<std::uint64_t> arena_used{0U};
    std::uint64_t              arena_size{0U};

    mc::intrusive::offset_ptr<mc::intrusive::detail::unordered_bucket> buckets{};
    mc::intrusive::offset_ptr<mc::intrusive::offset_ptr<quark_record>> records_by_id{};
    mc::intrusive::offset_ptr<char>                                    string_arena{};

    mc::shm::ipc_shared_mutex lock;
};

/** @brief shm 后端 quark provider */
class shm_quark_provider final : public quark_provider {
public:
    using set_type = mc::intrusive::unordered_set<quark_record,
                                                  mc::intrusive::hash<view_hasher>,
                                                  mc::intrusive::equal<view_equal>,
                                                  mc::intrusive::constant_time_size<false>>;

    /**
     * @param runtime         SHM runtime（需 is_valid()）
     * @param bucket_count    固定桶数（不是 2 的幂时向上取整）
     * @param soft_max_count  软上限，超过仅告警
     * @param arena_size      字符串 arena 字节数；不足时新 intern 返回 invalid_id
     */
    shm_quark_provider(mc::shm::shm_runtime& runtime, std::size_t bucket_count, std::size_t soft_max_count,
                       std::size_t arena_size) noexcept;

    ~shm_quark_provider() override;

    shm_quark_provider(const shm_quark_provider&)            = delete;
    shm_quark_provider& operator=(const shm_quark_provider&) = delete;

    bool is_valid() const noexcept;

    mc::quark::id_type  intern(mc::string_view value) override;
    mc::quark::id_type  lookup(mc::string_view value) noexcept override;
    const quark_record* resolve(mc::quark::id_type id) noexcept override;
    std::size_t         size() const noexcept override;
    std::size_t         bucket_count() const noexcept;

private:
    bool                _attach_or_initialize_locked() noexcept;
    bool                _initialize_control_locked() noexcept;
    void                _build_local_set_locked();
    quark_record*       _allocate_record_locked() noexcept;
    char*               _store_string_locked(mc::string_view value) noexcept;
    mc::quark::id_type  _create_record_locked(mc::string_view value, std::uint32_t hash) noexcept;
    void                _emit_overflow_warning_locked();

    static std::size_t  _next_pow2(std::size_t value) noexcept;

    mc::shm::shm_runtime* m_runtime{nullptr};
    mc::shm::shm_allocator m_allocator{};
    shm_quark_control*    m_control{nullptr};
    std::unique_ptr<set_type> m_set;
    bool                  m_overflow_warned{false};
};

} // namespace mc::quark_detail

#endif // MC_SRC_QUARK_SHM_QUARK_PROVIDER_H
