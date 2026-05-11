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

#ifndef MC_SRC_DICT_INCLUDE_INDEX_H
#define MC_SRC_DICT_INCLUDE_INDEX_H

#include <mc/dict/entry.h>

#include <cstddef>
#include <memory>
#include <unordered_map>
#include <vector>

namespace mc::dict_types {

class dict_index {
public:
    dict_index();
    ~dict_index();

    dict_index(const dict_index&)            = delete;
    dict_index& operator=(const dict_index&) = delete;
    dict_index(dict_index&&)                 = delete;
    dict_index& operator=(dict_index&&)      = delete;

    entry* find(mc::string_view key) const;
    entry* find(const variant& key) const;
    // 预算 hash 入口：quark 等已持有 mc::string_hash 结果的查找路径用，省一次 hash
    entry* find(mc::string_view key, std::size_t hash_code) const;

    void insert(entry& item);
    void erase(entry& item) noexcept;
    void clear() noexcept;
    void reserve(std::size_t entry_count);

    std::size_t bucket_count() const noexcept;

private:
    static constexpr std::size_t s_inline_bucket_count = MC_DICT_BUCKET_COUNT;

    void        maybe_grow();
    void        maybe_shrink();
    void        rehash(std::size_t new_bucket_count);
    void        insert_existing(entry& item) noexcept;
    void        erase_at(std::size_t slot_index) noexcept;
    std::size_t ideal_index(std::size_t hash_code) const noexcept;
    std::size_t probe_distance(std::size_t slot_index, std::size_t hash_code) const noexcept;

    entry*                    m_inline_slots[s_inline_bucket_count]{};
    entry**                   m_slots{m_inline_slots};
    std::unique_ptr<entry*[]> m_dynamic_slots;
    std::size_t               m_bucket_count{s_inline_bucket_count};
    std::size_t               m_size{0};
};

class dict_order_cache {
public:
    void         rebuild(const entry_list& entries);
    const entry* at(std::size_t index) const noexcept;
    int          index_of(const entry* item) const noexcept;
    std::size_t  size() const noexcept;

private:
    std::vector<entry*>                   m_entries;
    std::unordered_map<const entry*, int> m_indices;
};

} // namespace mc::dict_types

#endif // MC_SRC_DICT_INCLUDE_INDEX_H
