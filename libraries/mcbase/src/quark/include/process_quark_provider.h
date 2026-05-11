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
 * @file process_quark_provider.h
 * @brief 进程内 quark 后端：基于 mc::intrusive::unordered_set 实现，支持自动 rehash
 */
#ifndef MC_SRC_QUARK_PROCESS_QUARK_PROVIDER_H
#define MC_SRC_QUARK_PROCESS_QUARK_PROVIDER_H

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <vector>

#include <mc/intrusive/unordered_set.h>
#include <mc/quark.h>

#include <mc/common.h>

#include "quark_provider.h"
#include "quark_record.h"

namespace mc::quark_detail {

class process_quark_provider final : public quark_provider {
public:
    using set_type = mc::intrusive::unordered_set<quark_record, mc::intrusive::hash<view_hasher>,
                                                  mc::intrusive::equal<view_equal>>;

    /**
     * @param initial_buckets bucket 初始数量，必须 2 的幂
     * @param soft_max_count  容量软上限；超过仅告警，超容返回 invalid_id
     */
    process_quark_provider(std::size_t initial_buckets, std::size_t soft_max_count);

    ~process_quark_provider() override;

    process_quark_provider(const process_quark_provider&)            = delete;
    process_quark_provider& operator=(const process_quark_provider&) = delete;

    quark::id_type intern(mc::string_view value) override;
    quark::id_type lookup(mc::string_view value) noexcept override;
    const quark_record* resolve(quark::id_type id) noexcept override;
    std::size_t         size() const noexcept override;

    // ---- 测试辅助 ----
    std::size_t bucket_count() const noexcept;
    std::size_t capacity() const noexcept;

private:
    const char* _store_string(mc::string_view value);
    quark::id_type _create_record_locked(mc::string_view value, std::uint32_t hash);
    void _rehash_if_needed_locked();
    void _rebuild_set_locked(std::size_t new_bucket_count);
    void _emit_overflow_warning_locked();
    static std::size_t _next_pow2(std::size_t value) noexcept;
    static std::size_t _records_capacity_for(std::size_t soft_max_count) noexcept;

    static constexpr std::size_t string_arena_chunk_size = 64U * 1024U;

    struct string_chunk {
        std::unique_ptr<char[]> data;
        std::size_t             used;
        std::size_t             capacity;
    };

    mutable std::shared_mutex                           m_mutex;
    std::size_t                                         m_soft_max_count;
    std::size_t                                         m_records_capacity;
    std::size_t                                         m_bucket_count;
    bool                                                m_overflow_warned{false};
    std::atomic<std::size_t>                            m_records_published{0U};
    std::unique_ptr<set_type::bucket_type[]>            m_buckets;
    std::unique_ptr<set_type>                           m_set;
    std::vector<std::unique_ptr<quark_record>>          m_records_by_id;
    std::vector<string_chunk>                           m_string_chunks;
};

} // namespace mc::quark_detail

#endif // MC_SRC_QUARK_PROCESS_QUARK_PROVIDER_H
