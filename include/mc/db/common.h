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

#ifndef MC_DATABASE_COMMON_H
#define MC_DATABASE_COMMON_H

#include <mc/core/object.h>
#include <mc/db/query/proto_query.h>

namespace mc::db {

/**
 * 数据库对象ID类型
 */
using object_id_type = mc::core::object_id_type;
using object_base    = mc::core::object_base;
using object_ptr     = mc::ref_ptr<object_base>;

using query::field;

/**
 * 创建字段引用，通过成员指针自动获取字段名
 * @tparam KeyType 字段类型
 * @param member 成员指针
 * @return 字段引用对象，用于构建查询条件
 */
template <typename ObjectType, typename KeyType>
static auto field(KeyType ObjectType::* member) {
    std::string_view name;
    if constexpr (mc::reflect::is_reflectable<ObjectType>()) {
        name = mc::reflect::get_property_name<ObjectType>(member);
    }
    return query::dsl::field(name);
}

} // namespace mc::db

#endif // MC_DATABASE_COMMON_H