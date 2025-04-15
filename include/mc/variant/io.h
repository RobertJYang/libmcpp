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
 * @file io.h
 * @brief 为 variant 类型定义输入输出流操作符
 */
#ifndef MC_VARIANT_IO_H
#define MC_VARIANT_IO_H

#include <ostream>
#include <mc/variant/variant_base.h>
#include <mc/variant/variant_common.h>

namespace mc {

/**
 * @brief 为 type_id 枚举类型实现流输出运算符重载
 * 
 * 允许直接将 type_id 类型输出到流中，无需进行显式类型转换。
 * 输出枚举值对应的整数值。
 * 
 * @param os 输出流
 * @param type 类型ID枚举值
 * @return std::ostream& 输出流引用
 */
inline std::ostream& operator<<(std::ostream& os, const type_id& type) {
    return os << static_cast<int>(type);
}

/**
 * @brief 为 variant 类型实现流输出运算符重载
 * 
 * 此函数允许将任意 variant 对象通过流操作符 (<<) 输出到标准输出流中。
 * 根据 variant 存储的数据类型，输出格式会有所不同：
 * - null: 输出字符串 "null"
 * - 布尔值: 输出 "true" 或 "false"
 * - 整数类型: 统一转换为 int64_t 或 uint64_t 后输出
 * - 浮点数: 输出 double 值
 * - 字符串: 直接输出字符串内容(不包含引号)
 * - 数组/对象: 通过 mc::to_string 转换为字符串后输出
 * - 二进制数据: 输出格式为 "blob[大小]"
 * 
 * @tparam Config variant 配置类型
 * @param os 输出流
 * @param v variant 对象
 * @return std::ostream& 输出流引用，支持链式调用
 */
template <typename Config>
std::ostream& operator<<(std::ostream& os, const variant_base<Config>& v) {
    switch (v.get_type()) {
    case type_id::null_type:
        // 空值类型直接输出字符串 "null"
        return os << "null";
    case type_id::bool_type:
        // 布尔类型根据值输出 "true" 或 "false"
        return os << (v.as_bool() ? "true" : "false");
    case type_id::int8_type:
    case type_id::int16_type:
    case type_id::int32_type:
    case type_id::int64_type:
        // 所有有符号整数类型统一转换为 int64_t 后输出
        return os << v.as_int64();
    case type_id::uint8_type:
    case type_id::uint16_type:
    case type_id::uint32_type:
    case type_id::uint64_type:
        // 所有无符号整数类型统一转换为 uint64_t 后输出
        return os << v.as_uint64();
    case type_id::double_type:
        // 浮点类型直接输出 double 值
        return os << v.as_double();
    case type_id::string_type:
        // 字符串类型直接输出内容(不带引号)
        return os << v.get_string();
    case type_id::array_type:
        // 数组类型通过 mc::to_string 转换为字符串后输出
        return os << mc::to_string(v);
    case type_id::object_type:
        // 对象类型通过 mc::to_string 转换为字符串后输出
        return os << mc::to_string(v);
    case type_id::blob_type:
        // 二进制数据输出为 "blob[大小]" 格式
        os << "blob[" << v.get_blob().data.size() << "]";
        return os;
    default:
        // 未知类型或未处理的类型输出为 "unknown_type"
        return os << "unknown_type";
    }
}

} // namespace mc

#endif // MC_VARIANT_IO_H 