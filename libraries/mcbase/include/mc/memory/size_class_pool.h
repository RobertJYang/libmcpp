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

#ifndef MC_MEMORY_SIZE_CLASS_POOL_H
#define MC_MEMORY_SIZE_CLASS_POOL_H

#include <mc/common.h>
#include <mc/memory/fixed_size_pool.h>

#include <cstddef>
#include <memory>

namespace mc::memory {

/**
 * @brief 固定 2 幂尺寸类的薄封装：构造时即创建 [min_slot_size, max_slot_size] 内每个 2 幂一档的 fixed_size_pool。
 *
 * 请求尺寸到池下标：从 min_slot_size 起按 2 幂向上取档（档数很少，无需查表）。
 * 超过 max_slot_size 的分配走 std::aligned_alloc，释放时 try_release 须传入与 try_acquire 相同的 request_size（见实现说明）。
 */
class MC_API size_class_pool {
public:
    struct acquire_result {
        void* ptr{nullptr};
        bool  reused{false};
    };

    struct release_result {
        bool accepted{false};
    };

    struct pool_stats {
        /// 构造时创建的档位数（与 min/max 2 幂范围一致）
        std::size_t active_classes{0};
    };

    struct options {
        std::size_t alignment{8};
        /// 最小槽位字节数（须为 2 的幂，默认 16）
        std::size_t min_slot_size{16};
        /// 最大槽位字节数（须为 2 的幂，默认 256）；超过 max_slot_size 时用 std::aligned_alloc（按 alignment 对齐）
        std::size_t              max_slot_size{256};
        fixed_size_pool::options default_class_options{};
    };

    explicit size_class_pool(const options& opts);
    ~size_class_pool();

    size_class_pool(const size_class_pool&)            = delete;
    size_class_pool& operator=(const size_class_pool&) = delete;
    size_class_pool(size_class_pool&&)                 = delete;
    size_class_pool& operator=(size_class_pool&&)      = delete;

    void*          try_acquire(std::size_t request_size);
    acquire_result try_acquire_with_status(std::size_t request_size);
    bool           try_release(void* ptr, std::size_t request_size) noexcept;
    release_result try_release_with_status(void* ptr, std::size_t request_size) noexcept;
    void           clear();
    std::size_t    trim(std::size_t keep_cached_bytes = 0);
    pool_stats     get_stats() const;

private:
    static constexpr std::size_t s_invalid_class_index = static_cast<std::size_t>(-1);

    static bool is_power_of_two(std::size_t n) noexcept;
    std::size_t classify_request(std::size_t request_size) const noexcept;

    options m_options{};
    /// 构造时按 min～max 2 幂档数确定，档数很少且固定，用连续堆数组而非 vector
    std::size_t                                         m_pool_count{0};
    std::unique_ptr<std::unique_ptr<fixed_size_pool>[]> m_pools{};
};

} // namespace mc::memory

#endif // MC_MEMORY_SIZE_CLASS_POOL_H
