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
 * @file test_variant_comparison.cpp
 * @brief 测试variant比较运算符：
 * - variant间比较：类型和值都需匹配
 * - variant与非variant比较：
 *   - 基础类型：只需值相等
 *   - 字符串：支持string_type和blob_type
 *   - 其他类型：需类型和值都匹配
 */
#include <gtest/gtest.h>
#include <mc/variant.h>
#include "test_variant_helpers.h"
#include <limits>
#include <stdexcept>

namespace mc {
namespace test {

class VariantComparisonTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 在每个测试前执行
    }

    void TearDown() override {
        // 在每个测试后执行
    }
};

/**
 * @brief 测试variant间的比较（要求类型和值都匹配）
 */
TEST_F(VariantComparisonTest, VariantToVariantComparison) {
    // 相同类型和值的比较
    variant v1(42), v2(42);  // 都是int32_type
    ASSERT_EQ(v1, v2) << "相同类型和值的variant应该相等";

    // 不同类型但值相等的比较
    variant v3(42), v4(int64_t(42));  // int32_type vs int64_type
    ASSERT_NE(v3, v4) << "不同类型的variant即使值相等也不应该相等";

    // 浮点数类型比较
    variant v5(3.14), v6(3.14f);  // double vs float->double
    ASSERT_EQ(v5.as<float>(), v6) << "相同类型和值的浮点数variant应该相等";

    // 字符串类型比较
    variant v7("test"), v8(std::string("test"));
    ASSERT_EQ(v7, v8) << "相同内容的字符串variant应该相等";
}

/**
 * @brief 测试variant与基础类型的比较（只需值相等）
 */
TEST_F(VariantComparisonTest, VariantToPrimitiveComparison) {
    // 整数类型比较
    variant v1(int32_t(42));
    ASSERT_EQ(v1, 42) << "variant应该可以与不同类型的整数比较";
    ASSERT_EQ(v1, 42L) << "variant应该可以与int64_t类型比较";
    ASSERT_EQ(42, v1) << "整数应该可以与variant比较";

    // 浮点数类型比较
    variant v2(3.14);
    ASSERT_EQ(v2, 3.14f) << "variant应该可以与float比较";
    ASSERT_EQ(3.14, v2) << "double应该可以与variant比较";

    // 布尔类型比较
    variant v3(true);
    ASSERT_EQ(v3, true) << "variant应该可以与bool比较";
    ASSERT_EQ(true, v3) << "bool应该可以与variant比较";
}

/**
 * @brief 测试variant与字符串的比较（支持string_type和blob_type）
 */
TEST_F(VariantComparisonTest, VariantToStringComparison) {
    std::string test_str = "Hello, World!";
    
    // string_type比较
    variant v1(test_str);
    ASSERT_EQ(v1, test_str) << "string_type的variant应该可以与字符串比较";
    ASSERT_EQ(test_str, v1) << "字符串应该可以与string_type的variant比较";

    // blob_type比较
    blob b;
    b.data.assign(test_str.begin(), test_str.end());
    variant v2(b);
    ASSERT_EQ(v2, test_str) << "blob_type的variant应该可以与字符串比较";
    ASSERT_EQ(test_str, v2) << "字符串应该可以与blob_type的variant比较";

    // 其他类型与字符串比较
    variant v3(42);
    ASSERT_NE(v3, test_str) << "非字符串或blob类型的variant不应该与字符串相等";
}

/**
 * @brief 测试variant与其他类型的比较（需类型和值都匹配）
 */
TEST_F(VariantComparisonTest, VariantToOtherTypesComparison) {
    // 数组类型比较
    variants arr1 = {variant(1), variant(2)};
    variant v1(arr1);
    ASSERT_EQ(v1, arr1) << "variant应该可以与相同内容的数组比较";

    variants arr2 = {variant(1), variant(2.0)};
    ASSERT_NE(v1, arr2) << "variant不应该与不同类型元素的数组相等";

    // 对象类型比较
    mutable_dict dict1;
    dict1["key"] = 42;
    variant v2(dict1);
    ASSERT_EQ(v2, dict1) << "variant应该可以与相同内容的对象比较";

    mutable_dict dict2;
    dict2["key"] = 42.0;
    ASSERT_NE(v2, dict2) << "variant不应该与不同类型值的对象相等";

    // blob类型比较
    blob b1{{1, 2, 3}};
    variant v3(b1);
    ASSERT_EQ(v3, b1) << "variant应该可以与相同内容的blob比较";

    blob b2{{1, 2, 4}};
    ASSERT_NE(v3, b2) << "variant不应该与不同内容的blob相等";
}

} // namespace test
} // namespace mc