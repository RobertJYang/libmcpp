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

#include <mc/dict.h>
#include <mc/string.h>
#include <mc/variant/variant_base.h>
#include <mc/variant/io.h>

namespace mc {

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
    case type_id::extension_type: {
        if (auto extension = v.as_extension()) {
            return os << extension->get_type_name();
        }
        return os;
    }
    default:
        // 未知类型或未处理的类型输出为 "unknown_type"
        return os << "unknown_type";
    }
}

// 显式实例化
template MC_API std::ostream& operator<< <variant_config<>>(std::ostream& os, const variant_base<variant_config<>>& v);

} // namespace mc