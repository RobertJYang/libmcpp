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

#ifndef MC_INTRUSIVE_DETAIL_HASH_CORE_H
#define MC_INTRUSIVE_DETAIL_HASH_CORE_H

#include <mc/common.h>
#include <mc/intrusive/hook.h>

#include <cstddef>

namespace mc::intrusive::detail {

using unordered_match_fn = bool (*)(const void* key, const unordered_hook_state* node, const void* ctx) noexcept;

struct unordered_bucket {
    offset_ptr<unordered_hook_state> head{};
};

struct unordered_iterator_state {
    std::size_t           bucket_idx{0};
    unordered_hook_state* current{nullptr};
};

class MC_API unordered_core {
public:
    unordered_core(unordered_bucket* buckets = nullptr, std::size_t bucket_count = 0) noexcept;

    unordered_core(const unordered_core&)            = delete;
    unordered_core& operator=(const unordered_core&) = delete;
    unordered_core(unordered_core&&)                 = delete;
    unordered_core& operator=(unordered_core&&)      = delete;

    void reset_buckets(unordered_bucket* buckets, std::size_t bucket_count) noexcept;

    std::size_t bucket_count() const noexcept;

    unordered_hook_state*       bucket_head(std::size_t index) noexcept;
    const unordered_hook_state* bucket_head(std::size_t index) const noexcept;
    const unordered_hook_state* find(std::size_t index, std::size_t hash, const void* key, unordered_match_fn match,
                                     const void* ctx) const noexcept;
    std::size_t                 count_all() const noexcept;

    unordered_hook_state*       next(unordered_hook_state* node) noexcept;
    const unordered_hook_state* next(const unordered_hook_state* node) const noexcept;

    void insert_front(std::size_t index, std::size_t hash, unordered_hook_state* node) noexcept;
    bool erase(std::size_t index, unordered_hook_state* node) noexcept;
    void clear() noexcept;

    using unordered_dispose_fn = void (*)(unordered_hook_state* node, void* ctx);

    void clear_and_dispose(unordered_dispose_fn dispose_fn, void* ctx);
    void swap_rehash(unordered_core& other) noexcept;
    void rehash(unordered_bucket* new_buckets, std::size_t new_bucket_count) noexcept;

    std::size_t cached_begin() const noexcept;

    unordered_iterator_state begin() const noexcept;
    unordered_iterator_state advance(const unordered_iterator_state& state) const noexcept;

private:
    unordered_bucket*     bucket_at(std::size_t index) const noexcept;
    static void           reset_hook(unordered_hook_state* node) noexcept;
    void                  refresh_cached_begin() noexcept;

    offset_ptr<unordered_bucket> m_buckets{};
    std::size_t                  m_bucket_count{0};
    std::size_t                  m_cached_begin{0};
};

} // namespace mc::intrusive::detail

#endif // MC_INTRUSIVE_DETAIL_HASH_CORE_H
