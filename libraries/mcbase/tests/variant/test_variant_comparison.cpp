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
#include "test_variant_helpers.h"
#include <gtest/gtest.h>
#include <limits>
#include <mc/exception.h>
#include <mc/variant.h>
#include <stdexcept>
#include <string_view>
#include <test_utilities/base.h>

namespace mc {
namespace test {

class VariantComparisonTest : public mc::test::TestBase {
protected:
    void SetUp() override
    {
        // 在每个测试前执行
    }

    void TearDown() override
    {
        // 在每个测试后执行
    }
};

/**
 * @brief 测试variant间的比较（要求类型和值都匹配）
 */
TEST_F(VariantComparisonTest, VariantToVariantComparison)
{
    // 相同类型和值的比较
    variant v1(42), v2(42); // 都是int32_type
    ASSERT_EQ(v1, v2) << "相同类型和值的variant应该相等";

    // 不同类型但值相等的比较
    variant v3(42), v4(int64_t(42)); // int32_type vs int64_type
    ASSERT_EQ(v3, v4) << "不同类型的variant值相等就应该相等";

    // 浮点数类型比较
    variant v5(3.14), v6(3.14f); // double vs float->double
    ASSERT_EQ(v5, v6) << "相同类型和值的浮点数variant应该相等";

    // 字符串类型比较
    variant v7("test"), v8(mc::string("test"));
    ASSERT_EQ(v7, v8) << "相同内容的字符串variant应该相等";
}

/**
 * @brief 测试variant之间的比较(不同类型)
 */
TEST_F(VariantComparisonTest, VariantToVariantComparison_DifferentType)
{
    // 数值类型variant之间比较
    variant v1(42);
    variant v2(100);
    variant v3(42.0);

    EXPECT_TRUE(v1 < v2);
    EXPECT_TRUE(v1 <= v2);
    EXPECT_TRUE(v2 > v1);
    EXPECT_TRUE(v2 >= v1);

    // 相同值不同类型比较
    EXPECT_TRUE(v1 == v3); // int和double，但值相等
    EXPECT_FALSE(v1 != v3);

    // 字符串variant之间比较
    variant v4("hello");
    variant v5("world");

    EXPECT_TRUE(v4 < v5);
    EXPECT_TRUE(v4 <= v5);
    EXPECT_TRUE(v5 > v4);
    EXPECT_TRUE(v5 >= v4);

    // 不同类型variant之间比较
    EXPECT_THROW({ bool result = v1 < v4; }, mc::invalid_op_exception);
    EXPECT_THROW({ bool result = v4 < v1; }, mc::invalid_op_exception);
}

/**
 * @brief 测试variant与基础类型的比较（只需值相等）
 */
TEST_F(VariantComparisonTest, VariantToPrimitiveComparison)
{
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
TEST_F(VariantComparisonTest, VariantToStringComparison)
{
    mc::string test_str = "Hello, World!";

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
TEST_F(VariantComparisonTest, VariantToOtherTypesComparison)
{
    // 数组类型比较
    variants arr1 = {1, 2};
    variant  v1(arr1);
    ASSERT_EQ(v1, arr1);

    variants arr2 = {1, 2.0};
    ASSERT_EQ(v1, arr2);

    variants arr3 = {1, 1};
    ASSERT_GT(v1, arr3);

    variants arr4 = {1, 3};
    ASSERT_LT(v1, arr4);

    // 对象类型比较
    dict    dict1 = {{"key", 42}};
    variant v2(dict1);
    ASSERT_EQ(v2, dict1);
    ASSERT_EQ(dict1, v2);

    dict dict2 = {{"key", 42.0}};
    ASSERT_EQ(v2, dict2);
    ASSERT_EQ(dict2, v2);

    // blob类型比较
    blob    b1{{1, 2, 3}};
    variant v3(b1);
    ASSERT_EQ(v3, b1);
    ASSERT_EQ(b1, v3);

    blob b2{{1, 2, 4}};
    ASSERT_NE(v3, b2);
    ASSERT_NE(b2, v3);
}

/**
 * @brief 测试 double 与 NaN 的综合比较场景
 */
TEST_F(VariantComparisonTest, VariantLessEqualWithOtherDoubleNaN)
{
    variant int_value(42);
    variant nan_value(std::numeric_limits<double>::quiet_NaN());

    // other 为 double NaN，覆盖 other.is_double() 分支
    EXPECT_FALSE(int_value <= nan_value);
    EXPECT_FALSE(int_value >= nan_value);

    // 自身为 double NaN，覆盖 is_double() 分支
    EXPECT_FALSE(nan_value <= int_value);
    EXPECT_FALSE(nan_value >= int_value);
}

/**
 * @brief 测试跨类型数值比较与字符串转换
 */
TEST_F(VariantComparisonTest, VariantNumericCrossTypeConversionEquality)
{
    variant double_value(12.0);
    variant int_value(12);
    variant bool_true(true);
    variant bool_false(false);

    // 整数与浮点互等
    EXPECT_TRUE(double_value == int_value);
    EXPECT_TRUE(int_value == double_value);
    EXPECT_FALSE(variant(12.5) == int_value);

    // 浮点数与字符串比较，触发 try_as<double>()
    variant double_string("12");
    EXPECT_TRUE(double_value == double_string);
    EXPECT_TRUE(double_string == double_value);

    // 布尔值与数字、字符串比较
    EXPECT_TRUE(bool_true == variant(1));
    EXPECT_TRUE(bool_false == variant(0));
    EXPECT_TRUE(bool_true == variant("true"));
    EXPECT_TRUE(bool_false == variant("false"));
}

/**
 * @brief 测试无符号整数 variant 的排序关系
 */
TEST_F(VariantComparisonTest, UnsignedVariantOrdering)
{
    variant lhs(uint64_t(1));
    variant rhs(uint64_t(3));

    EXPECT_TRUE(lhs < rhs);
    EXPECT_FALSE(rhs < lhs);
    EXPECT_TRUE(lhs <= rhs);
    EXPECT_TRUE(rhs >= lhs);
}

/**
 * @brief 测试字符串与 double 之间的比较转换逻辑
 */
TEST_F(VariantComparisonTest, StringNumericComparisonsWithDouble)
{
    variant string_value("12.5");
    variant double_value(12.5);
    variant bigger_double(13.0);

    EXPECT_TRUE(string_value == double_value);
    EXPECT_TRUE(string_value < bigger_double);
    EXPECT_TRUE(bigger_double > string_value);
}

/**
 * @brief 测试字符串与布尔值的跨类型比较
 */
TEST_F(VariantComparisonTest, StringToBoolComparison)
{
    variant string_true("true");
    variant string_false("false");
    variant bool_true(true);
    variant bool_false(false);

    EXPECT_TRUE(string_true == bool_true);
    EXPECT_TRUE(string_false == bool_false);
    EXPECT_TRUE(string_false < bool_true);
}

/**
 * @brief 测试 string_view 与数值/布尔/blob 的比较
 */
TEST_F(VariantComparisonTest, VariantStringViewNumericComparisons)
{
    variant double_value(1.25);
    variant bool_value(false);

    const mc::string_view sv250("2.50", 4);
    const mc::string_view sv100("1.00", 4);
    const mc::string_view sv_true("true", 4);
    const mc::string_view sv_zz("zz", 2);
    const mc::string_view sv_aa("aa", 2);

    EXPECT_TRUE(double_value < sv250);
    EXPECT_TRUE(double_value > sv100);

    EXPECT_TRUE(bool_value < sv_true);
    EXPECT_FALSE(bool_value == sv_true);

    mc::blob blob_data;
    blob_data.data = {'b', 'c'};
    variant blob_value(blob_data);
    EXPECT_TRUE(blob_value < sv_zz);
    EXPECT_TRUE(blob_value > sv_aa);
}

/**
 * @brief 测试字符串与 blob 的大小比较
 */
TEST_F(VariantComparisonTest, VariantStringBlobCrossLessComparison)
{
    mc::blob blob_data;
    blob_data.data = {'x', 'y'};
    variant string_value("ab");
    variant blob_value(blob_data);

    EXPECT_TRUE(string_value < blob_value);
    EXPECT_TRUE(blob_value > string_value);
}

/**
 * @brief 测试variant与整数类型的比较
 */
TEST_F(VariantComparisonTest, IntegerVariantComparison)
{
    // int8_t
    variant v_int8(int8_t(42));
    EXPECT_TRUE(v_int8 < int8_t(100));
    EXPECT_TRUE(v_int8 <= int8_t(42));
    EXPECT_TRUE(v_int8 > int8_t(10));
    EXPECT_TRUE(v_int8 >= int8_t(42));
    EXPECT_TRUE(int8_t(10) < v_int8);
    EXPECT_TRUE(int8_t(42) <= v_int8);
    EXPECT_TRUE(int8_t(100) > v_int8);
    EXPECT_TRUE(int8_t(42) >= v_int8);

    // int16_t
    variant v_int16(int16_t(42));
    EXPECT_TRUE(v_int16 < int16_t(100));
    EXPECT_TRUE(v_int16 <= int16_t(42));
    EXPECT_TRUE(v_int16 > int16_t(10));
    EXPECT_TRUE(v_int16 >= int16_t(42));
    EXPECT_TRUE(int16_t(10) < v_int16);
    EXPECT_TRUE(int16_t(42) <= v_int16);
    EXPECT_TRUE(int16_t(100) > v_int16);
    EXPECT_TRUE(int16_t(42) >= v_int16);

    // int32_t
    variant v_int32(int32_t(42));
    EXPECT_TRUE(v_int32 < int32_t(100));
    EXPECT_TRUE(v_int32 <= int32_t(42));
    EXPECT_TRUE(v_int32 > int32_t(10));
    EXPECT_TRUE(v_int32 >= int32_t(42));
    EXPECT_TRUE(int32_t(10) < v_int32);
    EXPECT_TRUE(int32_t(42) <= v_int32);
    EXPECT_TRUE(int32_t(100) > v_int32);
    EXPECT_TRUE(int32_t(42) >= v_int32);

    // int64_t
    variant v_int64(int64_t(42));
    EXPECT_TRUE(v_int64 < int64_t(100));
    EXPECT_TRUE(v_int64 <= int64_t(42));
    EXPECT_TRUE(v_int64 > int64_t(10));
    EXPECT_TRUE(v_int64 >= int64_t(42));
    EXPECT_TRUE(int64_t(10) < v_int64);
    EXPECT_TRUE(int64_t(42) <= v_int64);
    EXPECT_TRUE(int64_t(100) > v_int64);
    EXPECT_TRUE(int64_t(42) >= v_int64);
}

/**
 * @brief 测试variant与无符号整数类型的比较
 */
TEST_F(VariantComparisonTest, UnsignedIntegerVariantComparison)
{
    // uint8_t
    variant v_uint8(uint8_t(42));
    EXPECT_TRUE(v_uint8 < uint8_t(100));
    EXPECT_TRUE(v_uint8 <= uint8_t(42));
    EXPECT_TRUE(v_uint8 > uint8_t(10));
    EXPECT_TRUE(v_uint8 >= uint8_t(42));
    EXPECT_TRUE(uint8_t(10) < v_uint8);
    EXPECT_TRUE(uint8_t(42) <= v_uint8);
    EXPECT_TRUE(uint8_t(100) > v_uint8);
    EXPECT_TRUE(uint8_t(42) >= v_uint8);

    // uint16_t
    variant v_uint16(uint16_t(42));
    EXPECT_TRUE(v_uint16 < uint16_t(100));
    EXPECT_TRUE(v_uint16 <= uint16_t(42));
    EXPECT_TRUE(v_uint16 > uint16_t(10));
    EXPECT_TRUE(v_uint16 >= uint16_t(42));
    EXPECT_TRUE(uint16_t(10) < v_uint16);
    EXPECT_TRUE(uint16_t(42) <= v_uint16);
    EXPECT_TRUE(uint16_t(100) > v_uint16);
    EXPECT_TRUE(uint16_t(42) >= v_uint16);

    // uint32_t
    variant v_uint32(uint32_t(42));
    EXPECT_TRUE(v_uint32 < uint32_t(100));
    EXPECT_TRUE(v_uint32 <= uint32_t(42));
    EXPECT_TRUE(v_uint32 > uint32_t(10));
    EXPECT_TRUE(v_uint32 >= uint32_t(42));
    EXPECT_TRUE(uint32_t(10) < v_uint32);
    EXPECT_TRUE(uint32_t(42) <= v_uint32);
    EXPECT_TRUE(uint32_t(100) > v_uint32);
    EXPECT_TRUE(uint32_t(42) >= v_uint32);

    // uint64_t
    variant v_uint64(uint64_t(42));
    EXPECT_TRUE(v_uint64 < uint64_t(100));
    EXPECT_TRUE(v_uint64 <= uint64_t(42));
    EXPECT_TRUE(v_uint64 > uint64_t(10));
    EXPECT_TRUE(v_uint64 >= uint64_t(42));
    EXPECT_TRUE(uint64_t(10) < v_uint64);
    EXPECT_TRUE(uint64_t(42) <= v_uint64);
    EXPECT_TRUE(uint64_t(100) > v_uint64);
    EXPECT_TRUE(uint64_t(42) >= v_uint64);
}

/**
 * @brief 测试variant与浮点类型的比较
 */
TEST_F(VariantComparisonTest, FloatingPointVariantComparison)
{
    // float
    variant v_float(3.14f);
    EXPECT_TRUE(v_float < 4.0f);
    EXPECT_TRUE(v_float <= 3.14f);
    EXPECT_TRUE(v_float > 1.0f);
    EXPECT_TRUE(v_float >= 3.14f);
    EXPECT_TRUE(1.0f < v_float);
    EXPECT_TRUE(3.14f <= v_float);
    EXPECT_TRUE(4.0f > v_float);
    EXPECT_TRUE(3.14f >= v_float);

    // double
    variant v_double(3.14159);
    EXPECT_TRUE(v_double < 4.0);
    EXPECT_TRUE(v_double <= 3.14159);
    EXPECT_TRUE(v_double > 1.0);
    EXPECT_TRUE(v_double >= 3.14159);
    EXPECT_TRUE(1.0 < v_double);
    EXPECT_TRUE(3.14159 <= v_double);
    EXPECT_TRUE(4.0 > v_double);
    EXPECT_TRUE(3.14159 >= v_double);
}

/**
 * @brief 测试variant与字符串类型的比较
 */
TEST_F(VariantComparisonTest, StringVariantComparison)
{
    // 字符串字面量测试
    variant v_str("hello");
    EXPECT_TRUE(v_str < "world");
    EXPECT_FALSE(v_str > "world");
    EXPECT_TRUE(v_str <= "hello");
    EXPECT_TRUE(v_str >= "hello");
    EXPECT_TRUE(v_str > "abc");
    EXPECT_TRUE(v_str >= "abc");

    // const char*与variant比较
    EXPECT_TRUE("abc" < v_str);
    EXPECT_FALSE("world" < v_str);
    EXPECT_TRUE("hello" <= v_str);
    EXPECT_TRUE("hello" >= v_str);

    // mc::string测试
    mc::string str1 = "hello";
    mc::string str2 = "world";
    mc::string str3 = "abc";

    EXPECT_TRUE(v_str < str2);
    EXPECT_FALSE(v_str > str2);
    EXPECT_TRUE(v_str <= str1);
    EXPECT_TRUE(v_str >= str1);
    EXPECT_TRUE(v_str > str3);
    EXPECT_TRUE(v_str >= str3);

    // mc::string与variant比较
    EXPECT_TRUE(str3 < v_str);
    EXPECT_FALSE(str2 < v_str);
    EXPECT_TRUE(str1 <= v_str);
    EXPECT_TRUE(str1 >= v_str);

    // 空字符串测试
    variant v_empty("");
    EXPECT_TRUE(v_empty < "a");
    EXPECT_TRUE(v_empty <= "");
    EXPECT_TRUE(v_empty >= "");
    EXPECT_FALSE(v_empty > "");

    // 特殊字符测试
    variant v_special("123");
    EXPECT_TRUE(v_special < "124");
    EXPECT_TRUE(v_special > "122");
    EXPECT_TRUE(v_special < "123a");

    // 前缀相等时，还要比较字符串的长度

    const char nullTermStr[] = "123\0";
    EXPECT_TRUE(v_special < mc::string(nullTermStr, 4));
    EXPECT_TRUE(v_special == mc::string(nullTermStr, 3));
}

/**
 * @brief 测试变体和类型转换的异常情况
 */
TEST_F(VariantComparisonTest, TypecastExceptionTest)
{
    // 字符串与数值不能直接比较
    variant v_str("hello");
    variant v_num(42);

    // 直接尝试比较应抛出异常
    EXPECT_THROW({ bool result = v_str < 42; }, mc::invalid_op_exception);
    EXPECT_THROW({ bool result = v_str > 42; }, mc::invalid_op_exception);
    EXPECT_THROW({ bool result = v_str <= 42; }, mc::invalid_op_exception);
    EXPECT_THROW({ bool result = v_str >= 42; }, mc::invalid_op_exception);

    EXPECT_THROW({ bool result = 42 < v_str; }, mc::invalid_op_exception);
    EXPECT_THROW({ bool result = 42 > v_str; }, mc::invalid_op_exception);
    EXPECT_THROW({ bool result = 42 <= v_str; }, mc::invalid_op_exception);
    EXPECT_THROW({ bool result = 42 >= v_str; }, mc::invalid_op_exception);
}

/**
 * @brief 测试边界值情况
 */
TEST_F(VariantComparisonTest, BoundaryValueComparison)
{
    // 最大和最小整数值
    variant v_max(std::numeric_limits<int64_t>::max());
    variant v_min(std::numeric_limits<int64_t>::min());

    // 与数字0的比较
    EXPECT_TRUE(v_min < 0);
    EXPECT_TRUE(v_max > 0);
    EXPECT_TRUE(0 > v_min);
    EXPECT_TRUE(0 < v_max);

    // 浮点数边界
    variant v_inf(std::numeric_limits<double>::infinity());
    variant v_neginf(-std::numeric_limits<double>::infinity());
    variant v_nan(std::numeric_limits<double>::quiet_NaN());

    EXPECT_TRUE(v_neginf < 0.0);
    EXPECT_TRUE(v_inf > 0.0);

    // NaN比较测试
    EXPECT_FALSE(v_nan < 0.0);
    EXPECT_FALSE(v_nan > 0.0);

    // 边界值相等测试
    EXPECT_TRUE(variant(std::numeric_limits<int64_t>::max()) == std::numeric_limits<int64_t>::max());
    EXPECT_TRUE(variant(std::numeric_limits<int64_t>::min()) == std::numeric_limits<int64_t>::min());
    EXPECT_TRUE(variant(std::numeric_limits<uint64_t>::max()) == std::numeric_limits<uint64_t>::max());
}

/**
 * @brief 测试不兼容类型抛出异常的情况
 */
TEST_F(VariantComparisonTest, IncompatibleTypesThrowException)
{
    // null类型与数值比较应抛出异常
    variant v1(nullptr);
    EXPECT_THROW({ bool result = v1 < 10; }, mc::invalid_op_exception);
    EXPECT_THROW({ bool result = v1 > 10; }, mc::invalid_op_exception);
    EXPECT_THROW({ bool result = v1 <= 10; }, mc::invalid_op_exception);
    EXPECT_THROW({ bool result = v1 >= 10; }, mc::invalid_op_exception);

    // 对象类型与数值比较应抛出异常
    dict dict;
    dict["key"] = 42;
    variant v2(dict);
    EXPECT_THROW({ bool result = v2 < 10; }, mc::invalid_op_exception);
    EXPECT_THROW({ bool result = v2 > 10; }, mc::invalid_op_exception);
    EXPECT_THROW({ bool result = v2 <= 10; }, mc::invalid_op_exception);
    EXPECT_THROW({ bool result = v2 >= 10; }, mc::invalid_op_exception);

    // 数组类型与数值比较应抛出异常
    variants arr = {1, 2, 3};
    variant  v3(arr);
    EXPECT_THROW({ bool result = v3 < 10; }, mc::invalid_op_exception);
    EXPECT_THROW({ bool result = v3 > 10; }, mc::invalid_op_exception);
    EXPECT_THROW({ bool result = v3 <= 10; }, mc::invalid_op_exception);
    EXPECT_THROW({ bool result = v3 >= 10; }, mc::invalid_op_exception);
}

/**
 * @brief 测试异常信息中使用pretty_name
 */
TEST_F(VariantComparisonTest, ExceptionMessageUsesPrettyName)
{
    // 创建非数值类型的variant
    variant v(nullptr);

    try {
        bool result = v < 10;
        FAIL() << "应当抛出异常";
    } catch (const mc::invalid_op_exception& e) {
        mc::string error_msg = e.what();
        // 检查异常信息中是否包含操作符
        EXPECT_TRUE(error_msg.find("<") != mc::string::npos);
        // 检查异常信息中是否包含类型名称
        EXPECT_TRUE(error_msg.find("null") != mc::string::npos);
        EXPECT_TRUE(error_msg.find("numeric") != mc::string::npos);
    }

    // 测试另一种操作符
    try {
        bool result = v > 10;
        FAIL() << "应当抛出异常";
    } catch (const mc::invalid_op_exception& e) {
        mc::string error_msg = e.what();
        // 检查异常信息中是否包含操作符
        EXPECT_TRUE(error_msg.find(">") != mc::string::npos);
    }
}

/**
 * @brief 测试数值类型相等和不等操作符
 */
TEST_F(VariantComparisonTest, NumericEqualityOperators)
{
    // 整数相等性测试
    variant v1(42);
    EXPECT_TRUE(v1.as<int>() == 42);
    EXPECT_FALSE(v1.as<int>() != 42);

    // 浮点数相等性测试
    variant v2(3.14159);
    EXPECT_TRUE(v2.as<double>() == 3.14159);
    EXPECT_FALSE(v2.as<double>() != 3.14159);

    // 不同整数类型的比较
    EXPECT_TRUE(variant(42) == int8_t(42));
    EXPECT_TRUE(variant(42) == int16_t(42));
    EXPECT_TRUE(variant(42) == int32_t(42));
    EXPECT_TRUE(variant(42) == int64_t(42));
    EXPECT_TRUE(variant(42) == uint8_t(42));
    EXPECT_TRUE(variant(42) == uint16_t(42));
    EXPECT_TRUE(variant(42) == uint32_t(42));
    EXPECT_TRUE(variant(42) == uint64_t(42));

    // 整数和浮点数比较
    EXPECT_TRUE(variant(42) == 42.0);
    EXPECT_TRUE(variant(42.0) == 42);
}

/**
 * @brief 测试字符串类型相等和不等操作符
 */
TEST_F(VariantComparisonTest, StringEqualityOperators)
{
    // 字符串相等性测试
    variant v3("hello");
    EXPECT_TRUE(v3.as<mc::string>() == "hello");
    EXPECT_FALSE(v3.as<mc::string>() != "hello");

    mc::string str = "hello";
    EXPECT_TRUE(v3.as<mc::string>() == str);
    EXPECT_FALSE(v3.as<mc::string>() != str);

    std::string std_str = "hello";
    EXPECT_TRUE(v3 == std_str);
    EXPECT_FALSE(v3 != std_str);

    std::string_view std_view = "hello";
    EXPECT_TRUE(variant(std_view) == std_view);
}

/**
 * @brief 测试跨类型数值比较
 */
TEST_F(VariantComparisonTest, CrossTypeNumericComparison)
{
    // 有符号与无符号整数比较
    variant v_int(int64_t(-1));
    variant v_uint(uint64_t(1));

    EXPECT_TRUE(v_int < v_uint);
    EXPECT_TRUE(v_uint > v_int);

    // 整数与浮点数比较
    variant v_int2(42);
    variant v_float(42.5);

    EXPECT_TRUE(v_int2 < v_float);
    EXPECT_TRUE(v_float > v_int2);

    // 不同位宽整数比较
    variant v_int8(int8_t(100));
    variant v_int32(int32_t(1000));

    EXPECT_TRUE(v_int8 < v_int32);
    EXPECT_TRUE(v_int32 > v_int8);
}

/**
 * @brief 测试bool类型比较
 */
TEST_F(VariantComparisonTest, BooleanComparison)
{
    variant v_true(true);
    variant v_false(false);

    EXPECT_TRUE(v_false < v_true);
    EXPECT_TRUE(v_true > v_false);
    EXPECT_TRUE(v_true >= v_true);
    EXPECT_TRUE(v_false <= v_false);

    // bool与数值比较
    EXPECT_TRUE(v_false < 1);
    EXPECT_TRUE(v_true == 1);
    EXPECT_TRUE(v_false == 0);
    EXPECT_TRUE(1 == v_true);
    EXPECT_TRUE(0 == v_false);
}

/**
 * @brief 测试variant与字符类型比较
 */
TEST_F(VariantComparisonTest, CharacterComparison)
{
    variant v_char('A');

    EXPECT_TRUE(v_char < 'B');
    EXPECT_TRUE(v_char > '@');
    EXPECT_TRUE(v_char == 65); // ASCII值比较
    EXPECT_TRUE(65 == v_char);

    // 字符与字符串比较应当抛出异常
    EXPECT_THROW({ bool result = v_char < "A"; }, mc::invalid_op_exception);
}

/**
 * @brief 测试variant与string_view的比较
 */
TEST_F(VariantComparisonTest, StringViewComparison)
{
    mc::string_view sv1 = "hello";
    mc::string_view sv2 = "world";
    mc::string_view sv3 = "abc";

    variant v_str("hello");

    // 直接比较
    EXPECT_TRUE(v_str == sv1);
    EXPECT_FALSE(v_str == sv2);
    EXPECT_TRUE(v_str != sv2);
    EXPECT_TRUE(v_str < sv2);
    EXPECT_TRUE(v_str <= sv1);
    EXPECT_TRUE(v_str > sv3);
    EXPECT_TRUE(v_str >= sv1);

    // 反向比较
    EXPECT_TRUE(sv1 == v_str);
    EXPECT_FALSE(sv2 == v_str);
    EXPECT_TRUE(sv2 != v_str);
    EXPECT_TRUE(sv2 > v_str);
    EXPECT_TRUE(sv1 >= v_str);
    EXPECT_TRUE(sv3 < v_str);
    EXPECT_TRUE(sv1 <= v_str);

    // 空视图测试
    mc::string_view empty_sv;
    variant         v_empty("");

    EXPECT_TRUE(v_empty == empty_sv);
    EXPECT_TRUE(empty_sv == v_empty);
    EXPECT_FALSE(v_str == empty_sv);
    EXPECT_TRUE(v_empty < sv1);
    EXPECT_TRUE(empty_sv < v_str);
}

/**
 * @brief 测试复杂嵌套结构的比较
 */
TEST_F(VariantComparisonTest, ComplexNestedStructureComparison)
{
    // 创建深度嵌套的结构
    dict level3_1 = {{"name", "inner"}, {"value", 42}};
    dict level3_2 = {{"name", "inner"}, {"value", 43}};

    dict level2_1 = {{"data", level3_1}, {"index", 1}};
    dict level2_2 = {{"data", level3_2}, {"index", 1}};

    dict level1_1 = {{"nested", level2_1}, {"top", true}};
    dict level1_2 = {{"nested", level2_2}, {"top", true}};

    variant v1(level1_1);
    variant v2(level1_2);

    // 由于内部值不同，两个variant应该不相等
    EXPECT_NE(v1, v2);

    // 使结构完全相同
    level3_2["value"] = 42;
    variant v3(level1_2);
    EXPECT_EQ(v1, v3);

    // 测试包含数组的嵌套结构
    variants arr1 = {1, "string", level3_1};
    variants arr2 = {1, "string", level3_2};

    level1_1["array"] = arr1;
    level1_2["array"] = arr2;

    variant v4(level1_1);
    variant v5(level1_2);
    EXPECT_EQ(v4, v5);

    // 修改内部数组，测试比较结果
    arr2[1]           = "different";
    level1_2["array"] = arr2;
    variant v6(level1_2);
    EXPECT_NE(v4, v6);
}

/**
 * @brief 测试特殊字符串比较
 */
TEST_F(VariantComparisonTest, SpecialStringComparison)
{
    // 包含特殊字符的字符串
    mc::string special_chars = "Special chars: \n\t\r\b\f\\\"\'";
    variant    v_special(special_chars);
    EXPECT_EQ(v_special, special_chars);

    // 包含二进制零的字符串
    const char bin_zero[] = "binary\0zero";
    mc::string bin_zero_str(bin_zero, sizeof(bin_zero) - 1);
    variant    v_bin_zero(bin_zero_str);
    EXPECT_EQ(v_bin_zero, bin_zero_str);

    // 非常长的字符串
    mc::string long_string(10000, 'a');
    variant    v_long(long_string);
    EXPECT_EQ(v_long, long_string);

    // Unicode字符串
    mc::string unicode = "Unicode: 中文 Русский नमस्ते";
    variant    v_unicode(unicode);
    EXPECT_EQ(v_unicode, unicode);
    EXPECT_TRUE(v_unicode < unicode + "a");
    EXPECT_TRUE(v_unicode > unicode.substr(0, unicode.size() - 1));
}

/**
 * @brief 测试字符类型在比较中的自动升级
 */
TEST_F(VariantComparisonTest, CharacterUpgradeInComparison)
{
    variant v_char('A');

    // 与数字比较
    EXPECT_TRUE(v_char == 65);
    EXPECT_TRUE(65 == v_char);
    EXPECT_TRUE(v_char < 66);
    EXPECT_TRUE(64 < v_char);

    // 与其他字符比较
    EXPECT_TRUE(v_char < 'B');
    EXPECT_TRUE('Z' > v_char);

    // 与不同类型的数值比较
    EXPECT_TRUE(v_char == int8_t(65));
    EXPECT_TRUE(v_char == uint8_t(65));
    EXPECT_TRUE(v_char == int16_t(65));
    EXPECT_TRUE(v_char == uint16_t(65));
    EXPECT_TRUE(v_char == int32_t(65));
    EXPECT_TRUE(v_char == uint32_t(65));
    EXPECT_TRUE(v_char == int64_t(65));
    EXPECT_TRUE(v_char == uint64_t(65));

    // 与浮点数比较
    EXPECT_TRUE(v_char == 65.0f);
    EXPECT_TRUE(v_char == 65.0);

    // 异常情况
    EXPECT_THROW({ bool result = v_char < "A"; }, mc::invalid_op_exception);
    EXPECT_THROW({ bool result = v_char > "Z"; }, mc::invalid_op_exception);
}

/**
 * @brief 扩展dict和dict比较测试
 */
TEST_F(VariantComparisonTest, DictComparisonExtended)
{
    // 创建具有相同键值的dict和dict
    dict dict1  = {{"key1", 1}, {"key2", "value"}, {"key3", true}};
    dict mdict1 = {{"key1", 1}, {"key2", "value"}, {"key3", true}};

    variant v_dict(dict1);

    // 测试dict和dict之间的相等性
    EXPECT_TRUE(v_dict == dict1);
    EXPECT_TRUE(v_dict == mdict1);
    EXPECT_TRUE(dict1 == v_dict);
    EXPECT_TRUE(mdict1 == v_dict);

    // 修改dict并测试
    mdict1["key1"] = 2;
    EXPECT_FALSE(v_dict == mdict1);

    // 测试键顺序不同的字典
    dict dict2 = {{"key3", true}, {"key1", 1}, {"key2", "value"}};
    EXPECT_TRUE(v_dict == dict2); // 键的顺序不应该影响相等性

    // 测试嵌套字典
    dict nested1 = {{"inner", dict1}};
    dict nested2 = {{"inner", dict2}};

    variant v_nested1(nested1);
    variant v_nested2(nested2);

    EXPECT_TRUE(v_nested1 == v_nested2);

    // 与数值类型比较时应抛出异常
    variant v_number(42);
    EXPECT_THROW({ bool result = v_dict < v_number; }, mc::invalid_op_exception);
    EXPECT_THROW({ bool result = v_dict > v_number; }, mc::invalid_op_exception);
    EXPECT_THROW({ bool result = v_dict <= v_number; }, mc::invalid_op_exception);
    EXPECT_THROW({ bool result = v_dict >= v_number; }, mc::invalid_op_exception);
    EXPECT_FALSE(v_dict == v_number);
    EXPECT_TRUE(v_dict != v_number);
}

/**
 * @brief 测试NaN值的比较行为
 *
 * 注意：C++标准对NaN的比较定义如下：
 * 1. NaN与任何值（包括它自己）比较（<, >, <=, >=）都返回false
 * 2. NaN与任何值（包括它自己）相等比较（==）返回false
 * 3. NaN与任何值（包括它自己）不等比较（!=）返回true
 */
TEST_F(VariantComparisonTest, NaNComparisonBehavior)
{
    double  nan_value = std::numeric_limits<double>::quiet_NaN();
    variant v_nan(nan_value);
    variant v_number(42.0);

    // NaN与普通数值比较 - variant < 值
    EXPECT_FALSE(v_nan < v_number);
    EXPECT_FALSE(v_nan > v_number);
    EXPECT_FALSE(v_nan <= v_number);
    EXPECT_FALSE(v_nan >= v_number);
    EXPECT_FALSE(v_nan == v_number);
    EXPECT_TRUE(v_nan != v_number);

    // NaN与NaN比较 - variant == variant
    variant v_nan2(nan_value);
    EXPECT_FALSE(v_nan == v_nan2);
    EXPECT_TRUE(v_nan != v_nan2);
    EXPECT_FALSE(v_nan < v_nan2);
    EXPECT_FALSE(v_nan > v_nan2);
    EXPECT_FALSE(v_nan <= v_nan2);
    EXPECT_FALSE(v_nan >= v_nan2);

    // 与常量直接比较 - variant op 常量
    EXPECT_FALSE(v_nan < 0.0);
    EXPECT_FALSE(v_nan > 0.0);
    EXPECT_FALSE(v_nan <= 0.0);
    EXPECT_FALSE(v_nan >= 0.0);
    EXPECT_FALSE(v_nan == 0.0);
    EXPECT_TRUE(v_nan != 0.0);
}

/**
 * @brief 测试NaN值的友元比较函数
 */
TEST_F(VariantComparisonTest, NaNFriendComparisonOperators)
{
    double  nan_value = std::numeric_limits<double>::quiet_NaN();
    variant v_nan(nan_value);

    // 常量在左侧的比较 - 常量 op variant
    EXPECT_FALSE(0.0 < v_nan);
    EXPECT_FALSE(0.0 > v_nan);
    EXPECT_FALSE(0.0 <= v_nan);
    EXPECT_FALSE(0.0 >= v_nan);
    EXPECT_FALSE(0.0 == v_nan);
    EXPECT_TRUE(0.0 != v_nan);

    // 不同类型的数值与NaN比较
    EXPECT_FALSE(42 < v_nan);
    EXPECT_FALSE(42 > v_nan);
    EXPECT_FALSE(42 <= v_nan);
    EXPECT_FALSE(42 >= v_nan);
    EXPECT_FALSE(42 == v_nan);
    EXPECT_TRUE(42 != v_nan);

    // 浮点类型的NaN与variant比较
    EXPECT_FALSE(nan_value < v_nan);
    EXPECT_FALSE(nan_value > v_nan);
    EXPECT_FALSE(nan_value <= v_nan);
    EXPECT_FALSE(nan_value >= v_nan);
    EXPECT_FALSE(nan_value == v_nan);
    EXPECT_TRUE(nan_value != v_nan);
}

/**
 * @brief 测试blob与其他类型的比较
 */
TEST_F(VariantComparisonTest, BlobComparisonOperators)
{
    // 创建不同的blob对象
    blob b1{{1, 2, 3}};
    blob b2{{1, 2, 4}};
    blob b3{{1, 2, 3, 4}};
    blob b4{{1, 2}};

    variant v_blob1(b1);

    // blob与blob比较
    EXPECT_TRUE(v_blob1 == b1);
    EXPECT_FALSE(v_blob1 == b2);
    EXPECT_TRUE(v_blob1 != b2);

    // blob的大小比较
    EXPECT_TRUE(v_blob1 < b2);  // 相同长度，但内容小
    EXPECT_TRUE(v_blob1 < b3);  // 长度更短
    EXPECT_TRUE(v_blob1 > b4);  // 长度更长
    EXPECT_TRUE(v_blob1 <= b1); // 相等
    EXPECT_TRUE(v_blob1 <= b2); // 小于
    EXPECT_TRUE(v_blob1 >= b1); // 相等
    EXPECT_TRUE(v_blob1 >= b4); // 大于

    // 比较不同长度
    variant v_blob3(b3);
    variant v_blob4(b4);
    EXPECT_TRUE(v_blob1 < v_blob3);
    EXPECT_TRUE(v_blob1 > v_blob4);

    // 比较相同长度不同内容
    variant v_blob2(b2);
    EXPECT_TRUE(v_blob1 < v_blob2);
    EXPECT_TRUE(v_blob2 > v_blob1);

    // 与字符串比较
    mc::string str1 = "\x01\x02\x03";
    EXPECT_TRUE(v_blob1 == str1);
    EXPECT_TRUE(str1 == v_blob1);

    mc::string str2 = "\x01\x02\x04";
    EXPECT_TRUE(v_blob1 < str2);
    EXPECT_TRUE(str2 > v_blob1);
}

/**
 * @brief 测试blob的友元比较函数
 */
TEST_F(VariantComparisonTest, BlobFriendComparisonOperators)
{
    blob b1{{1, 2, 3}};
    blob b2{{1, 2, 4}};

    variant v_blob1(b1);

    // blob在左边的比较
    EXPECT_TRUE(b1 == v_blob1);
    EXPECT_FALSE(b2 == v_blob1);
    EXPECT_TRUE(b2 != v_blob1);

    // blob的大小比较 - blob op variant
    EXPECT_TRUE(b1 <= v_blob1);
    EXPECT_TRUE(b1 >= v_blob1);
    EXPECT_FALSE(b1 < v_blob1);
    EXPECT_FALSE(b1 > v_blob1);

    EXPECT_TRUE(b2 > v_blob1);
    EXPECT_TRUE(b2 >= v_blob1);
    EXPECT_FALSE(b2 < v_blob1);
    EXPECT_FALSE(b2 <= v_blob1);

    // 与其他variant比较
    variant v_int(42);
    EXPECT_THROW({ bool result = b1 < v_int; }, mc::invalid_op_exception);
    EXPECT_THROW({ bool result = b1 > v_int; }, mc::invalid_op_exception);
    EXPECT_THROW({ bool result = b1 <= v_int; }, mc::invalid_op_exception);
    EXPECT_THROW({ bool result = b1 >= v_int; }, mc::invalid_op_exception);
    EXPECT_FALSE(b1 == v_int);
    EXPECT_TRUE(b1 != v_int);
}

TEST_F(VariantComparisonTest, NumericStringAndBlobConversions)
{
    variant v_number(123);
    variant v_string("123");
    variant v_blob(mc::blob{'1', '2', '3'});

    EXPECT_TRUE(v_number == v_string);
    EXPECT_TRUE(v_string == v_number);
    EXPECT_TRUE(v_number == v_blob);
    EXPECT_TRUE(v_blob == v_number);

    variant v_signed_negative(-5);
    variant v_signed_str("-5");
    EXPECT_TRUE(v_signed_negative == v_signed_str);

    variant v_unsigned_value(uint64_t(6));
    variant v_unsigned_str("6");
    EXPECT_TRUE(v_unsigned_value == v_unsigned_str);

    variant v_bool(true);
    variant v_bool_str("true");
    EXPECT_TRUE(v_bool == v_bool_str);
    EXPECT_TRUE(v_bool_str == v_bool);
}

TEST_F(VariantComparisonTest, VariantStringViewConversions)
{
    variant v_numeric(5);
    EXPECT_TRUE(v_numeric < mc::string_view("6"));
    EXPECT_TRUE(v_numeric > mc::string_view("4"));

    variant v_blob(mc::blob{'1', '0'});
    EXPECT_TRUE(v_blob == mc::string_view("10"));
    EXPECT_TRUE(v_blob < mc::string_view("11"));

    variant v_string("abc");
    variant v_blob_compare(mc::blob{'a', 'b', 'd'});
    EXPECT_TRUE(v_string < v_blob_compare);
    EXPECT_TRUE(v_blob_compare > v_string);

    variant v_bool(true);
    EXPECT_THROW([&v_bool]() {
        return v_bool < mc::string_view("maybe");
    }(), mc::invalid_op_exception);
}

// 测试数组比较时大小不匹配提前返回
TEST_F(VariantComparisonTest, ArraySizeMismatchReturnsFalse)
{
    variants arr1{1, 2, 3};
    variants arr2{1, 2, 3, 4}; // 大小不同

    variant v1(arr1);
    variant v2(arr2);

    // 大小不同的数组应该不相等
    EXPECT_FALSE(v1 == v2);
}

// 测试 variants 的关系操作符
TEST_F(VariantComparisonTest, VariantsRelOps)
{
    variants arr1{1, 2, 3};
    variants arr2{1, 2, 4};
    variants arr3{1, 2, 3, 4};

    // 测试 operator!=
    EXPECT_TRUE(arr1 != arr2);
    EXPECT_FALSE(arr1 != arr1);

    // 测试 operator<=
    EXPECT_TRUE(arr1 <= arr2); // [1,2,3] < [1,2,4]
    EXPECT_TRUE(arr1 <= arr1); // 相等
    EXPECT_TRUE(arr1 <= arr3); // [1,2,3] < [1,2,3,4]

    // 测试 operator>=
    EXPECT_FALSE(arr1 >= arr2); // [1,2,3] < [1,2,4]
    EXPECT_TRUE(arr1 >= arr1);  // 相等
    EXPECT_FALSE(arr1 >= arr3); // [1,2,3] < [1,2,3,4]
    EXPECT_TRUE(arr3 >= arr1);  // [1,2,3,4] > [1,2,3]
}

} // namespace test
} // namespace mc