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

/**
 * @file field_accessor.h
 * @brief 定义通用的字段访问器，支持基于反射的字段访问
 */
#ifndef MC_DATABASE_QUERY_FIELD_ACCESSOR_H
#define MC_DATABASE_QUERY_FIELD_ACCESSOR_H

#include <functional>
#include <string_view>

#include <mc/reflect.h>
#include <mc/variant.h>

namespace mc::database::query {

/**
 * 创建基于反射的字段访问器函数
 *
 * 该函数返回一个函数对象，用于根据字段名获取对象的属性值
 * 字段访问使用反射机制
 *
 * @tparam T 对象类型，必须支持反射
 * @param obj 对象引用
 * @return 返回一个函数，接受字段名并返回属性值
 */
template <typename T, typename = std::enable_if_t<mc::reflect::is_reflectable<T>()>>
std::function<mc::variant(std::string_view)> make_field_accessor(const T& obj) {
    return [&obj](std::string_view field) -> mc::variant {
        auto property = mc::reflect::get_property(obj, field);
        if (property.has_value()) {
            return property.value();
        }
        return mc::variant();
    };
}

/**
 * 针对字典对象的特化访问器
 *
 * @param dict 字典对象
 * @return 返回一个函数，接受字段名并返回属性值
 */
inline std::function<mc::variant(std::string_view)> make_field_accessor(const mc::dict& dict) {
    return [&dict](std::string_view field) -> mc::variant {
        if (dict.contains(field)) {
            return dict.at(field);
        }
        return mc::variant();
    };
}

/**
 * 针对可变字典对象的特化访问器
 *
 * @param dict 可变字典对象
 * @return 返回一个函数，接受字段名并返回属性值
 */
inline std::function<mc::variant(std::string_view)>
make_field_accessor(const mc::mutable_dict& dict) {
    return [&dict](std::string_view field) -> mc::variant {
        if (dict.contains(field)) {
            return dict.at(field);
        }
        return mc::variant();
    };
}

} // namespace mc::database::query

#endif // MC_DATABASE_QUERY_FIELD_ACCESSOR_H