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

#include <mc/memory/size_class_pool.h>

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <stdlib.h>

namespace mc::memory {

namespace {

std::size_t round_up_to_multiple(std::size_t value, std::size_t multiple) noexcept
{
    if (multiple == 0) {
        return 0;
    }
    const auto quotient = (value + multiple - 1) / multiple;
    return quotient * multiple;
}

std::size_t count_power_of_two_classes(std::size_t min_slot, std::size_t max_slot)
{
    std::size_t n = 0;
    for (std::size_t s = min_slot; s <= max_slot; s *= 2) {
        ++n;
    }
    if (n == 0) {
        throw std::invalid_argument("size_class_pool: min_slot_size 与 max_slot_size 无合法 2 幂档位");
    }
    return n;
}

/// need >= 1：不小于 need 的最小 2 幂。msb = floor(log2(need)) 为最高位 1 的位号（从 0 计），
/// 若 need 已是 2 幂则结果为 2^msb，否则为 2^(msb+1)。
std::size_t ceil_smallest_power_of_two(std::size_t need) noexcept
{
    if (need <= 1) {
        return 1;
    }
    std::size_t msb = 0;
#if defined(__GNUC__) || defined(__clang__)
    const auto n = static_cast<unsigned long long>(need);
    msb          = static_cast<std::size_t>(std::numeric_limits<unsigned long long>::digits - 1 - __builtin_clzll(n));
#else
    std::size_t t = need;
    while (t >>= 1) {
        ++msb;
    }
#endif
    if ((need & (need - 1)) == 0) {
        return need;
    }
    return std::size_t{1} << (msb + 1);
}

/// ratio 为 2 的幂时，返回 k 使 ratio == 2^k
std::size_t log2_exact_power_of_two(std::size_t ratio) noexcept
{
#if defined(__GNUC__) || defined(__clang__)
    return static_cast<std::size_t>(__builtin_ctzll(static_cast<unsigned long long>(ratio)));
#else
    std::size_t k = 0;
    while (ratio > 1) {
        ratio >>= 1;
        ++k;
    }
    return k;
#endif
}

/// 超过池上限时使用，与 fixed_size_pool 段分配一致：对齐块大小为 alignment 的整数倍
void* allocate_overflow_system(std::size_t request_size, std::size_t alignment) noexcept
{
    if (request_size == 0) {
        return nullptr;
    }
    const std::size_t raw = round_up_to_multiple(request_size, alignment);
    if (raw < request_size) {
        return nullptr;
    }
    return ::aligned_alloc(alignment, raw);
}

} // namespace

bool size_class_pool::is_power_of_two(std::size_t n) noexcept
{
    return n != 0 && (n & (n - 1)) == 0;
}

size_class_pool::size_class_pool(const options& opts) : m_options(opts)
{
    if (!is_power_of_two(m_options.alignment)) {
        throw std::invalid_argument("size_class_pool: alignment 必须为 2 的幂且非 0");
    }
    if (!is_power_of_two(m_options.min_slot_size) || !is_power_of_two(m_options.max_slot_size)) {
        throw std::invalid_argument("size_class_pool: min_slot_size / max_slot_size 必须为 2 的幂");
    }
    if (m_options.min_slot_size > m_options.max_slot_size) {
        throw std::invalid_argument("size_class_pool: min_slot_size 不能大于 max_slot_size");
    }

    const std::size_t class_count = count_power_of_two_classes(m_options.min_slot_size, m_options.max_slot_size);
    m_pool_count                  = class_count;
    m_pools                       = std::make_unique<std::unique_ptr<fixed_size_pool>[]>(class_count);

    for (std::size_t i = 0; i < class_count; ++i) {
        const auto slot_size   = m_options.min_slot_size << i;
        const auto object_size = round_up_to_multiple(slot_size, m_options.alignment);
        m_pools[i] =
            std::make_unique<fixed_size_pool>(object_size, m_options.alignment, m_options.default_class_options);
    }
}

size_class_pool::~size_class_pool() = default;

std::size_t size_class_pool::classify_request(std::size_t request_size) const noexcept
{
    if (request_size == 0 || request_size > m_options.max_slot_size) {
        return s_invalid_class_index;
    }

    // need=max(req,min) 后：槽大小 = ceil_pow2(need)，由 need 的最高位 1（msb）决定 2^msb 或 2^(msb+1)
    const std::size_t min_s         = m_options.min_slot_size;
    const std::size_t need_at_least = std::max(request_size, min_s);
    const std::size_t slot          = ceil_smallest_power_of_two(need_at_least);
    const std::size_t ratio         = slot / min_s;
    const std::size_t idx           = log2_exact_power_of_two(ratio);
    if (idx >= m_pool_count) {
        return s_invalid_class_index;
    }
    return idx;
}

void* size_class_pool::try_acquire(std::size_t request_size)
{
    return try_acquire_with_status(request_size).ptr;
}

size_class_pool::acquire_result size_class_pool::try_acquire_with_status(std::size_t request_size)
{
    if (request_size == 0) {
        return {};
    }
    if (request_size > m_options.max_slot_size) {
        void* const ptr = allocate_overflow_system(request_size, m_options.alignment);
        return {
            ptr,
            false,
        };
    }

    const auto class_index = classify_request(request_size);
    if (class_index == s_invalid_class_index) {
        return {};
    }

    const auto result = m_pools[class_index]->allocate_with_status();
    return {
        result.ptr,
        result.reused,
    };
}

bool size_class_pool::try_release(void* ptr, std::size_t request_size) noexcept
{
    return try_release_with_status(ptr, request_size).accepted;
}

size_class_pool::release_result size_class_pool::try_release_with_status(void* ptr, std::size_t request_size) noexcept
{
    if (ptr == nullptr) {
        return {};
    }
    // 与 try_acquire 使用相同的 request_size：超过 max 的为 aligned_alloc，否则走子池
    if (request_size > m_options.max_slot_size) {
        std::free(ptr);
        return {true};
    }
    if (request_size == 0) {
        return {};
    }

    const auto class_index = classify_request(request_size);
    if (class_index == s_invalid_class_index) {
        return {};
    }

    m_pools[class_index]->deallocate(ptr);
    return {true};
}

void size_class_pool::clear()
{
    for (std::size_t i = 0; i < m_pool_count; ++i) {
        m_pools[i]->clear();
    }
}

std::size_t size_class_pool::trim(std::size_t keep_cached_bytes)
{
    MC_UNUSED(keep_cached_bytes);
    std::size_t released_bytes = 0;
    for (std::size_t i = 0; i < m_pool_count; ++i) {
        const auto slot_stride = m_pools[i]->slot_stride();
        released_bytes += m_pools[i]->trim(0) * slot_stride;
    }
    return released_bytes;
}

size_class_pool::pool_stats size_class_pool::get_stats() const
{
    return {
        m_pool_count,
    };
}

} // namespace mc::memory
