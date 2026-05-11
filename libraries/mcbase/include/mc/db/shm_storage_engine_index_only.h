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

#ifndef MC_DATABASE_SHM_STORAGE_ENGINE_INDEX_ONLY_H
#define MC_DATABASE_SHM_STORAGE_ENGINE_INDEX_ONLY_H

#include <mc/common.h>
#include <mc/db/shm_storage_engine.h>
#include <mc/exception.h>
#include <mc/intrusive/offset_ptr.h>
#include <mc/shm/byte_string.h>
#include <mc/shm/container/map.h>
#include <mc/shm/shm_runtime.h>

#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace mc::db {

// ============================================================================
// shm_storage_engine_index_only（纯索引 SHM 存储引擎）
//
// 仅索引、无 heap pool 的 SHM 存储引擎，专给"系统级全局表"使用：
//   - 全局 object_table 不持对象生命周期（每个 service 自己持），只在 SHM 中
//     维护 `byte_string -> offset_ptr<ShmRecord>` 这层"哪个对象在哪儿"的索引；
//   - 任意进程查询：拿到 ShmRecord*，由调用方决定如何包装（同进程可直接读，
//     跨进程通常封一层只读 proxy）；
//   - 不需要 reconstruct_fn：索引天然就在 SHM，重启即可见。
//
// 设计契约：
//   - **不**符合通用 storage_engine concept（不持 shared_ptr<Object>）；
//     专门给"持久化纯索引"场景，业务代码自己决定如何使用。
//   - 与 shm_storage_engine 共享 shm_engine_alloc + derive_per_index_alloc 命名规则。
//   - 无 heap pool → 多进程读完全无锁竞争（仅 shm::map 内部的 container_guard）。
//
// 典型用法：
//   shm_storage_engine_index_only<shm_object, 3>
//                                 e{alloc, "object_table"};
//   e.put(0, key, sh);
//   auto* sh = e.find(0, key);
// ============================================================================

template <typename ShmRecord, std::size_t IndexCount, typename Allocator = shm_engine_alloc>
class shm_storage_engine_index_only {
    static_assert(IndexCount >= 1U, "shm_storage_engine_index_only 至少需要 1 个 index");

public:
    using shm_record_type = ShmRecord;
    using shm_record_ptr  = mc::intrusive::offset_ptr<ShmRecord>;
    using allocator_type  = Allocator;
    using map_type =
        mc::shm::container::map<mc::shm::byte_string, shm_record_ptr, mc::shm::byte_string_less>;

    static constexpr std::size_t index_count = IndexCount;

    explicit shm_storage_engine_index_only(const allocator_type& alloc      = allocator_type(),
                                           mc::string_view       table_name = mc::string_view())
        : m_alloc(alloc), m_table_name(table_name.data(), table_name.size())
    {
        if (m_alloc.runtime != nullptr) {
            _bind_all();
        }
    }

    ~shm_storage_engine_index_only()                                                 = default;
    shm_storage_engine_index_only(const shm_storage_engine_index_only&)              = delete;
    shm_storage_engine_index_only& operator=(const shm_storage_engine_index_only&)   = delete;
    shm_storage_engine_index_only(shm_storage_engine_index_only&&)                   = delete;
    shm_storage_engine_index_only& operator=(shm_storage_engine_index_only&&)        = delete;

    // ----- 写 -----
    // 返回 (旧 ShmRecord*, 是否覆盖)
    std::pair<ShmRecord*, bool> put(std::size_t idx, mc::string_view key, ShmRecord* sh)
    {
        auto& m = _at(idx);
        if (sh == nullptr) {
            return {nullptr, false};
        }
        ShmRecord* old = nullptr;
        if (auto existing = m.find(key); existing) {
            old = existing.value->get();
            if (!m.erase(key)) {
                return {nullptr, false};
            }
        }
        auto& arena = *m.allocator();
        auto  bs    = mc::shm::byte_string::create(arena, key);
        if (bs.size() == 0 && !key.empty()) {
            return {old, old != nullptr};
        }
        auto [mp, inserted] = m.try_emplace(std::move(bs), shm_record_ptr(sh));
        (void)mp;
        (void)inserted;
        return {old, old != nullptr};
    }

    // 返回被删的 ShmRecord*（删除前 map 中保存的）；不存在返回 nullptr。
    ShmRecord* erase(std::size_t idx, mc::string_view key)
    {
        auto& m  = _at(idx);
        auto  mp = m.find(key);
        if (!mp) {
            return nullptr;
        }
        ShmRecord* old = mp.value->get();
        if (!m.erase(key)) {
            return nullptr;
        }
        return old;
    }

    void clear(std::size_t idx) { _at(idx).clear(); }

    void clear()
    {
        for (std::size_t i = 0; i < IndexCount; ++i) {
            m_maps[i]->clear();
        }
    }

    // ----- 读 -----
    ShmRecord* find(std::size_t idx, mc::string_view key) const
    {
        const auto& m  = _at(idx);
        auto        mp = m.find(key);
        if (!mp) {
            return nullptr;
        }
        return mp.value->get();
    }

    bool        empty(std::size_t idx) const { return _at(idx).empty(); }
    std::size_t size(std::size_t idx) const { return _at(idx).size(); }

    // ----- 遍历 -----
    using const_iterator = typename map_type::const_iterator;
    const_iterator begin(std::size_t idx) const { return _at(idx).cbegin(); }
    const_iterator end(std::size_t idx) const { return _at(idx).cend(); }

    // ----- 诊断 -----
    map_type&             index_map(std::size_t idx) { return _at(idx); }
    const map_type&       index_map(std::size_t idx) const { return _at(idx); }
    const allocator_type& get_allocator() const noexcept { return m_alloc; }
    mc::string_view       table_name() const noexcept { return m_table_name; }

private:
    void _bind_all()
    {
        for (std::size_t i = 0; i < IndexCount; ++i) {
            shm_engine_alloc per = derive_per_index_alloc(m_alloc, i, m_table_name);
            auto             opt =
                per.runtime->get_or_create_map<mc::shm::byte_string, shm_record_ptr,
                                               mc::shm::byte_string_less>(per.name);
            if (!opt) {
                MC_THROW(mc::invalid_arg_exception,
                         "shm_storage_engine_index_only: get_or_create_map 失败 name=${n}",
                         ("n", per.name));
            }
            m_maps[i] = std::make_unique<map_type>(std::move(*opt));
        }
    }

    map_type& _at(std::size_t idx) const
    {
        if (idx >= IndexCount) {
            MC_THROW(mc::invalid_arg_exception,
                     "shm_storage_engine_index_only: index_id 越界 idx=${idx}, IndexCount=${n}",
                     ("idx", idx)("n", IndexCount));
        }
        if (!m_maps[idx]) {
            MC_THROW(mc::invalid_arg_exception,
                     "shm_storage_engine_index_only: 未绑定 shm_runtime");
        }
        return *m_maps[idx];
    }

    allocator_type                                    m_alloc;
    std::string                                       m_table_name;
    std::array<std::unique_ptr<map_type>, IndexCount> m_maps{};
};

} // namespace mc::db

#endif // MC_DATABASE_SHM_STORAGE_ENGINE_INDEX_ONLY_H
