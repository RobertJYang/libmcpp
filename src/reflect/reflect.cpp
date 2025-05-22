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
 * @file reflect.cpp
 * @brief 实现反射相关的功能
 */
#include <mc/exception.h>
#include <mc/reflect/metadata_info.h>
#include <mc/reflect/reflect.h>

namespace mc::reflect {

/**
 * @brief 抛出枚举转换异常
 *
 * @param value 整数值
 * @param enum_type 枚举类型名称
 */
void throw_bad_enum_cast(int64_t value, const char* enum_type) {
    MC_THROW(mc::bad_cast_exception, "无法将整数 ${value} 转换为枚举类型 ${enum_type}",
             ("value", value)("enum_type", enum_type));
}

/**
 * @brief 抛出枚举转换异常
 *
 * @param value 字符串键
 * @param enum_type 枚举类型名称
 */
void throw_bad_enum_cast(const char* value, const char* enum_type) {
    MC_THROW(mc::bad_cast_exception, "无法将字符串 ${value} 转换为枚举类型 ${enum_type}",
             ("value", value)("enum_type", enum_type));
}

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

/**
 * @brief 抛出方法参数数量不足异常
 *
 * @param method_name 方法名称
 * @param expect_count 期望的参数数量
 * @param actual_count 实际的参数数量
 */
void throw_method_arg_not_enough(std::string_view method_name, size_t expect_count,
                                 size_t actual_count) {
    MC_THROW(
        mc::bad_function_call_exception,
        "调用方法 ${method_name} 参数不足，需要 ${expect_count} 个，实际提供 ${actual_count} 个",
        ("method_name", method_name)("expect_count", expect_count)("actual_count", actual_count));
}

/**
 * @brief 抛出方法不存在异常
 *
 * @param method_name 方法名称
 */
void throw_method_not_exist(std::string_view method_name) {
    MC_THROW(mc::bad_function_call_exception, "调用不存在的方法 ${method_name}",
             ("method_name", method_name));
}

/**
 * @brief 抛出变体转换异常
 *
 * @param k 变体类型
 * @param e 反射类型
 */
void throw_variant_cast(const char* type, const char* variant_type) {
    MC_THROW(mc::bad_cast_exception, "反射类型 ${type} 不支持从 ${variant_type} 转换",
             ("type", type)("variant_type", variant_type));
}

} // namespace mc::reflect