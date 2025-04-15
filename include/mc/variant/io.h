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
 * @brief 为 variant 类型实现流输出运算符重载
 * 
 * @tparam Config variant 配置类型
 * @param os 输出流
 * @param v variant 对象
 * @return std::ostream& 输出流引用
 */
template <typename Config>
std::ostream& operator<<(std::ostream& os, const variant_base<Config>& v) {
    switch (v.get_type()) {
    case type_id::null_type:
        return os << "null";
    case type_id::bool_type:
        return os << (v.as_bool() ? "true" : "false");
    case type_id::int8_type:
    case type_id::int16_type:
    case type_id::int32_type:
    case type_id::int64_type:
        return os << v.as_int64();
    case type_id::uint8_type:
    case type_id::uint16_type:
    case type_id::uint32_type:
    case type_id::uint64_type:
        return os << v.as_uint64();
    case type_id::double_type:
        return os << v.as_double();
    case type_id::string_type:
        return os << v.get_string();
    case type_id::array_type:
        return os << mc::to_string(v);
    case type_id::object_type:
        return os << mc::to_string(v);
    case type_id::blob_type:
        os << "blob[" << v.get_blob().data.size() << "]";
        return os;
    default:
        return os << "unknown_type";
    }
}

} // namespace mc

#endif // MC_VARIANT_IO_H 