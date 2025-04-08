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

#ifndef MC_DATABASE_OBJECT_H
#define MC_DATABASE_OBJECT_H

#include <cstdint>
#include <mc/database/query/proto_query.h>
#include <mc/im/ref_base.h>
#include <mc/im/ref_ptr.h>
#include <mc/reflect/reflect_metadata.h>
#include <memory>
#include <utility>

namespace mc::database {

/**
 * 数据库对象基类，提供ID和引用计数管理
 * @tparam ObjectType 具体对象类型
 * @tparam ObjectIdType ID类型
 * @tparam Allocator 分配器类型
 */
template <typename ObjectType, typename Allocator = std::allocator<ObjectType>,
          typename ObjectIdType = uint32_t>
class object_base : public mc::im::ref_base<ObjectType> {
public:
    using object_id_type = ObjectIdType;
    using alloc_type     = Allocator;

    /**
     * 默认构造函数
     */
    object_base(object_id_type id = 0, const alloc_type& alloc = alloc_type())
        : m_object_id(id), m_alloc(alloc) {
    }

    virtual ~object_base() = default;

    /**
     * 获取对象ID
     * @return 对象ID
     */
    object_id_type get_object_id() const {
        return m_object_id;
    }

    /**
     * 设置对象ID
     * @param id 对象ID
     */
    void set_object_id(object_id_type id) {
        m_object_id = id;
    }

    /**
     * 检查对象ID是否有效
     * @return 如果ID不为0则返回true
     */
    bool has_valid_id() const {
        return m_object_id != 0;
    }

    /**
     * 创建字段引用，通过指定字段名
     * @param name 字段名
     * @return 字段引用对象，用于构建查询条件
     */
    static query::dsl::query_expr<boost::proto::terminal<query::dsl::field_ref>::type>
    field(std::string_view name) {
        return query::dsl::field(name);
    }

    /**
     * 创建字段引用，通过成员指针自动获取字段名
     * @tparam KeyType 字段类型
     * @param member 成员指针
     * @return 字段引用对象，用于构建查询条件
     */
    template <typename KeyType>
    static query::dsl::query_expr<boost::proto::terminal<query::dsl::field_ref>::type>
    field(KeyType ObjectType::* member) {
        std::string_view name;
        if constexpr (mc::reflect::is_reflectable<ObjectType>()) {
            name = mc::reflect::get_member_name<ObjectType>(member);
        }
        return query::dsl::field(name);
    }

    /**
     * 创建对象并返回引用计数指针
     * @tparam Args 构造函数参数类型
     * @param args 构造函数参数
     * @return 指向新创建对象的引用计数指针
     */
    template <typename... Args>
    static mc::im::ref_ptr<ObjectType> create(Args&&... args) {
        return mc::im::make_ref<ObjectType>(std::forward<Args>(args)...);
    }

    /**
     * 使用自定义分配器创建对象
     * @tparam Args 构造函数参数类型
     * @param alloc 分配器
     * @param args 构造函数参数
     * @return 指向新创建对象的引用计数指针
     */
    template <typename... Args>
    static mc::im::ref_ptr<ObjectType> create_with_allocator(const Allocator& alloc,
                                                             Args&&... args) {
        return mc::im::allocate_ref<ObjectType>(alloc, std::forward<Args>(args)...);
    }

protected:
    object_id_type m_object_id;
    alloc_type     m_alloc;

    friend class mc::im::ref_ptr<ObjectType>;
};

} // namespace mc::database

#endif // MC_DATABASE_OBJECT_H