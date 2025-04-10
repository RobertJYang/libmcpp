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

#ifndef MC_DATABASE_TABLE_OP_H
#define MC_DATABASE_TABLE_OP_H

#include <mc/db/transaction.h>
#include <mc/im/ref_ptr.h>
#include <memory>
#include <type_traits>
#include <vector>

namespace mc::db {

/**
 * 表操作类型
 */
enum class table_op_type {
    add,    // 添加操作
    update, // 更新操作
    remove  // 删除操作
};

/**
 * 表操作资源基类
 * @tparam TableType 表类型
 */
template <typename TableType>
class table_op_resource : public db_resource {
public:
    using table_type      = TableType;
    using object_type     = typename table_type::object_type;
    using object_ptr_type = typename table_type::object_ptr_type;

    /**
     * 构造函数
     * @param table_id 表ID
     * @param table 表引用
     */
    table_op_resource(uint32_t table_id, uint32_t object_id, table_type& table, int32_t sp,
                      table_op_type op_type)
        : m_table_id(table_id), m_object_id(object_id), m_table(table), m_savepoint_id(sp),
          m_op_type(op_type) {
    }

    virtual ~table_op_resource() = default;

    table_op_type get_op_type() const {
        return m_op_type;
    }

    /**
     * 获取表ID
     * @return 表ID
     */
    uint32_t get_table_id() const {
        return m_table_id;
    }

    // 获取资源ID
    // 资源ID = 表ID << 32 | 对象ID，目前这样做是足够了，每个表最多支持
    // std::numeric_limits<uint32_t>::max() 个对象
    uint64_t resource_id() const override {
        return (uint64_t(m_table_id) << 32) | m_object_id;
    }

    /**
     * 提交操作
     * @return 成功返回true，失败返回false
     */
    bool commit() override {
        m_table.commit();
        return true;
    }

    /**
     * 回滚操作
     * @return 成功返回true，失败返回false
     */
    bool rollback() override {
        m_table.rollback_to(m_savepoint_id);
        return true;
    }

    bool merge(const db_resource& other) override {
        if (!this->m_is_valid) {
            return false;
        }

        return do_merge(reinterpret_cast<const table_op_resource<TableType>&>(other));
    }

    virtual bool do_merge(const table_op_resource<TableType>& other) = 0;

    uint32_t      m_table_id;
    uint32_t      m_object_id;
    table_type&   m_table;
    int32_t       m_savepoint_id;
    table_op_type m_op_type;

    object_ptr_type m_old_obj_ptr;
    object_ptr_type m_new_obj_ptr;
};

/**
 * 表添加操作资源
 * @tparam TableType 表类型
 */
template <typename TableType>
class table_add_resource final : public table_op_resource<TableType> {
public:
    using base_type = table_op_resource<TableType>;
    using typename base_type::object_ptr_type;
    using typename base_type::object_type;
    using typename base_type::table_type;

    /**
     * 构造函数
     * @param table_id 表ID
     * @param table 表引用
     * @param obj_ptr 添加的对象指针
     */
    table_add_resource(uint32_t table_id, table_type& table, object_ptr_type obj_ptr, int32_t sp)
        : base_type(table_id, obj_ptr->get_object_id(), table, sp, table_op_type::add) {
        this->m_new_obj_ptr = std::move(obj_ptr);
    }

    bool do_merge(const base_type& other) override {
        if (!this->m_is_valid) {
            return false;
        }

        MC_ASSERT(other.resource_id() == this->resource_id(), "资源ID不匹配");
        MC_ASSERT(other.m_op_type != table_op_type::add, "不允许增加后再增加");

        if (other.get_op_type() == table_op_type::update) {
            this->m_new_obj_ptr = other.m_new_obj_ptr;
            return true;
        } else if (other.get_op_type() == table_op_type::remove) {
            this->m_is_valid = false;
            return true;
        }
        return false;
    }
};

/**
 * 表更新操作资源
 * @tparam TableType 表类型
 */
template <typename TableType>
class table_update_resource final : public table_op_resource<TableType> {
public:
    using base_type = table_op_resource<TableType>;
    using typename base_type::object_ptr_type;
    using typename base_type::object_type;
    using typename base_type::table_type;

    /**
     * 构造函数
     * @param table_id 表ID
     * @param table 表引用
     * @param old_obj_ptr 旧对象指针
     * @param new_obj_ptr 新对象指针
     */
    table_update_resource(uint32_t table_id, table_type& table, const object_ptr_type& old_obj_ptr,
                          object_ptr_type new_obj_ptr, int32_t sp)
        : base_type(table_id, new_obj_ptr->get_object_id(), table, sp, table_op_type::update) {
        this->m_old_obj_ptr = std::move(old_obj_ptr);
        this->m_new_obj_ptr = std::move(new_obj_ptr);
    }

    bool do_merge(const base_type& other) override {
        if (!this->m_is_valid) {
            return false;
        }

        MC_ASSERT(other.resource_id() == this->resource_id(), "资源ID不匹配");
        MC_ASSERT(other.m_op_type != table_op_type::add, "不允许增加后再增加");

        if (other.m_op_type == table_op_type::update) {
            this->m_new_obj_ptr = other.m_new_obj_ptr;
            return true;
        } else if (other.m_op_type == table_op_type::remove) {
            this->m_is_valid = false;
            return true;
        }
        return false;
    }
};

/**
 * 表删除操作资源
 * @tparam TableType 表类型
 */
template <typename TableType>
class table_remove_resource final : public table_op_resource<TableType> {
public:
    using base_type = table_op_resource<TableType>;
    using typename base_type::object_ptr_type;
    using typename base_type::object_type;
    using typename base_type::table_type;

    /**
     * 构造函数（接受对象指针）
     * @param table_id 表ID
     * @param table 表引用
     * @param obj_ptr 删除的对象指针
     */
    table_remove_resource(uint32_t table_id, table_type& table, object_ptr_type obj_ptr, int32_t sp)
        : base_type(table_id, obj_ptr->get_object_id(), table, sp, table_op_type::remove) {
        this->m_old_obj_ptr = std::move(obj_ptr);
    }

    bool do_merge(const base_type& other) override {
        if (!this->m_is_valid) {
            return false;
        }

        MC_ASSERT(other.resource_id() == this->resource_id(), "资源ID不匹配");
        MC_ASSERT(other.m_op_type != table_op_type::update, "不允许删除后再更新");
        MC_ASSERT(other.m_op_type != table_op_type::remove, "不允许删除后再删除");

        if (other.m_op_type == table_op_type::add) {
            // 删除后增加不允许合并
            return false;
        }
        return false;
    }
};

} // namespace mc::db

#endif // MC_DATABASE_TABLE_OP_H