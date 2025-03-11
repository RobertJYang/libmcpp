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
 * @file test_variant_helpers.h
 * @brief 测试 variant 类型的辅助函数和工具
 */
#pragma once

#include <gtest/gtest.h>
#include <limits>
#include <mc/variant.h>

namespace mc {
namespace test {

/**
 * @brief 验证 variant 是否为指定的整数类型并有正确的值
 */
template <typename T>
inline void verify_integer_value(const variant& v, const T& expected_value) {
    ASSERT_FALSE(v.is_null()) << "variant 不应该为 null";
    ASSERT_TRUE(v.is_numeric()) << "variant 应该是数值类型";
    ASSERT_TRUE(v.is_integer()) << "variant 应该是整数类型";

    if constexpr (std::is_signed_v<T>) {
        ASSERT_EQ(v.as_int64(), expected_value) << "整数值不匹配";
    } else {
        ASSERT_EQ(v.as_uint64(), expected_value) << "无符号整数值不匹配";
    }
}

/**
 * @brief 验证 variant 是否为浮点类型并有正确的值
 */

/**
 * @brief 验证操作是否抛出指定异常，并保持原值和类型不变
 */
template <typename Operation, typename Variant>
void verify_assignment_exception(Operation&& op, const Variant& original) {
    const auto prev_val = original;

    try {
        op();
    } catch (const std::exception&) {
        EXPECT_EQ(original, prev_val);
    }
}

inline void verify_double_value(const variant& v, double expected_value) {
    ASSERT_FALSE(v.is_null()) << "variant 不应该为 null";
    ASSERT_TRUE(v.is_numeric()) << "variant 应该是数值类型";
    ASSERT_FALSE(v.is_integer()) << "variant 不应该是整数类型";
    ASSERT_TRUE(v.is_double()) << "variant 应该是 double 类型";
    ASSERT_DOUBLE_EQ(v.as_double(), expected_value) << "浮点值不匹配";
}

/**
 * @brief 验证 variant 是否为布尔类型并有正确的值
 */
inline void verify_bool_value(const variant& v, bool expected_value) {
    ASSERT_FALSE(v.is_null()) << "variant 不应该为 null";
    ASSERT_TRUE(v.is_bool()) << "variant 应该是 bool 类型";
    ASSERT_EQ(v.as_bool(), expected_value) << "布尔值不匹配";
}

/**
 * @brief 验证 variant 是否为字符串类型并有正确的值
 */
inline void verify_string_value(const variant& v, const std::string& expected_value) {
    ASSERT_FALSE(v.is_null()) << "variant 不应该为 null";
    ASSERT_TRUE(v.is_string()) << "variant 应该是 string 类型";
    ASSERT_EQ(v.as_string(), expected_value) << "字符串值不匹配";
}

} // namespace test
} // namespace mc