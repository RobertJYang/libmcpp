/*
 * Copyright (c) 2024-2026 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#ifndef MC_DATABASE_TABLE_BASE_H
#define MC_DATABASE_TABLE_BASE_H

#include <mc/db/common.h>
#include <mc/db/local_storage_engine.h>
#include <mc/db/storage_engine.h>
#include <mc/db/transaction.h>
#include <mc/signal/signal.h>

namespace mc::db {

class MC_API table_base {
public:
    virtual ~table_base() = default;

    virtual uint32_t        get_table_id() const      = 0;
    virtual void            set_table_id(uint32_t id) = 0;
    virtual mc::string_view get_table_name() const    = 0;
    virtual bool            empty() const             = 0;
    virtual size_t          size() const              = 0;
    virtual void            clear()                   = 0;

    object_ptr add_object(const mc::dict& var, transaction* txn = nullptr)
    {
        return do_add_object(var, txn);
    }

    /**
     * 生成新的对象ID
     * @return 新的对象ID
     */
    object_id_type generate_id();

    mc::signal<void(object_base&)>               on_object_added;
    mc::signal<void(object_base&)>               on_object_removed;
    mc::signal<void(object_base&, object_base&)> on_object_updated;

protected:
    virtual object_ptr do_add_object(const mc::dict& var, transaction* txn) = 0;
};

// index_base：索引对外的虚基类（v3）
//
// Engine 模板参数决定 raw_iterator 的具体类型。默认 = local_storage_engine
// 单棵树形态（与 db::index<> 默认 Engine 对齐，方便 standalone 用法）。
template <typename ObjectType, typename Allocator = std::allocator<char>,
          typename Engine = local_storage_engine<ObjectType, 1U, Allocator>>
class index_base {
public:
    using object_type     = ObjectType;
    using alloc_type      = Allocator;
    using object_ptr_type = mc::shared_ptr<object_type>;
    using engine_type     = Engine;
    using raw_iterator    = engine_raw_iterator_t<Engine>;

    virtual ~index_base() = default;

    virtual raw_iterator raw_begin() const = 0;

    raw_iterator raw_end() const
    {
        return raw_iterator();
    }

    virtual object_ptr_type raw_find(const mc::variant& value)        = 0;
    virtual raw_iterator    raw_lower_bound(const mc::variant& value) = 0;
    virtual raw_iterator    raw_upper_bound(const mc::variant& value) = 0;
    virtual table_base*     get_table() const                         = 0;
};

} // namespace mc::db

#endif // MC_DATABASE_TABLE_BASE_H
