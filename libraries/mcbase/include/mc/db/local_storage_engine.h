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

#ifndef MC_DATABASE_LOCAL_STORAGE_ENGINE_H
#define MC_DATABASE_LOCAL_STORAGE_ENGINE_H

#include <mc/db/common.h>
#include <mc/db/storage_engine.h>
#include <mc/exception.h>
#include <mc/im/radix_tree.h>
#include <mc/memory.h>

#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

namespace mc::db {

// ============================================================================
// local_storage_engine
//
// local_storage_engine：本地堆存储引擎实现
//
// 内部持 IndexCount 个独立 mc::im::transaction，每个 transaction 驱动一棵
// radix_tree。对外完全符合 storage_engine concept（见 storage_engine.h）。
//
// 事务、保存点、锁均在 table 维度统一管理，避免 per-index 半提交半回滚的不一致。
//
// 模板参数：
//   Object       - 被存对象类型（mc::object_base 派生）
//   IndexCount   - 索引数量（含默认 object_id 索引；= IndexDef 索引数 + 1）
//   Allocator    - 内存分配器，默认 std::allocator<char>
//
// 线程模型：
//   - 引擎自身不加锁；调用方（db::table）以 std::recursive_mutex 兜底
//
// 事务模型：
//   - save_point() 在所有 N 棵 tree 上一次性推进同一逻辑保存点 id
//   - rollback_to(id) 同理
//   - 调用方（db::table）只在最外层管理 savepoint id
// ============================================================================

template <typename Object, std::size_t IndexCount, typename Allocator = std::allocator<char>>
class local_storage_engine {
    static_assert(IndexCount >= 1U, "local_storage_engine 至少需要 1 个 index（object_id）");

public:
    using object_type     = Object;
    using object_ptr_type = mc::shared_ptr<object_type>;
    using leaf_type       = std::optional<object_ptr_type>;
    using allocator_type  = Allocator;

    using tree_config  = mc::im::tree_config<object_ptr_type, allocator_type>;
    using txn_type     = mc::im::transaction<tree_config>;
    using tree_type    = typename txn_type::tree_type;
    using raw_iterator = typename tree_type::iterator;

    static constexpr std::size_t index_count = IndexCount;

    // ----- 构造 / 析构 -----
    explicit local_storage_engine(const allocator_type& alloc      = allocator_type(),
                                  mc::string_view       table_name = mc::string_view())
    {
        for (std::size_t i = 0; i < IndexCount; ++i) {
            m_txns[i] = std::make_unique<txn_type>(derive_per_index_alloc(alloc, i, table_name));
        }
    }

    ~local_storage_engine()                                      = default;
    local_storage_engine(const local_storage_engine&)            = delete;
    local_storage_engine& operator=(const local_storage_engine&) = delete;
    local_storage_engine(local_storage_engine&&)                 = delete;
    local_storage_engine& operator=(local_storage_engine&&)      = delete;

    // ----- per-index 写 -----
    std::pair<leaf_type, bool> insert(std::size_t idx, mc::string_view key, object_ptr_type v)
    {
        return txn(idx).insert(key, std::move(v));
    }

    leaf_type remove(std::size_t idx, mc::string_view key)
    {
        return txn(idx).remove(key);
    }

    void clear(std::size_t idx)
    {
        m_txns[idx] = std::make_unique<txn_type>();
    }

    // ----- per-index 读 -----
    leaf_type get(std::size_t idx, mc::string_view key)
    {
        return _txn_at(idx).get(key);
    }

    raw_iterator find(std::size_t idx, mc::string_view key)
    {
        return _txn_at(idx).root().find(key);
    }

    raw_iterator lower_bound(std::size_t idx, mc::string_view key)
    {
        return _txn_at(idx).root().lower_bound(key);
    }

    raw_iterator upper_bound(std::size_t idx, mc::string_view key)
    {
        return _txn_at(idx).root().upper_bound(key);
    }

    raw_iterator begin(std::size_t idx)
    {
        return _txn_at(idx).root().begin();
    }

    raw_iterator end(std::size_t idx)
    {
        return _txn_at(idx).root().end();
    }

    bool contains(std::size_t idx, mc::string_view key) const
    {
        const auto& tree = _txn_at(idx).root();
        return tree.find(key) != tree.end();
    }

    bool empty(std::size_t idx) const
    {
        return _txn_at(idx).root().empty();
    }

    std::size_t size(std::size_t idx) const
    {
        return _txn_at(idx).root().size();
    }

    // ----- engine-wide -----
    void clear()
    {
        for (auto& t : m_txns) {
            t = std::make_unique<txn_type>();
        }
    }

    void commit()
    {
        for (auto& t : m_txns) {
            t->commit();
        }
    }

    void rollback()
    {
        for (auto& t : m_txns) {
            t->rollback();
        }
    }

    int save_point()
    {
        int sp = -1;
        for (auto& t : m_txns) {
            sp = t->save_point();
        }
        return sp;
    }

    int current_save_point() const
    {
        return m_txns.front()->current_save_point();
    }

    void rollback_to(int save_point_id)
    {
        for (auto& t : m_txns) {
            t->rollback(save_point_id);
        }
    }

    void lock_db()
    {
        for (auto& t : m_txns) {
            t->lock_db();
        }
    }

    void unlock_db()
    {
        for (auto& t : m_txns) {
            t->unlock_db();
        }
    }

    // recover：local 模式数据完全在堆上，进程内一致，无需重建。占位以满足 concept。
    void recover()
    {}

    // ----- 诊断 -----
    txn_type& txn(std::size_t idx)
    {
        return _txn_at(idx);
    }
    const txn_type& txn(std::size_t idx) const
    {
        return _txn_at(idx);
    }

private:
    txn_type& _txn_at(std::size_t idx) const
    {
        if (idx >= IndexCount) {
            MC_THROW(mc::invalid_arg_exception, "local_storage_engine: index_id 越界 idx=${idx}, IndexCount=${n}",
                     ("idx", idx)("n", IndexCount));
        }
        return *m_txns[idx];
    }

    std::array<std::unique_ptr<txn_type>, IndexCount> m_txns{};
};

} // namespace mc::db

#endif // MC_DATABASE_LOCAL_STORAGE_ENGINE_H
