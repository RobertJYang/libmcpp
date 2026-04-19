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

#ifndef MC_DATABASE_STORAGE_ENGINE_H
#define MC_DATABASE_STORAGE_ENGINE_H

#include <mc/db/common.h>

#include <cstddef>
#include <type_traits>

namespace mc::db {

// ============================================================================
// storage_engine 抽象接口
//
// per-table、N-index 存储引擎：
//   - 一个 db::table 持有一个 Engine 实例
//   - Engine 内部维护 N 个有序索引（local 实现 = N 棵 radix_tree；
//                                  shm 实现   = N 个 shm::map）
//   - db::index 是 (key_extractor + index_id) 元数据载体，所有树操作转发到
//     m_engine->op(index_id, ...)
//
// 引擎不暴露 root() / tree_view 等"内部树"鸭子接口；所有读/写都通过下方显式
// API 完成，避免上层代码直接戳到引擎内部数据结构。
//
// ----------------------------------------------------------------------------
// Engine 必须暴露的成员类型：
//   object_type           被存对象类型
//   object_ptr_type       mc::shared_ptr<object_type>
//   leaf_type             std::optional<object_ptr_type>
//   allocator_type        分配器类型
//   raw_iterator          per-index 原生迭代器（兼容 db::iterator 现有契约：
//                         operator++/==/!=、is_end()、key()、->second、
//                         to_next_prefix(prefix_view)）
//
// per-index 写：
//   std::pair<leaf_type, bool> insert(std::size_t idx, mc::string_view k,
//                                     object_ptr_type v);
//   leaf_type                  remove(std::size_t idx, mc::string_view k);
//   void                       clear(std::size_t idx);
//
// per-index 读：
//   leaf_type    get        (std::size_t idx, mc::string_view k) const;
//   raw_iterator find       (std::size_t idx, mc::string_view k);
//   raw_iterator lower_bound(std::size_t idx, mc::string_view k);
//   raw_iterator upper_bound(std::size_t idx, mc::string_view k);
//   raw_iterator begin      (std::size_t idx);
//   raw_iterator end        (std::size_t idx);
//   bool         empty      (std::size_t idx) const;
//   std::size_t  size       (std::size_t idx) const;
//
// engine-wide：
//   void clear();                  // 清空所有 index
//   void commit();                 // 整个 engine 提交一次
//   void rollback();               // 整个 engine 回滚一次
//   int  save_point();             // 在所有 index 上推进同一逻辑保存点
//   int  current_save_point() const;
//   void rollback_to(int save_point_id);
//   void lock_db();
//   void unlock_db();
//   void recover();                // local: no-op；shm: 从 SHM 重建 heap 对象
//
// 高层：
//   bool add        (object_ptr_type);            // engine 自驱遍历 N 个 key_extractor
//   bool remove_by_id(object_id_type);
//
// ----------------------------------------------------------------------------
// 默认 Engine：local_storage_engine<Object, Allocator, IndexCount>
//   - IndexCount = std::tuple_size_v<IndexDef::indices> + 1（+1 = object_id 索引）
//   - mcbase/db/local_storage_engine.h 提供实现，内部持 N 个 mc::im::transaction
//
// 编译期切换：
//   - MCENGINE_USE_SHM=ON ：mcengine 使用 shm_storage_engine
//   - 普通 mcbase 用户代码继续用 local_storage_engine（默认）即可
// ============================================================================

// ----------------------------------------------------------------------------
// engine_traits：从 Engine 类型读出 raw_iterator
// ----------------------------------------------------------------------------
template <typename Engine, typename = void>
struct engine_traits {
    using raw_iterator = typename Engine::raw_iterator;
};

template <typename Engine>
using engine_raw_iterator_t = typename engine_traits<Engine>::raw_iterator;

// ----------------------------------------------------------------------------
// has_engine_clear / has_engine_recover
//   detection helpers，供 index<>::clear() 等少数路径使用
// ----------------------------------------------------------------------------
template <typename T, typename = void>
struct has_engine_clear : std::false_type {};

template <typename T>
struct has_engine_clear<T, std::void_t<decltype(std::declval<T>().clear())>> : std::true_type {};

template <typename T>
inline constexpr bool has_engine_clear_v = has_engine_clear<T>::value;

template <typename T, typename = void>
struct has_engine_recover : std::false_type {};

template <typename T>
struct has_engine_recover<T, std::void_t<decltype(std::declval<T>().recover())>> : std::true_type {};

template <typename T>
inline constexpr bool has_engine_recover_v = has_engine_recover<T>::value;

// ----------------------------------------------------------------------------
// derive_per_index_alloc
//
// table 在为每个 index 派生分配器时调用此重载点：
//   - 默认 std::allocator<char> 等：忽略 index_id / table_name，原样返回
//   - shm 后端在 shm_storage_engine.h 中重载，附加 ".idx<N>" 作为命名后缀
// ----------------------------------------------------------------------------
template <typename Alloc>
Alloc derive_per_index_alloc(const Alloc& alloc, std::size_t /*index_id*/,
                             mc::string_view /*table_name*/)
{
    return alloc;
}

} // namespace mc::db

#endif // MC_DATABASE_STORAGE_ENGINE_H
