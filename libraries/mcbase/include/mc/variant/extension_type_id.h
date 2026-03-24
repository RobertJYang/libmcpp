/*
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
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
 * @file extension_type_id.h
 * @brief variant extension 的类型ID系统
 *
 * 为 extension 提供一个类型系统，实现：
 * 1. 快速类型判断（O(1)，无需 dynamic_cast）
 * 2. 类型特征查询（是否可遍历、是否支持索引等）
 * 3. 类型安全的转换
 */
#ifndef MC_VARIANT_EXTENSION_TYPE_ID_H
#define MC_VARIANT_EXTENSION_TYPE_ID_H

#include <cstdint>
#include <string_view>
#include <type_traits>

#include <mc/pretty_name.h>

namespace mc {

/**
 * @brief extension 类型ID
 *
 * 使用 uint32_t 表示类型ID：
 * - 0: 保留，表示未知类型
 * - 1-1000: 系统保留类型（如 array, dict 等）
 * - 1001+: 用户自定义类型
 */
using extension_type_id_t = uint32_t;

/**
 * @brief extension 类型特征标志位
 */
enum class extension_type_trait : uint32_t {
    none           = 0,
    iterable       = 1 << 0, // 可遍历（支持 visit）
    indexable      = 1 << 1, // 支持索引访问
    key_value      = 1 << 2, // 键值对容器
    ref_accessible = 1 << 3, // 支持零拷贝引用访问
    arithmetic     = 1 << 4, // 支持算术运算
    comparable     = 1 << 5, // 支持比较运算
};

constexpr extension_type_trait operator|(extension_type_trait a, extension_type_trait b)
{
    return static_cast<extension_type_trait>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

constexpr extension_type_trait operator&(extension_type_trait a, extension_type_trait b)
{
    return static_cast<extension_type_trait>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

constexpr bool has_trait(extension_type_trait traits, extension_type_trait trait)
{
    return (static_cast<uint32_t>(traits) & static_cast<uint32_t>(trait)) != 0;
}

/**
 * @brief extension 类型信息
 */
struct extension_type_info {
    extension_type_id_t  id;     // 类型ID
    mc::string_view     name;   // 类型名称
    extension_type_trait traits; // 类型特征

    constexpr extension_type_info()
        : id(0), name("unknown", 7), traits(extension_type_trait::none)
    {}

    constexpr extension_type_info(extension_type_id_t id_, mc::string_view name_,
                                  extension_type_trait traits_ = extension_type_trait::none)
        : id(id_), name(name_), traits(traits_)
    {}

    // 查询类型特征
    constexpr bool is_iterable() const
    {
        return has_trait(traits, extension_type_trait::iterable);
    }
    constexpr bool is_indexable() const
    {
        return has_trait(traits, extension_type_trait::indexable);
    }
    constexpr bool is_key_value() const
    {
        return has_trait(traits, extension_type_trait::key_value);
    }
    constexpr bool is_ref_accessible() const
    {
        return has_trait(traits, extension_type_trait::ref_accessible);
    }
    constexpr bool is_arithmetic() const
    {
        return has_trait(traits, extension_type_trait::arithmetic);
    }
    constexpr bool is_comparable() const
    {
        return has_trait(traits, extension_type_trait::comparable);
    }
};

/**
 * @brief 系统保留的 extension 类型ID
 */
namespace extension_type_ids {
constexpr extension_type_id_t unknown     = 0;
constexpr extension_type_id_t typed_array = 1; // mc::array<T> (T != variant)
constexpr extension_type_id_t typed_dict  = 2; // mc::dict 的强类型版本
// 1000 以下为系统保留
constexpr extension_type_id_t user_defined_start = 1000;
} // namespace extension_type_ids

/**
 * @brief extension 类型注册器（用于自动分配类型ID）
 */
class extension_type_registry {
public:
    // 获取下一个可用的用户类型ID
    static extension_type_id_t next_user_type_id()
    {
        static extension_type_id_t next_id = extension_type_ids::user_defined_start;
        return next_id++;
    }
};

/**
 * @brief 为特定类型生成唯一的类型ID
 *
 * 使用模板特化和静态变量确保每个类型只分配一次ID
 */
template <typename T>
struct extension_type_id_generator {
    static extension_type_id_t get()
    {
        static const extension_type_id_t id = extension_type_registry::next_user_type_id();
        return id;
    }
};

/**
 * @brief 获取 extension 类型的类型信息
 *
 * 默认实现：自动生成类型ID，使用 pretty_name 作为类型名
 * 可以通过模板特化为特定类型提供自定义的类型信息
 */
template <typename T>
struct extension_type_info_traits {
    static extension_type_info get()
    {
        return extension_type_info(extension_type_id_generator<T>::get(), mc::pretty_name<T>(),
                                   extension_type_trait::none);
    }
};

} // namespace mc

#endif // MC_VARIANT_EXTENSION_TYPE_ID_H
