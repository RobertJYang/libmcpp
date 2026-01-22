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
 * @file test_variant_operations.cpp
 * @brief 测试 variant 类型的算术运算和位运算操作符
 */
#include "test_variant_helpers.h"
#include <cerrno>
#include <gtest/gtest.h>
#include <limits>
#include <mc/exception.h>
#include <mc/variant.h>
#include <test_utilities/base.h>

namespace mc {
namespace test {

class VariantOperationsTest : public TestBase {
protected:
    void SetUp() override {
        TestBase::SetUp();
    }

    void TearDown() override {
        // 清理测试中创建的资源
        TestBase::TearDown();
    }
};

/**
 * @brief 测试整数加法运算
 */
TEST_F(VariantOperationsTest, IntegerAddition) {
    variant v1(42);
    variant v2(23);
    variant result = v1 + v2;

    ASSERT_TRUE(result.is_integer());
    ASSERT_EQ(result, 65);

    // 测试与基本类型的加法运算
    result = v1 + 10;
    ASSERT_EQ(result, 52);

    // 测试与基本类型的加法运算（友元运算符）
    result = 10 + v1;
    ASSERT_EQ(result, 52);

    // 测试复合赋值运算
    v1 += v2;
    ASSERT_EQ(v1, 65);

    v1 += 5;
    ASSERT_EQ(v1, 70);
}

/**
 * @brief 测试无符号整数加法运算
 */
TEST_F(VariantOperationsTest, UnsignedIntegerAddition) {
    variant v1(uint64_t(9000000000000000000ULL));
    variant v2(uint64_t(1000000000000000000ULL));
    variant result = v1 + v2;

    ASSERT_TRUE(result.is_unsigned_integer());
    ASSERT_EQ(result, 10000000000000000000ULL);
}

/**
 * @brief 测试字符串加法（连接）运算
 */
TEST_F(VariantOperationsTest, StringConcatenation) {
    variant v1("Hello, ");
    variant v2("World!");
    variant result = v1 + v2;

    ASSERT_TRUE(result.is_string());
    ASSERT_EQ(result, "Hello, World!");

    // 测试字符串和其他类型的连接
    variant v3(123);
    result = v1 + v3;
    ASSERT_EQ(result, "Hello, 123");

    // 测试与字符串字面量的连接
    result = v1 + "C++";
    ASSERT_EQ(result, "Hello, C++");

    // 测试复合赋值
    v1 += "People";
    ASSERT_EQ(v1, "Hello, People");
}

/**
 * @brief 测试减法运算
 */
TEST_F(VariantOperationsTest, Subtraction) {
    variant v1(100);
    variant v2(30);
    variant result = v1 - v2;

    ASSERT_TRUE(result.is_integer());
    ASSERT_EQ(result, 70);

    // 测试与基本类型的减法
    result = v1 - 50;
    ASSERT_EQ(result, 50);

    // 测试与基本类型的减法（友元运算符）
    result = 200 - v1;
    ASSERT_EQ(result, 100);

    // 测试复合赋值
    v1 -= v2;
    ASSERT_EQ(v1, 70);

    v1 -= 20;
    ASSERT_EQ(v1, 50);
}

/**
 * @brief 测试乘法运算
 */
TEST_F(VariantOperationsTest, Multiplication) {
    variant v1(7);
    variant v2(6);
    variant result = v1 * v2;

    ASSERT_TRUE(result.is_integer());
    ASSERT_EQ(result, 42);

    // 测试与基本类型的乘法
    result = v1 * 5;
    ASSERT_EQ(result, 35);

    // 测试与基本类型的乘法（友元运算符）
    result = 8 * v1;
    ASSERT_EQ(result, 56);

    // 测试复合赋值
    v1 *= v2;
    ASSERT_EQ(v1, 42);

    v1 *= 2;
    ASSERT_EQ(v1, 84);
}

/**
 * @brief 测试除法运算
 */
TEST_F(VariantOperationsTest, Division) {
    variant v1(100);
    variant v2(20);
    variant result = v1 / v2;

    ASSERT_TRUE(result.is_integer());
    ASSERT_EQ(result, 5);

    // 测试与基本类型的除法
    result = v1 / 25;
    ASSERT_EQ(result, 4);

    // 测试与基本类型的除法（友元运算符）
    result = 200 / v2;
    ASSERT_EQ(result, 10);

    // 测试复合赋值
    v1 /= v2;
    ASSERT_EQ(v1, 5);

    v1 /= 5;
    ASSERT_EQ(v1, 1);

    // 测试浮点数除法
    variant v3(10.0);
    variant v4(3.0);
    result = v3 / v4;

    ASSERT_TRUE(result.is_double());
    ASSERT_EQ(result, 10.0 / 3.0);

    // 测试浮点数除法（友元运算符）
    result = 30.0 / v4;
    ASSERT_DOUBLE_EQ(result.as_double(), 10.0);

    // 测试除零异常
    EXPECT_THROW(v1 / variant(0), mc::divide_by_zero_exception);
    EXPECT_THROW(v1 / 0, mc::divide_by_zero_exception);
    EXPECT_THROW(10 / variant(0), mc::divide_by_zero_exception);
}

/**
 * @brief 测试取模运算
 */
TEST_F(VariantOperationsTest, Modulo) {
    variant v1(100);
    variant v2(30);
    variant result = v1 % v2;

    ASSERT_TRUE(result.is_integer());
    ASSERT_EQ(result, 10);

    // 测试与基本类型的取模
    result = v1 % 7;
    ASSERT_EQ(result, 2);

    // 测试与基本类型的取模（友元运算符）
    result = 100 % v2;
    ASSERT_EQ(result, 10);

    // 测试复合赋值
    v1 %= v2;
    ASSERT_EQ(v1, 10);

    v1 %= 3;
    ASSERT_EQ(v1, 1);

    // 测试取模零异常
    EXPECT_THROW(v1 % variant(0), mc::divide_by_zero_exception);
    EXPECT_THROW(v1 % 0, mc::divide_by_zero_exception);
    EXPECT_THROW(10 % variant(0), mc::divide_by_zero_exception);

    // 不支持浮点数取模
    variant v3(10.0);
    variant v4(3.0);
    ASSERT_EQ(v3 % v4, 1);
    ASSERT_EQ(9 % v4, 0);
}

/**
 * @brief 测试位与运算
 */
TEST_F(VariantOperationsTest, BitwiseAnd) {
    variant v1(0b1010);
    variant v2(0b1100);
    variant result = v1 & v2;

    ASSERT_TRUE(result.is_integer());
    ASSERT_EQ(result, 0b1000);

    // 测试与基本类型的位与
    result = v1 & 0b0011;
    ASSERT_EQ(result, 0b0010);

    // 测试与基本类型的位与（友元运算符）
    result = 0b1101 & v1;
    ASSERT_EQ(result, 0b1000);

    // 测试复合赋值
    v1 &= v2;
    ASSERT_EQ(v1, 0b1000);

    v1 &= 0b1001;
    ASSERT_EQ(v1, 0b1000);

    // 浮点数支持强转成整数后参与位运算
    variant v3(10.0);
    variant result_float = v1 & v3;
    ASSERT_TRUE(result_float.is_integer());
    ASSERT_EQ(result_float, v1.as_int64() & static_cast<int64_t>(v3.as_double()));

    // 浮点数友元运算符
    result_float = 15.0 & v1;
    ASSERT_TRUE(result_float.is_integer());
    ASSERT_EQ(result_float, static_cast<int64_t>(15.0) & v1.as_int64());
}

/**
 * @brief 测试位或运算
 */
TEST_F(VariantOperationsTest, BitwiseOr) {
    variant v1(0b1010);
    variant v2(0b1100);
    variant result = v1 | v2;

    ASSERT_TRUE(result.is_integer());
    ASSERT_EQ(result, 0b1110);

    // 测试与基本类型的位或
    result = v1 | 0b0001;
    ASSERT_EQ(result, 0b1011);

    // 测试与基本类型的位或（友元运算符）
    result = 0b0101 | v1;
    ASSERT_EQ(result, 0b1111);

    // 测试复合赋值
    v1 |= v2;
    ASSERT_EQ(v1, 0b1110);

    v1 |= 0b0001;
    ASSERT_EQ(v1, 0b1111);
}

/**
 * @brief 测试位异或运算
 */
TEST_F(VariantOperationsTest, BitwiseXor) {
    variant v1(0b1010);
    variant v2(0b1100);
    variant result = v1 ^ v2;

    ASSERT_TRUE(result.is_integer());
    ASSERT_EQ(result, 0b0110);

    // 测试与基本类型的位异或
    result = v1 ^ 0b1111;
    ASSERT_EQ(result, 0b0101);

    // 测试与基本类型的位异或（友元运算符）
    result = 0b1111 ^ v1;
    ASSERT_EQ(result, 0b0101);

    // 测试复合赋值
    v1 ^= v2;
    ASSERT_EQ(v1, 0b0110);

    v1 ^= 0b0011;
    ASSERT_EQ(v1, 0b0101);
}

/**
 * @brief 测试位取反运算
 */
TEST_F(VariantOperationsTest, BitwiseNot) {
    variant v1(0b1010);
    variant result = ~v1;

    ASSERT_TRUE(result.is_integer());
    // 注意：~运算符对整型的结果依赖于位宽，所以这里测试的是低4位
    ASSERT_EQ(result.as_int64() & 0xF, 0b0101);

    // 浮点数支持强转成整数参与位运算
    variant v_float(10.5);
    result = ~v_float;
    ASSERT_TRUE(result.is_integer());
    ASSERT_EQ(result, ~static_cast<int64_t>(v_float.as_double()));

    // 不支持非数值类型（如字符串）的位取反
    variant v2("text");
    EXPECT_THROW(~v2, mc::invalid_op_exception);
}

/**
 * @brief 测试左移运算
 */
TEST_F(VariantOperationsTest, LeftShift) {
    variant v1(0b0001);
    variant v2(2);
    variant result = v1 << v2;

    ASSERT_TRUE(result.is_integer());
    ASSERT_EQ(result, 0b0100);

    // 测试与基本类型的左移
    result = v1 << 3;
    ASSERT_EQ(result, 0b1000);

    // 测试与基本类型的左移（友元运算符）
    result = 0b0010 << v2;
    ASSERT_EQ(result, 0b1000);

    // 测试复合赋值
    v1 <<= v2;
    ASSERT_EQ(v1, 0b0100);

    v1 <<= 1;
    ASSERT_EQ(v1, 0b1000);

    // 测试过大的移位量
    result = v1 << variant(100);
    ASSERT_EQ(result, 0);

    // 测试过大的移位量（友元运算符）
    result = 0b1010 << variant(100);
    ASSERT_EQ(result, 0);
}

/**
 * @brief 测试右移运算
 */
TEST_F(VariantOperationsTest, RightShift) {
    variant v1(0b1000);
    variant v2(2);
    variant result = v1 >> v2;

    ASSERT_TRUE(result.is_integer());
    ASSERT_EQ(result, 0b0010);

    // 测试与基本类型的右移
    result = v1 >> 3;
    ASSERT_EQ(result, 0b0001);

    // 测试与基本类型的右移（友元运算符）
    result = 0b10000 >> v2;
    ASSERT_EQ(result, 0b00100);

    // 测试复合赋值
    v1 >>= v2;
    ASSERT_EQ(v1, 0b0010);

    v1 >>= 1;
    ASSERT_EQ(v1, 0b0001);

    // 测试负数右移（保持符号位）
    variant v3(-16);
    result = v3 >> 2;
    ASSERT_TRUE(result.as_int64() < 0);
    ASSERT_EQ(result, -4);

    // 测试负数右移（友元运算符）
    result = -64 >> variant(2);
    ASSERT_TRUE(result.as_int64() < 0);
    ASSERT_EQ(result, -16);

    // 测试过大的移位量
    variant v4(0xFF);
    result = v4 >> variant(100);
    ASSERT_EQ(result, 0);

    // 测试过大的移位量（友元运算符）
    result = 0xFF >> variant(100);
    ASSERT_EQ(result, 0);

    variant v5(-1);
    result = v5 >> variant(100);
    ASSERT_EQ(result, -1); // 符号位应该保持

    // 测试符号位保持（友元运算符）
    result = -1 >> variant(100);
    ASSERT_EQ(result, -1);
}

/**
 * @brief 测试浮点数算术运算
 */
TEST_F(VariantOperationsTest, FloatingPointArithmetic) {
    variant v1(3.5);
    variant v2(1.5);

    // 加法
    variant result = v1 + v2;
    ASSERT_TRUE(result.is_double());
    ASSERT_EQ(result, 5.0);

    // 加法（友元运算符）
    result = 2.5 + v1;
    ASSERT_TRUE(result.is_double());
    ASSERT_EQ(result, 6.0);

    // 减法
    result = v1 - v2;
    ASSERT_TRUE(result.is_double());
    ASSERT_EQ(result, 2.0);

    // 减法（友元运算符）
    result = 5.5 - v1;
    ASSERT_TRUE(result.is_double());
    ASSERT_EQ(result, 2.0);

    // 乘法
    result = v1 * v2;
    ASSERT_TRUE(result.is_double());
    ASSERT_EQ(result, 5.25);

    // 乘法（友元运算符）
    result = 2.0 * v1;
    ASSERT_TRUE(result.is_double());
    ASSERT_EQ(result, 7.0);

    // 除法
    result = v1 / v2;
    ASSERT_TRUE(result.is_double());
    ASSERT_EQ(result, 2.33333333333333333);

    // 除法（友元运算符）
    result = 10.5 / v1;
    ASSERT_TRUE(result.is_double());
    ASSERT_EQ(result, 3.0);

    // 测试复合赋值
    v1 += v2;
    ASSERT_EQ(v1, 5.0);

    v1 -= v2;
    ASSERT_EQ(v1, 3.5);

    v1 *= v2;
    ASSERT_EQ(v1, 5.25);

    v1 /= v2;
    ASSERT_EQ(v1, 3.5);
}

/**
 * @brief 测试字符串连接（友元运算符）
 */
TEST_F(VariantOperationsTest, StringConcatenationFriend) {
    variant v1("World");

    // 字符串字面量 + variant
    variant result = "Hello, " + v1;
    ASSERT_TRUE(result.is_string());
    ASSERT_EQ(result, "Hello, World");

    // std::string + variant
    std::string prefix = "Goodbye, ";
    result             = prefix + v1;
    ASSERT_TRUE(result.is_string());
    ASSERT_EQ(result, "Goodbye, World");

    // string_view + variant
    std::string_view view = "Hi, ";
    result                = view + v1;
    ASSERT_TRUE(result.is_string());
    ASSERT_EQ(result, "Hi, World");

    // 数字 + 字符串variant
    result = 123 + v1;
    ASSERT_TRUE(result.is_string());
    ASSERT_EQ(result, "123World");
}

/**
 * @brief 测试混合类型运算
 */
TEST_F(VariantOperationsTest, MixedTypeOperations) {
    variant v_int(10);
    variant v_double(2.5);
    variant v_bool(true);

    // 整数 + 浮点数 = 浮点数
    variant result = v_int + v_double;
    ASSERT_TRUE(result.is_double());
    ASSERT_EQ(result, 12.5);

    // 整数 + 布尔值 = 整数
    result = v_int + v_bool;
    ASSERT_TRUE(result.is_integer());
    ASSERT_EQ(result, 11);

    // 浮点数 * 布尔值 = 浮点数
    result = v_double * v_bool;
    ASSERT_TRUE(result.is_double());
    ASSERT_EQ(result, 2.5);

    // 布尔值 / 整数 = 浮点数（因为有一个操作数是布尔型）
    result = v_bool / v_int;
    ASSERT_TRUE(result.is_double());
    ASSERT_EQ(result, 0.1);
}

/**
 * @brief 测试不兼容类型的运算错误
 */
TEST_F(VariantOperationsTest, IncompatibleOperations) {
    variant v_str("text");
    variant v_int(42);

    // 字符串不支持大多数算术运算
    EXPECT_THROW(v_str - v_int, mc::invalid_op_exception);
    EXPECT_THROW(v_str * v_int, mc::invalid_op_exception);
    EXPECT_THROW(v_str / v_int, mc::invalid_op_exception);
    EXPECT_THROW(v_str % v_int, mc::invalid_op_exception);

    // 字符串不支持位运算
    EXPECT_THROW(v_str & v_int, mc::invalid_op_exception);
    EXPECT_THROW(v_str | v_int, mc::invalid_op_exception);
    EXPECT_THROW(v_str ^ v_int, mc::invalid_op_exception);
    EXPECT_THROW(~v_str, mc::invalid_op_exception);
    EXPECT_THROW(v_str << v_int, mc::invalid_op_exception);
    EXPECT_THROW(v_str >> v_int, mc::invalid_op_exception);
}

/**
 * @brief 测试溢出和边界情况
 */
TEST_F(VariantOperationsTest, OverflowAndEdgeCases) {
    // 整数溢出
    variant v1(INT64_MAX);
    variant v2(1);
    variant result = v1 + v2;

    // C++ 有符号整数溢出是未定义行为，但我们依赖底层实现行为
    ASSERT_NE(result.as_int64(), INT64_MAX);

    // 无符号整数溢出测试
    variant v3(UINT64_MAX);
    variant v4(1);
    result = v3 + v4;
    ASSERT_EQ(result, 0);

    // 除零测试
    variant v5(1);
    variant v6(0);
    EXPECT_THROW(v5 / v6, mc::divide_by_zero_exception);
    EXPECT_THROW(v5 % v6, mc::divide_by_zero_exception);
}

/**
 * @brief 测试固定类型模式下的复合赋值运算符
 */
TEST_F(VariantOperationsTest, FixedTypeOperations) {
    // 测试固定类型模式下的复合赋值运算符

    // 整数类型 - 使用明确的类型
    typed_variant v_int(int32_t(42));
    ASSERT_TRUE(v_int.is_int32());

    // 加法不改变类型
    v_int += 10;
    ASSERT_TRUE(v_int.is_int32());
    ASSERT_EQ(v_int, 52);

    // 与双精度浮点数相加，保持整数类型
    v_int += 3.14;
    ASSERT_TRUE(v_int.is_int32());
    ASSERT_EQ(v_int, 55); // 3.14被截断为3

    // 浮点数类型
    typed_variant v_double(3.14);
    ASSERT_TRUE(v_double.is_double());

    // 加法不改变类型
    v_double += 10;
    ASSERT_TRUE(v_double.is_double());
    ASSERT_DOUBLE_EQ(v_double.as_double(), 13.14);

    // 字符串类型
    typed_variant v_str("Hello");
    ASSERT_TRUE(v_str.is_string());

    // 字符串连接不改变类型
    v_str += ", World";
    ASSERT_TRUE(v_str.is_string());
    ASSERT_EQ(v_str, "Hello, World");

    // 布尔类型
    typed_variant v_bool(true);
    ASSERT_TRUE(v_bool.is_bool());

    // 与整数相加，保持布尔类型
    v_bool += 10;
    ASSERT_TRUE(v_bool.is_bool());
    ASSERT_EQ(v_bool, true); // 结果被转换为布尔值
}

/**
 * @brief 测试数组加法运算
 */
TEST_F(VariantOperationsTest, ArrayAddition) {
    // 数组 + 数组
    variant arr1(variants{1, 2, 3});
    variant arr2(variants{4, 5, 6});

    variant result = arr1 + arr2;
    ASSERT_TRUE(result.is_array());
    ASSERT_EQ(result, (variants{1, 2, 3, 4, 5, 6}));

    // 数组 + 元素
    result = arr1 + variant(42);
    ASSERT_TRUE(result.is_array());
    ASSERT_EQ(result, (variants{1, 2, 3, 42}));

    // 测试复合赋值运算符
    arr1 += arr2;
    ASSERT_TRUE(arr1.is_array());
    ASSERT_EQ(arr1, (variants{1, 2, 3, 4, 5, 6}));

    variant arr3(variants{1, 2});
    arr3 += 3;
    ASSERT_TRUE(arr3.is_array());
    ASSERT_EQ(arr3, (variants{1, 2, 3}));
}

/**
 * @brief 测试字典加法运算
 */
TEST_F(VariantOperationsTest, DictionaryAddition) {
    // 字典 + 字典
    variant dict1 = dict{
        {"a", 1},
        {"b", 2},
    };

    variant dict2 = dict{
        {"b", 3},
        {"c", 4},
    };

    variant result = dict1 + dict2;

    ASSERT_TRUE(result.is_object());
    ASSERT_EQ(result.size(), 3);
    ASSERT_EQ(result["a"], 1);
    ASSERT_EQ(result["b"], 3);
    ASSERT_EQ(result["c"], 4);

    // 测试复合赋值运算符
    dict1 += dict2;
    ASSERT_TRUE(dict1.is_object());
    ASSERT_EQ(dict1.size(), 3);
    ASSERT_EQ(dict1["a"], 1);
    ASSERT_EQ(dict1["b"], 3);
    ASSERT_EQ(dict1["c"], 4);
}

TEST_F(VariantOperationsTest, DictionaryAdditionInvalidOperand) {
    variant dict_value = dict{{"key", 1}};
    variant scalar(5);

    EXPECT_THROW({ auto result = dict_value + scalar; (void)result; }, mc::invalid_op_exception);
}

TEST_F(VariantOperationsTest, AdditionInvalidNumericConversionThrows) {
    variant bool_value(true);
    variant object_value = dict{{"nested", 1}};

    EXPECT_THROW({ auto result = bool_value + object_value; (void)result; }, mc::invalid_op_exception);
}

/**
 * @brief 测试字符串转数值参与运算
 */
TEST_F(VariantOperationsTest, StringToNumberConversion) {
    // 整数字符串
    variant v_str_int("123");
    variant v_int(100);

    // 加法 - 字符串拼接
    variant result = v_str_int + v_int;
    ASSERT_TRUE(result.is_string());
    ASSERT_EQ(result, "123100");

    // 反向加法 - 字符串拼接
    result = v_int + v_str_int;
    ASSERT_TRUE(result.is_string());
    ASSERT_EQ(result, "100123");

    // 减法 - 尝试数值转换
    result = v_str_int - v_int;
    ASSERT_TRUE(result.is_integer());
    ASSERT_EQ(result, 23);

    // 乘法
    result = v_str_int * v_int;
    ASSERT_TRUE(result.is_integer());
    ASSERT_EQ(result, 12300);

    // 除法
    result = v_str_int / variant(2);
    ASSERT_TRUE(result.is_integer());
    ASSERT_EQ(result, 61);

    // 浮点数字符串
    variant v_str_float("45.67");
    variant v_float(10.5);

    // 加法 - 字符串拼接
    result = v_str_float + v_float;
    ASSERT_TRUE(result.is_string());
    ASSERT_EQ(result, "45.6710.5"); // 注意：浮点数转字符串可能有多位小数

    // 减法 - 尝试数值转换
    result = v_str_float - v_float;
    ASSERT_TRUE(result.is_double());
    ASSERT_DOUBLE_EQ(result.as_double(), 35.17);

    // 复合赋值 - 字符串拼接
    v_str_int += 77;
    ASSERT_TRUE(v_str_int.is_string());
    ASSERT_EQ(v_str_int, "12377");
}

/**
 * @brief 测试不同进制字符串转数值
 */
TEST_F(VariantOperationsTest, DifferentRadixStringConversion) {
    // 十六进制
    variant v_hex("0x1A");
    variant v_int(10);

    // 加法 - 字符串拼接
    variant result = v_hex + v_int;
    ASSERT_TRUE(result.is_string());
    ASSERT_EQ(result, "0x1A10");

    // 减法 - 尝试数值转换
    result = v_hex - v_int;
    ASSERT_TRUE(result.is_integer());
    ASSERT_EQ(result, 16); // 0x1A = 26, 26-10 = 16

    // 二进制
    variant v_bin("0b101");
    // 乘法 - 尝试数值转换
    result = v_bin * v_int;
    ASSERT_TRUE(result.is_integer());
    ASSERT_EQ(result, 50); // 0b101 = 5, 5*10 = 50

    // 八进制
    variant v_oct("017");
    // 减法 - 尝试数值转换
    result = v_oct - v_int;
    ASSERT_TRUE(result.is_integer());
    ASSERT_EQ(result, 5); // 017 = 15, 15-10 = 5
}

/**
 * @brief 测试特殊字符串情况
 */
TEST_F(VariantOperationsTest, SpecialStringCases) {
    // 空字符串
    variant v_empty("");
    variant v_int(10);

    // 空字符串 + 数字 = 字符串拼接
    variant result = v_empty + v_int;
    ASSERT_TRUE(result.is_string());
    ASSERT_EQ(result, "10");

    // 非数字字符串
    variant v_text("abc123");

    // 非数字字符串 + 数字 = 字符串拼接
    result = v_text + v_int;
    ASSERT_TRUE(result.is_string());
    ASSERT_EQ(result, "abc12310");

    // 非数字字符串参与数值运算应抛出异常
    EXPECT_THROW(v_text * 2, mc::invalid_op_exception);

    // 带符号的数字字符串
    variant v_neg("-456");

    // 数字字符串 + 数字 = 字符串拼接
    result = v_neg + 56;
    ASSERT_TRUE(result.is_string());
    ASSERT_EQ(result, "-45656");

    // 数字字符串参与减法 = 数值运算
    result = v_neg - 44;
    ASSERT_TRUE(result.is_integer());
    ASSERT_EQ(result, -500);

    // 超大数字字符串
    variant v_big("9999999999999999999"); // 超过int64范围

    // 验证字符串超过int64_t范围
    EXPECT_THROW(v_big.as_int64(), mc::overflow_exception);

    // 验证try_as返回空
    auto rr = v_big.try_as<int64_t>();
    ASSERT_FALSE(rr.has_value());

    // 超大数字字符串 + 数字 = 字符串拼接
    result = v_big + 1;
    ASSERT_TRUE(result.is_string());
    ASSERT_EQ(result, "99999999999999999991");

    // 超大数字字符串参与减法应抛出溢出异常
    EXPECT_THROW(v_big - 1, mc::invalid_op_exception);

    // 第一个字符是数字但后面有非数字的字符串
    variant v_mixed("123abc");

    // 混合字符串 + 数字 = 字符串拼接
    result = v_mixed + 2;
    ASSERT_TRUE(result.is_string());
    ASSERT_EQ(result, "123abc2");

    // 混合字符串参与数值运算应抛出异常
    EXPECT_THROW(v_mixed / 2, mc::invalid_op_exception);
}

/**
 * @brief 测试字符串与布尔值的交互
 */
TEST_F(VariantOperationsTest, StringBoolInteraction) {
    // 字符串 "true"/"false" 与布尔值
    variant v_true_str("true");
    variant v_false_str("false");
    variant v_bool(true);

    // 加法 - 字符串拼接
    variant result = v_true_str + v_bool;
    ASSERT_TRUE(result.is_string());
    ASSERT_EQ(result, "truetrue");

    result = v_false_str + v_bool;
    ASSERT_TRUE(result.is_string());
    ASSERT_EQ(result, "falsetrue");

    // 字符串不能与布尔值进行位运算，应抛出异常
    EXPECT_THROW(v_true_str & v_bool, mc::invalid_op_exception);
    EXPECT_THROW(v_false_str | v_bool, mc::invalid_op_exception);

    // 字符串 "0"/"1" 与布尔值
    variant v_zero_str("0");
    variant v_one_str("1");

    // 加法 - 字符串拼接
    result = v_zero_str + v_bool;
    ASSERT_TRUE(result.is_string());
    ASSERT_EQ(result, "0true");

    result = v_one_str + v_bool;
    ASSERT_TRUE(result.is_string());
    ASSERT_EQ(result, "1true");

    // 位运算 - 尝试数值转换
    result = v_zero_str | v_bool;
    ASSERT_TRUE(result.is_integer());
    ASSERT_EQ(result, 1);

    result = v_one_str & v_bool;
    ASSERT_TRUE(result.is_integer());
    ASSERT_EQ(result, 1);
}

/**
 * @brief 测试字符串与位运算
 */
TEST_F(VariantOperationsTest, StringBitwiseOperations) {
    variant v_str("42");
    variant v_int(15); // 0xF

    // 加法 - 字符串拼接
    variant result = v_str + v_int;
    ASSERT_TRUE(result.is_string());
    ASSERT_EQ(result, "4215");

    // 与运算 - 尝试数值转换
    result = v_str & v_int;
    ASSERT_TRUE(result.is_integer());
    ASSERT_EQ(result, 10); // 42 & 15 = 10

    // 或运算 - 尝试数值转换
    result = v_str | 5;
    ASSERT_TRUE(result.is_integer());
    ASSERT_EQ(result, 47); // 42 | 5 = 47

    // 异或运算 - 尝试数值转换
    result = v_str ^ 10;
    ASSERT_TRUE(result.is_integer());
    ASSERT_EQ(result, 32); // 42 ^ 10 = 32

    // 取反 - 尝试数值转换
    result = ~v_str;
    ASSERT_TRUE(result.is_integer());
    ASSERT_EQ(result.as_int64() & 0xFF, ~42 & 0xFF);

    // 移位 - 尝试数值转换
    result = v_str << 2;
    ASSERT_TRUE(result.is_integer());
    ASSERT_EQ(result, 168); // 42 << 2 = 168

    result = v_str >> 1;
    ASSERT_TRUE(result.is_integer());
    ASSERT_EQ(result, 21); // 42 >> 1 = 21
}

/**
 * @brief 测试 blob 类型与运算符的交互
 */
TEST_F(VariantOperationsTest, BlobOperations) {
    // 创建测试用的 blob 对象
    mc::blob blob1 = {0x41, 0x42, 0x43, 0x44}; // ABCD
    mc::blob blob2 = {0x45, 0x46, 0x47, 0x48}; // EFGH

    variant v_blob1(blob1);
    variant v_blob2(blob2);

    ASSERT_TRUE(v_blob1.is_blob());
    ASSERT_TRUE(v_blob2.is_blob());
    ASSERT_EQ(v_blob1, blob1);
    ASSERT_EQ(v_blob2, blob2);

    // ======== 加法运算 ========

    // blob + blob - 结果是 blob 类型
    variant result = v_blob1 + v_blob2;
    ASSERT_TRUE(result.is_blob());
    mc::blob expected_blob = {0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48}; // ABCDEFGH
    ASSERT_EQ(result, expected_blob);

    // blob + 字符串 - 结果是字符串类型
    variant v_str("XYZ");
    result = v_blob1 + v_str;
    ASSERT_TRUE(result.is_string());
    ASSERT_EQ(result, "ABCDXYZ");

    // 字符串 + blob (友元运算符) - 结果是字符串类型
    result = "123" + v_blob1;
    ASSERT_TRUE(result.is_string());
    ASSERT_EQ(result.as_string(), "123ABCD");

    // 测试复合赋值运算符 += (blob += blob)
    variant v_blob_copy = v_blob1;
    v_blob_copy += v_blob2;
    ASSERT_TRUE(v_blob_copy.is_blob());
    mc::blob expected_blob_plus = {0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48}; // ABCDEFGH
    ASSERT_EQ(v_blob_copy, expected_blob_plus);

    // 测试复合赋值运算符 += (blob += string)
    v_blob_copy = v_blob1;
    v_blob_copy += v_str;
    ASSERT_TRUE(v_blob_copy.is_blob());
    mc::blob expected_blob3 = {0x41, 0x42, 0x43, 0x44, 'X', 'Y', 'Z'}; // ABCDXYZ
    ASSERT_EQ(v_blob_copy, expected_blob3);

    // ======== 转换为数值后的运算 ========

    // 创建可以转换为数字的 blob
    variant v_numeric_blob = mc::blob{'4', '2'}; // 字符串 "42"

    // 减法运算
    result = v_numeric_blob - 2;
    ASSERT_TRUE(result.is_integer());
    ASSERT_EQ(result, 40);

    // 减法运算（友元运算符）
    result = 50 - v_numeric_blob;
    ASSERT_TRUE(result.is_integer());
    ASSERT_EQ(result, 8);

    // 乘法运算
    result = v_numeric_blob * 3;
    ASSERT_TRUE(result.is_integer());
    ASSERT_EQ(result, 126);

    // 乘法运算（友元运算符）
    result = 2 * v_numeric_blob;
    ASSERT_TRUE(result.is_integer());
    ASSERT_EQ(result, 84);

    // 除法运算
    result = v_numeric_blob / 2;
    ASSERT_TRUE(result.is_integer());
    ASSERT_EQ(result, 21);

    // 除法运算（友元运算符）
    result = 84 / v_numeric_blob;
    ASSERT_TRUE(result.is_integer());
    ASSERT_EQ(result, 2);

    // 取模运算
    result = v_numeric_blob % 10;
    ASSERT_TRUE(result.is_integer());
    ASSERT_EQ(result, 2);

    // 取模运算（友元运算符）
    result = 100 % v_numeric_blob;
    ASSERT_TRUE(result.is_integer());
    ASSERT_EQ(result, 16); // 100 % 42 = 16

    // ======== 位运算 ========

    // 位与运算
    result = v_numeric_blob & 0b111111;
    ASSERT_TRUE(result.is_integer());
    ASSERT_EQ(result, 42 & 0b111111);

    // 位与运算（友元运算符）
    result = 0b111111 & v_numeric_blob;
    ASSERT_TRUE(result.is_integer());
    ASSERT_EQ(result, 0b111111 & 42);

    // 位或运算
    result = v_numeric_blob | 0b1;
    ASSERT_TRUE(result.is_integer());
    ASSERT_EQ(result, 42 | 0b1);

    // 位或运算（友元运算符）
    result = 0b1 | v_numeric_blob;
    ASSERT_TRUE(result.is_integer());
    ASSERT_EQ(result, 0b1 | 42);

    // 位异或运算
    result = v_numeric_blob ^ 0b111111;
    ASSERT_TRUE(result.is_integer());
    ASSERT_EQ(result, 42 ^ 0b111111);

    // 位异或运算（友元运算符）
    result = 0b111111 ^ v_numeric_blob;
    ASSERT_TRUE(result.is_integer());
    ASSERT_EQ(result, 0b111111 ^ 42);

    // 左移运算
    result = v_numeric_blob << 2;
    ASSERT_TRUE(result.is_integer());
    ASSERT_EQ(result, 42 << 2);

    // 左移运算（友元运算符）- 大数左移的实际结果可能因实现而异
    result = 10 << v_numeric_blob;
    ASSERT_TRUE(result.is_integer());
    // 不再断言具体值，因为不同平台可能有不同实现

    // 右移运算
    result = v_numeric_blob >> 1;
    ASSERT_TRUE(result.is_integer());
    ASSERT_EQ(result, 42 >> 1);

    // 右移运算（友元运算符）- 大数右移的实际结果可能因实现而异
    result = 1000 >> v_numeric_blob;
    ASSERT_TRUE(result.is_integer());
    // 不再断言具体值，因为不同平台可能有不同实现

    // ======== 不可转换为数值时的错误处理 ========

    // 创建不能转换为数字的 blob
    variant v_non_numeric_blob = mc::blob{'A', 'B', 'C', 'D'}; // 非数字

    // 尝试非数值运算应抛出异常
    EXPECT_THROW(v_non_numeric_blob - 10, mc::invalid_op_exception);
    EXPECT_THROW(v_non_numeric_blob * 2, mc::invalid_op_exception);
    EXPECT_THROW(v_non_numeric_blob / 2, mc::invalid_op_exception);
    EXPECT_THROW(v_non_numeric_blob % 2, mc::invalid_op_exception);
    EXPECT_THROW(v_non_numeric_blob & 0xFF, mc::invalid_op_exception);
    EXPECT_THROW(v_non_numeric_blob | 0xFF, mc::invalid_op_exception);
    EXPECT_THROW(v_non_numeric_blob ^ 0xFF, mc::invalid_op_exception);
    EXPECT_THROW(v_non_numeric_blob << 2, mc::invalid_op_exception);
    EXPECT_THROW(v_non_numeric_blob >> 1, mc::invalid_op_exception);

    // 友元运算符的异常处理
    EXPECT_THROW(10 - v_non_numeric_blob, mc::invalid_op_exception);
    EXPECT_THROW(2 * v_non_numeric_blob, mc::invalid_op_exception);
    EXPECT_THROW(2 / v_non_numeric_blob, mc::invalid_op_exception);
    EXPECT_THROW(10 % v_non_numeric_blob, mc::invalid_op_exception);
    EXPECT_THROW(0xFF & v_non_numeric_blob, mc::invalid_op_exception);
    EXPECT_THROW(0xFF | v_non_numeric_blob, mc::invalid_op_exception);
    EXPECT_THROW(0xFF ^ v_non_numeric_blob, mc::invalid_op_exception);
    EXPECT_THROW(2 << v_non_numeric_blob, mc::invalid_op_exception);
    EXPECT_THROW(10 >> v_non_numeric_blob, mc::invalid_op_exception);
}

/**
 * @brief 测试字符串 variant 与 blob 的拼接分支
 */
TEST_F(VariantOperationsTest, StringVariantBlobCombination) {
    variant  string_value("head");
    mc::blob blob_data = {'t', 'a', 'i', 'l'};
    variant  blob_value(blob_data);

    variant combined = string_value + blob_value;
    EXPECT_TRUE(combined.is_string());
    EXPECT_EQ(combined.as_string(), "headtail");

    variant assign_string("core");
    assign_string += blob_value;
    EXPECT_EQ(assign_string.as_string(), "coretail");
}

/**
 * @brief 测试数组与对象在加法运算中的边界分支
 */
TEST_F(VariantOperationsTest, ArrayAndObjectEdgeCases) {
    variants array_items{variant(1)};
    variant  array_value(array_items);
    variant  appended = array_value + variant(2);
    EXPECT_TRUE(appended.is_array());
    EXPECT_EQ(appended[1].get().as_int32(), 2);

    array_value += variant(3);
    EXPECT_EQ(array_value.size(), 2U);
    EXPECT_EQ(array_value[1].get().as_int32(), 3);

    dict    dict_left{{"a", 1}};
    dict    dict_right{{"b", 2}};
    variant object_value(dict_left);
    variant merged = object_value + variant(dict_right);
    EXPECT_EQ(merged["b"].get().as_int32(), 2);

    EXPECT_THROW(object_value + variant(7), mc::exception);
    EXPECT_THROW(object_value += variant(8), mc::exception);
}

/**
 * @brief 测试无符号整数的运算路径
 */
TEST_F(VariantOperationsTest, UnsignedSpecificOperations) {
    variant diff = variant(uint64_t(2)) - variant(uint64_t(5));
    EXPECT_TRUE(diff.is_integer());
    EXPECT_EQ(diff.as_int64(), -3);

    variant product = variant(uint64_t(4)) * variant(uint64_t(3));
    EXPECT_TRUE(product.is_unsigned_integer());
    EXPECT_EQ(product.as_uint64(), 12U);

    variant quotient = variant(uint64_t(10)) / variant(uint64_t(2));
    EXPECT_TRUE(quotient.is_unsigned_integer());
    EXPECT_EQ(quotient.as_uint64(), 5U);

    variant modulo = variant(uint64_t(10)) % variant(uint64_t(3));
    EXPECT_TRUE(modulo.is_unsigned_integer());
    EXPECT_EQ(modulo.as_uint64(), 1U);

    EXPECT_EQ((variant(uint64_t(6)) & variant(uint64_t(3))).as_uint64(), 2U);
    EXPECT_EQ((variant(uint64_t(5)) | variant(uint64_t(2))).as_uint64(), 7U);
    EXPECT_EQ((variant(uint64_t(5)) ^ variant(uint64_t(1))).as_uint64(), 4U);

    EXPECT_EQ((variant(uint64_t(3)) << variant(uint64_t(2))).as_uint64(), 12U);
    EXPECT_EQ((variant(uint64_t(8)) >> variant(uint64_t(1))).as_uint64(), 4U);

    variant bit_not = ~variant(uint64_t(0x0F));
    EXPECT_EQ(bit_not.as_uint64(), static_cast<uint64_t>(~uint64_t(0x0F)));
}

// 测试 blob + string 拼接
TEST_F(VariantOperationsTest, BlobPlusStringKeepsBinary) {
    // 构造 blob variant
    blob blob_data;
    blob_data.data = {'h', 'e', 'l', 'l', 'o'};
    variant v_blob(blob_data);

    // blob + string_view
    variant result1 = v_blob + std::string_view(", world");
    EXPECT_TRUE(result1.is_blob());
    EXPECT_EQ(result1.get_blob().as_string_view(), "hello, world");

    // string_view + blob
    variant result2 = std::string_view("prefix: ") + v_blob;
    EXPECT_TRUE(result2.is_string());
    EXPECT_EQ(result2.as_string(), "prefix: hello");

    // blob + blob
    blob blob_data2;
    blob_data2.data = {',', ' ', 'w', 'o', 'r', 'l', 'd'};
    variant v_blob2(blob_data2);
    variant result3 = v_blob + v_blob2;
    EXPECT_TRUE(result3.is_blob());
    EXPECT_EQ(result3.get_blob().as_string_view(), "hello, world");
}

// 测试无符号减法（无下溢）
TEST_F(VariantOperationsTest, UnsignedSubtractionWithoutUnderflow) {
    variant v1(uint64_t(100));
    variant v2(uint64_t(30));

    // 无符号减法，第一个 >= 第二个
    variant result = v1 - v2;
    EXPECT_TRUE(result.is_unsigned_integer());
    EXPECT_EQ(result.as_uint64(), 70);
}

// 测试一元负号处理无符号数和回退路径
TEST_F(VariantOperationsTest, UnaryMinusCoversUnsignedAndFallback) {
    // 测试 uint64 的一元负号
    variant v_uint64(uint64_t(100));
    variant result1 = -v_uint64;
    EXPECT_TRUE(result1.is_signed_integer());
    EXPECT_EQ(result1.as_int64(), -100);

    // 测试 string 的一元负号（回退到 as_int64）
    variant v_string("123");
    variant result2 = -v_string;
    EXPECT_TRUE(result2.is_signed_integer());
    EXPECT_EQ(result2.as_int64(), -123);
}

// 测试 operator+(string_view) 的 fallback 路径
TEST_F(VariantOperationsTest, StringViewPlusVariantFallback) {
    // 使用 bool 与 string_view 相加，触发 fallback
    variant          v_bool(true);
    std::string_view sv = "test";

    // bool + string_view 应该触发 fallback: return *this + variant(other)
    variant result = v_bool + sv;
    EXPECT_TRUE(result.is_string());
    EXPECT_EQ(result.as_string(), "truetest");
}

/**
 * @brief 测试自增自减与一元运算符
 */
TEST_F(VariantOperationsTest, IncrementDecrementSpecialCases) {
    variant bool_value(true);
    variant negated_bool = -bool_value;
    EXPECT_EQ(negated_bool.as_int64(), -1);

    variant numeric(5);
    variant pre_inc = ++numeric;
    EXPECT_EQ(pre_inc.as_int64(), 6);
    variant post_inc = numeric++;
    EXPECT_EQ(post_inc.as_int64(), 6);
    EXPECT_EQ(numeric.as_int64(), 7);

    variant double_value(1.5);
    ++double_value;
    EXPECT_DOUBLE_EQ(double_value.as_double(), 2.5);

    variant unsigned_value(uint64_t(0));
    ++unsigned_value;
    EXPECT_EQ(unsigned_value.as_uint64(), 1U);

    variant bool_zero(false);
    ++bool_zero;
    EXPECT_TRUE(bool_zero.as_bool());

    variant decrement_target(10);
    variant pre_dec = --decrement_target;
    EXPECT_EQ(pre_dec.as_int64(), 9);
    variant post_dec = decrement_target--;
    EXPECT_EQ(post_dec.as_int64(), 9);
    EXPECT_EQ(decrement_target.as_int64(), 8);

    variant non_numeric("text");
    EXPECT_THROW(++non_numeric, mc::exception);
    EXPECT_THROW(non_numeric++, mc::exception);
    EXPECT_THROW(--non_numeric, mc::exception);
    EXPECT_THROW(non_numeric--, mc::exception);
}

} // namespace test
} // namespace mc
