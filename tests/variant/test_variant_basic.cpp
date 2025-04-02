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
 * @file test_variant_basic.cpp
 * @brief 测试 variant 类型的基本功能和基本数据类型操作
 */
#include "test_variant_helpers.h"
#include <gtest/gtest.h>
#include <limits>
#include <mc/variant.h>
#include <stdexcept>
#include <test_utilities/test_base.h>

namespace mc {
namespace test {

class VariantBasicTest : public TestBase {
protected:
    void SetUp() override {
        TestBase::SetUp();
    }

    void TearDown() override {
        TestBase::TearDown();
    }
};

/**
 * @brief 测试创建空 variant
 */
TEST_F(VariantBasicTest, NullVariant) {
    variant v;
    ASSERT_TRUE(v.is_null()) << "默认构造的 variant 应该是 null";

    variant v2(nullptr);
    ASSERT_TRUE(v2.is_null()) << "从 nullptr 构造的 variant 应该是 null";
}

/**
 * @brief 测试布尔类型 variant
 */
TEST_F(VariantBasicTest, BooleanValues) {
    variant v_true(true);
    verify_bool_value(v_true, true);

    variant v_false(false);
    verify_bool_value(v_false, false);
}

/**
 * @brief 测试整数类型 variant
 */
TEST_F(VariantBasicTest, IntegerValues) {
    // 测试 int32_t
    variant v_int(42);
    verify_integer_value(v_int, 42);
    ASSERT_TRUE(v_int.is_int32()) << "variant 应该是 int32 类型";

    // 测试 int64_t
    variant v_int64(int64_t(9876543210LL));
    verify_integer_value(v_int64, int64_t(9876543210LL));
    ASSERT_TRUE(v_int64.is_int64()) << "variant 应该是 int64 类型";

    // 测试 uint32_t
    variant v_uint(uint32_t(4294967295U));
    verify_integer_value(v_uint, uint32_t(4294967295U));
    ASSERT_TRUE(v_uint.is_uint32()) << "variant 应该是 uint32 类型";

    // 测试 uint64_t
    variant v_uint64(uint64_t(1844674407370955161ULL));
    verify_integer_value(v_uint64, uint64_t(1844674407370955161ULL));
    ASSERT_TRUE(v_uint64.is_uint64()) << "variant 应该是 uint64 类型";
}

/**
 * @brief 测试 8 位和 16 位整数类型
 */
TEST_F(VariantBasicTest, SmallIntegerTypes) {
    // 测试 int8_t
    variant v_int8(int8_t(-128));
    verify_integer_value(v_int8, int8_t(-128));
    ASSERT_TRUE(v_int8.is_int8()) << "variant 应该是 int8 类型";

    // 测试 uint8_t
    variant v_uint8(uint8_t(255));
    verify_integer_value(v_uint8, uint8_t(255));
    ASSERT_TRUE(v_uint8.is_uint8()) << "variant 应该是 uint8 类型";

    // 测试 int16_t
    variant v_int16(int16_t(-32768));
    verify_integer_value(v_int16, int16_t(-32768));
    ASSERT_TRUE(v_int16.is_int16()) << "variant 应该是 int16 类型";

    // 测试 uint16_t
    variant v_uint16(uint16_t(65535));
    verify_integer_value(v_uint16, uint16_t(65535));
    ASSERT_TRUE(v_uint16.is_uint16()) << "variant 应该是 uint16 类型";
}

/**
 * @brief 测试浮点类型 variant
 */
TEST_F(VariantBasicTest, FloatingPointValues) {
    // 测试 double
    variant v_double(3.14159265359);
    verify_double_value(v_double, 3.14159265359);

    // 测试 float（会自动转为 double）
    variant v_float(2.71828f);
    verify_double_value(v_float, 2.71828f);
}

/**
 * @brief 测试字符串类型 variant
 */
TEST_F(VariantBasicTest, StringValues) {
    // 从 std::string 构造
    std::string str = "Hello, World!";
    variant     v_str(str);
    verify_string_value(v_str, str);

    // 从 C 字符串构造
    const char* c_str = "C-style string";
    variant     v_cstr(c_str);
    verify_string_value(v_cstr, c_str);
}

/**
 * @brief 测试整数类型间的相互转换
 */
TEST_F(VariantBasicTest, IntegerTypeConversions) {
    // 从 int32_t 转到其他类型
    variant v(42);

    ASSERT_EQ(v.as<int8_t>(), 42) << "转换到 int8_t 失败";
    ASSERT_EQ(v.as<uint8_t>(), 42) << "转换到 uint8_t 失败";
    ASSERT_EQ(v.as<int16_t>(), 42) << "转换到 int16_t 失败";
    ASSERT_EQ(v.as<uint16_t>(), 42) << "转换到 uint16_t 失败";
    ASSERT_EQ(v.as<int32_t>(), 42) << "转换到 int32_t 失败";
    ASSERT_EQ(v.as<uint32_t>(), 42) << "转换到 uint32_t 失败";
    ASSERT_EQ(v.as<int64_t>(), 42) << "转换到 int64_t 失败";
    ASSERT_EQ(v.as<uint64_t>(), 42) << "转换到 uint64_t 失败";

    // 越界转换应该被截断到目标类型的范围
    variant v_large(1000);
    ASSERT_EQ(v_large.as<int8_t>(), static_cast<int8_t>(1000)) << "超出 int8_t 范围的值应被截断";
    ASSERT_EQ(v_large.as<uint8_t>(), static_cast<uint8_t>(1000)) << "超出 uint8_t 范围的值应被截断";

    // 负值转无符号类型应被解释为对应的大整数
    variant v_negative(-1);
    ASSERT_EQ(v_negative.as<uint8_t>(), static_cast<uint8_t>(-1))
        << "负值转 uint8_t 应被解释为对应的无符号值";
    ASSERT_EQ(v_negative.as<uint16_t>(), static_cast<uint16_t>(-1))
        << "负值转 uint16_t 应被解释为对应的无符号值";
    ASSERT_EQ(v_negative.as<uint32_t>(), static_cast<uint32_t>(-1))
        << "负值转 uint32_t 应被解释为对应的无符号值";
    ASSERT_EQ(v_negative.as<uint64_t>(), static_cast<uint64_t>(-1))
        << "负值转 uint64_t 应被解释为对应的无符号值";
}

/**
 * @brief 测试浮点数和整数之间的转换
 */
TEST_F(VariantBasicTest, DISABLED_FloatIntegerConversions) {
    variant v_int(42);
    EXPECT_DOUBLE_EQ(v_int.as_double(), 42.0) << "int 转换到 double 失败";
}

/**
 * @brief 测试类型不匹配时的异常
 */
TEST_F(VariantBasicTest, TypeMismatchExceptions) {
    variant v_str("string value");
    EXPECT_THROW(v_str.as<int>(), std::runtime_error) << "字符串转 int 应抛出异常";
    EXPECT_THROW(v_str.as<double>(), std::runtime_error) << "字符串转 double 应抛出异常";

    variant v_null;
    EXPECT_THROW(v_null.as<int>(), std::runtime_error) << "null 转 int 应抛出异常";
}

/**
 * @brief 测试 variant 的赋值操作
 */
TEST_F(VariantBasicTest, AssignmentOperations) {
    variant v;

    // 赋值不同类型
    v = 42;
    verify_integer_value(v, 42);

    v = 3.14;
    verify_double_value(v, 3.14);

    v = true;
    verify_bool_value(v, true);

    v = "string value";
    verify_string_value(v, "string value");

    v = nullptr;
    ASSERT_TRUE(v.is_null()) << "赋值 nullptr 后应该是 null";
}

/**
 * @brief 测试 variant 的复制和移动操作
 */
TEST_F(VariantBasicTest, CopyAndMoveOperations) {
    // 复制构造
    variant original(42);
    variant copy(original);
    verify_integer_value(copy, 42);

    // 复制赋值
    variant copy_assign;
    copy_assign = original;
    verify_integer_value(copy_assign, 42);

    // 移动构造
    variant move_src("movable string");
    variant move_dest(std::move(move_src));
    verify_string_value(move_dest, "movable string");

    // 移动赋值
    variant move_assign_src(3.14);
    variant move_assign_dest;
    move_assign_dest = std::move(move_assign_src);
    verify_double_value(move_assign_dest, 3.14);
}

/**
 * @brief 测试 variant 的清除操作
 */
TEST_F(VariantBasicTest, ClearOperation) {
    variant v(42);
    ASSERT_FALSE(v.is_null()) << "初始不应该是 null";

    v.clear();
    ASSERT_TRUE(v.is_null()) << "清除后应该是 null";
}

/**
 * @brief 测试带默认值的as方法
 */
TEST_F(VariantBasicTest, AsWithDefaultValue) {
    // 测试正常转换情况
    variant v_int(42);
    ASSERT_EQ(v_int.as<int>(100), 42) << "正常转换时应返回实际值而非默认值";
    ASSERT_EQ(v_int.as<double>(3.14), 42.0) << "整数转浮点数时应返回实际值而非默认值";

    // 测试类型不匹配时返回默认值
    variant v_str("string value");
    ASSERT_EQ(v_str.as<int>(100), 100) << "字符串转整数失败时应返回默认值";
    ASSERT_EQ(v_str.as<double>(3.14), 3.14) << "字符串转浮点数失败时应返回默认值";
    ASSERT_EQ(v_str.as<bool>(true), true) << "字符串转布尔值失败时应返回默认值";

    // 测试null转换时返回默认值
    variant v_null;
    ASSERT_EQ(v_null.as<int>(100), 100) << "null转整数失败时应返回默认值";
    ASSERT_EQ(v_null.as<double>(3.14), 3.14) << "null转浮点数失败时应返回默认值";
    // 注意：null转字符串会返回"null"而不是抛出异常
    ASSERT_EQ(v_null.as<std::string>("default"), "null") << "null转字符串会返回'null'而不是默认值";
    ASSERT_EQ(v_null.as<bool>(true), false) << "null转布尔值会返回false而不是默认值";

    // 测试不同整数类型间的转换
    variant v_large(1000);
    ASSERT_EQ(v_large.as<int8_t>(50), static_cast<int8_t>(1000))
        << "整数转换到较小类型时应进行截断而非返回默认值";

    // 测试边界情况
    variant v_edge_case(std::numeric_limits<int>::max());
    ASSERT_EQ(v_edge_case.as<int16_t>(0), static_cast<int16_t>(std::numeric_limits<int>::max()))
        << "边界值转换应进行截断而非返回默认值";
}

} // namespace test
} // namespace mc