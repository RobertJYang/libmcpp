/*
 * Copyright (c) 2024 Huawei Technologies Co., Ltd.
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
#include <mc/db/query/builder.h>
#include <mc/db/query/query.h>
#include <mc/db/transaction.h>
#include <mc/im/radix_tree.h>

namespace mc::db {

class table_base {
public:
    virtual ~table_base() = default;

    virtual uint32_t         get_table_id() const      = 0;
    virtual void             set_table_id(uint32_t id) = 0;
    virtual std::string_view get_table_name() const    = 0;
    virtual bool             empty() const             = 0;
    virtual size_t           size() const              = 0;
    virtual void             clear()                   = 0;

    object_ptr add_object(const mc::dict& var, transaction* txn = nullptr) {
        return do_add_object(var, txn);
    }

    size_t remove_object(const query_builder& condition, transaction* txn = nullptr) {
        return do_remove_object(condition, txn);
    }

    object_ptr find_object(const query_builder& condition) {
        return do_find_object(condition);
    }

    using query_handler = std::function<bool(object_base&)>;
    bool query_object(const query_builder& builder, query_handler&& handler) {
        return do_query_object(builder, std::forward<query_handler>(handler));
    }

    size_t update_object(const query_builder& condition, const mc::dict& values,
                         transaction* txn = nullptr) {
        return do_update_object(condition, values, txn);
    }

    size_t update_object(const query_builder&                  condition,
                         const std::map<std::string, variant>& values, transaction* txn = nullptr) {
        return do_update_object(condition, values, txn);
    }

    mc::signal<void(object_base&)>               on_object_added;
    mc::signal<void(object_base&)>               on_object_removed;
    mc::signal<void(object_base&, object_base&)> on_object_updated;

protected:
    virtual object_ptr do_add_object(const mc::dict& var, transaction* txn)                   = 0;
    virtual size_t     do_remove_object(const query_builder& condition, transaction* txn)     = 0;
    virtual object_ptr do_find_object(const query_builder& condition)                         = 0;
    virtual bool       do_query_object(const query_builder& builder, query_handler&& handler) = 0;

    virtual size_t do_update_object(const query_builder&                  condition,
                                    const std::map<std::string, variant>& values,
                                    transaction*                          txn) = 0;
    virtual size_t do_update_object(const query_builder& condition, const mc::dict& values,
                                    transaction* txn) = 0;

    /**
     * 生成新的对象ID
     * @return 新的对象ID
     */
    object_id_type generate_id() {
        return m_next_id.fetch_add(1, std::memory_order_relaxed);
    }
    static std::atomic<object_id_type> m_next_id; ///< 下一个可用的对象ID
};

template <typename ObjectType, typename Allocator = std::allocator<char>>
class index_base {
public:
    using object_type     = ObjectType;
    using alloc_type      = Allocator;
    using object_ptr_type = mc::ref_ptr<object_type>;
    using tree_config     = mc::im::tree_config<object_ptr_type, alloc_type>;
    using tree_type       = mc::im::radix_tree<tree_config>;
    using raw_iterator    = typename tree_type::iterator;

    virtual ~index_base() = default;

    virtual raw_iterator raw_begin() const = 0;

    raw_iterator raw_end() const {
        return raw_iterator();
    }

    virtual object_ptr_type raw_find(const mc::variant& value)        = 0;
    virtual raw_iterator    raw_lower_bound(const mc::variant& value) = 0;
    virtual raw_iterator    raw_upper_bound(const mc::variant& value) = 0;
    virtual table_base*     get_table() const                         = 0;
};

} // namespace mc::db

#endif // MC_DATABASE_TABLE_BASE_H