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
 * @file variant.cpp
 * @brief 实现 variant.h 中声明的方法
 */
#include <cstring>
#include <limits>
#include <mc/dict.h>
#include <mc/exception.h>
#include <mc/string.h>
#include <mc/variant.h>
#include <mc/variant/variant_base.h>
#include <mc/variant/variant_extension.h>
#include <stdexcept>

namespace mc {

// variant_extension_base 的实现
variant_extension_base::~variant_extension_base() = default;

mc::variant variant_extension_base::get(std::size_t index) const {
    throw std::runtime_error("扩展类型不支持索引访问");
}

void variant_extension_base::set(std::size_t index, const mc::variant& value) {
    throw std::runtime_error("扩展类型不支持索引访问");
}

mc::variant variant_extension_base::get(std::string_view key) const {
    throw std::runtime_error("扩展类型不支持键访问");
}

void variant_extension_base::set(std::string_view key, const mc::variant& value) {
    throw std::runtime_error("扩展类型不支持键访问");
}

namespace detail {

/**
 * @brief 抛出方法参数类型不匹配异常
 *
 * @param method_name 方法名称
 * @param expect_type 期望的参数类型
 * @param actual_type 实际的参数类型
 */
void throw_method_arg_not_match(std::string_view method_name, std::string_view expect_type,
                                std::string_view actual_type) {
    MC_THROW(mc::invalid_arg_exception,
             "调用方法 ${method_name} 参数不匹配，需要 ${expect_type} 类型，实际提供 "
             "${actual_type} 类型",
             ("method_name", method_name)("expect_type", expect_type)("actual_type", actual_type));
}

} // namespace detail

// 获取类型名称
const char* get_type_name_internal(type_id type) {
    static const char* type_names[] = {
        "null",     // null_type
        "int8",     // int8_type
        "uint8",    // uint8_type
        "int16",    // int16_type
        "uint16",   // uint16_type
        "int32",    // int32_type
        "uint32",   // uint32_type
        "int64",    // int64_type
        "uint64",   // uint64_type
        "double",   // double_type
        "bool",     // bool_type
        "string",   // string_type
        "array",    // array_type
        "object",   // object_type
        "blob",     // blob_type
        "extension" // extension_type
    };

    int index = static_cast<int>(type);
    if (index >= 0 && index < static_cast<int>(type_id::max_type)) {
        return type_names[index];
    }

    return "unknown";
}

void throw_type_error(const char* expected_type, type_id actual_type) {
    if (actual_type < type_id::max_type) {
        MC_THROW(mc::invalid_arg_exception, "类型错误：期望${type}，实际为${actual_type}",
                 ("type", expected_type)("actual_type", get_type_name_internal(actual_type)));
    } else {
        MC_THROW(mc::invalid_arg_exception, "类型错误：期望${type}，实际为${actual_type}",
                 ("type", expected_type)("actual_type", static_cast<int>(actual_type)));
    }
}

void throw_unknow_type_error(type_id actual_type) {
    MC_THROW(mc::invalid_arg_exception, "未知类型：${type}",
             ("type", static_cast<int>(actual_type)));
}

void throw_invalid_type_comparison_error(const char* type1, const char* type2, const char* op) {
    MC_THROW(mc::invalid_op_exception, "不支持的类型比较操作: ${type1} ${op} ${type2}",
             ("type1", type1)("op", op)("type2", type2));
}

void throw_invalid_type_operation_error(const char* type1, const char* type2, const char* op) {
    MC_THROW(mc::invalid_op_exception, "无效的类型操作: ${type1} ${op} ${type2}",
             ("type1", type1)("op", op)("type2", type2));
}

void throw_divide_by_zero_exception(const char* msg) {
    MC_THROW(mc::divide_by_zero_exception, "${msg}", ("msg", msg));
}

// 计算字符串的哈希值
size_t calculate_str_hash(std::string_view data) {
    if (data.empty()) {
        return 0;
    }

    return std::hash<std::string_view>()(data);
}

} // namespace mc