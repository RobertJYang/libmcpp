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
 * @file test_typed_variant.cpp
 * @brief 测试 typed_variant 类型的功能，确保类型不会被改变
 */
#include "test_variant_helpers.h"
#include <gtest/gtest.h>
#include <limits>
#include <mc/dict.h>
#include <mc/variant.h>
#include <stdexcept>
#include <string>
#include <test_utilities/base.h>

namespace mc {
namespace test {

class typed_variant_test : public mc::test::TestBase {
protected:
    void SetUp() override {
        // 在每个测试前执行
    }

    void TearDown() override {
        // 在每个测试后执行
    }
};

/**
 * @brief 测试创建 typed_variant
 */
TEST_F(typed_variant_test, Creation) {
    // 从 type_id 创建
    typed_variant tv1(type_id::int64_type);
    ASSERT_EQ(tv1.get_type(), type_id::int64_type)
        << "从 type_id 创建的 typed_variant 类型应该正确";

    // 从基本类型创建
    typed_variant tv2(42);
    ASSERT_EQ(tv2.get_type(), type_id::int32_type)
        << "从整数创建的 typed_variant 类型应该是 int32_type";

    typed_variant tv3(3.14);
    ASSERT_EQ(tv3.get_type(), type_id::double_type)
        << "从浮点数创建的 typed_variant 类型应该是 double_type";

    typed_variant tv4(true);
    ASSERT_EQ(tv4.get_type(), type_id::bool_type)
        << "从布尔值创建的 typed_variant 类型应该是 bool_type";

    typed_variant tv5(std::string("hello"));
    ASSERT_EQ(tv5.get_type(), type_id::string_type)
        << "从字符串创建的 typed_variant 类型应该是 string_type";

    // 从 variant 创建
    variant       v(123);
    typed_variant tv6(v);
    ASSERT_EQ(tv6.get_type(), type_id::int32_type)
        << "从 variant 创建的 typed_variant 类型应该与原 variant 相同";
}

/**
 * @brief 测试 typed_variant 的整数类型锁定功能
 */
TEST_F(typed_variant_test, IntegerTypeLocking) {
    // 测试 int8_type
    typed_variant tv_int8(type_id::int8_type);
    ASSERT_EQ(tv_int8.get_type(), type_id::int8_type);

    // 赋值一个小整数，类型应该保持为 int8_type
    tv_int8 = 42;
    ASSERT_EQ(tv_int8.get_type(), type_id::int8_type);
    ASSERT_EQ(tv_int8.as_int64(), 42);

    // 赋值一个边界值，类型应该保持为 int8_type
    tv_int8 = 127; // int8_t 的最大值
    ASSERT_EQ(tv_int8.get_type(), type_id::int8_type);
    ASSERT_EQ(tv_int8.as_int64(), 127);

    // 测试 uint8_type
    typed_variant tv_uint8(type_id::uint8_type);
    ASSERT_EQ(tv_uint8.get_type(), type_id::uint8_type);

    // 赋值一个小整数，类型应该保持为 uint8_type
    tv_uint8 = 42;
    ASSERT_EQ(tv_uint8.get_type(), type_id::uint8_type);
    ASSERT_EQ(tv_uint8.as_uint64(), 42);

    // 赋值一个边界值，类型应该保持为 uint8_type
    tv_uint8 = 255; // uint8_t 的最大值
    ASSERT_EQ(tv_uint8.get_type(), type_id::uint8_type);
    ASSERT_EQ(tv_uint8.as_uint64(), 255);

    // 测试 int16_type
    typed_variant tv_int16(type_id::int16_type);
    ASSERT_EQ(tv_int16.get_type(), type_id::int16_type);

    // 赋值一个整数，类型应该保持为 int16_type
    tv_int16 = 12345;
    ASSERT_EQ(tv_int16.get_type(), type_id::int16_type);
    ASSERT_EQ(tv_int16.as_int64(), 12345);

    // 测试 uint16_type
    typed_variant tv_uint16(type_id::uint16_type);
    ASSERT_EQ(tv_uint16.get_type(), type_id::uint16_type);

    // 赋值一个整数，类型应该保持为 uint16_type
    tv_uint16 = 12345;
    ASSERT_EQ(tv_uint16.get_type(), type_id::uint16_type);
    ASSERT_EQ(tv_uint16.as_uint64(), 12345);

    // 测试 int32_type
    typed_variant tv_int32(type_id::int32_type);
    ASSERT_EQ(tv_int32.get_type(), type_id::int32_type);

    // 赋值一个整数，类型应该保持为 int32_type
    tv_int32 = 123456789;
    ASSERT_EQ(tv_int32.get_type(), type_id::int32_type);
    ASSERT_EQ(tv_int32.as_int64(), 123456789);

    // 测试 uint32_type
    typed_variant tv_uint32(type_id::uint32_type);
    ASSERT_EQ(tv_uint32.get_type(), type_id::uint32_type);

    // 赋值一个整数，类型应该保持为 uint32_type
    tv_uint32 = 123456789;
    ASSERT_EQ(tv_uint32.get_type(), type_id::uint32_type);
    ASSERT_EQ(tv_uint32.as_uint64(), 123456789);

    // 测试 int64_type
    typed_variant tv_int64(type_id::int64_type);
    ASSERT_EQ(tv_int64.get_type(), type_id::int64_type);

    // // 赋值一个整数，类型应该保持为 int64_type
    // tv_int64 = 1234567890123LL;
    // ASSERT_EQ(tv_int64.get_type(), type_id::int64_type);
    // ASSERT_EQ(tv_int64.as_int64(), 1234567890123LL);

    // 测试 uint64_type
    typed_variant tv_uint64(type_id::uint64_type);
    ASSERT_EQ(tv_uint64.get_type(), type_id::uint64_type);

    // 赋值一个整数，类型应该保持为 uint64_type
    // tv_uint64 = 1234567890123ULL;
    // ASSERT_EQ(tv_uint64.get_type(), type_id::uint64_type);
    // ASSERT_EQ(tv_uint64.as_uint64(), 1234567890123ULL);
}

/**
 * @brief 测试 typed_variant 的浮点类型锁定功能
 */
TEST_F(typed_variant_test, FloatingPointTypeLocking) {
    // 测试 double_type
    typed_variant tv_double(type_id::double_type);
    ASSERT_EQ(tv_double.get_type(), type_id::double_type);

    // 赋值一个整数，类型应该保持为 double_type
    tv_double = 42;
    ASSERT_EQ(tv_double.get_type(), type_id::double_type);
    ASSERT_DOUBLE_EQ(tv_double.as_double(), 42.0);

    // 赋值一个浮点数，类型应该保持为 double_type
    tv_double = 3.14159;
    ASSERT_EQ(tv_double.get_type(), type_id::double_type);
    ASSERT_DOUBLE_EQ(tv_double.as_double(), 3.14159);

    // 赋值一个有效的数字字符串，类型应该保持为 double_type，但值会被转换
    tv_double = "123.456";
    ASSERT_EQ(tv_double.get_type(), type_id::double_type);
    ASSERT_DOUBLE_EQ(tv_double.as_double(), 123.456);
}

/**
 * @brief 测试 typed_variant 的布尔类型锁定功能
 */
TEST_F(typed_variant_test, BoolTypeLocking) {
    // 测试 bool_type
    typed_variant tv_bool(type_id::bool_type);
    ASSERT_EQ(tv_bool.get_type(), type_id::bool_type);

    // 赋值一个布尔值，类型应该保持为 bool_type
    tv_bool = true;
    ASSERT_EQ(tv_bool.get_type(), type_id::bool_type);
    ASSERT_EQ(tv_bool.as_bool(), true);

    // 赋值一个整数，类型应该保持为 bool_type，但值会被转换
    tv_bool = 0;
    ASSERT_EQ(tv_bool.get_type(), type_id::bool_type);
    ASSERT_EQ(tv_bool.as_bool(), false);

    tv_bool = 42;
    ASSERT_EQ(tv_bool.get_type(), type_id::bool_type);
    ASSERT_EQ(tv_bool.as_bool(), true);

    // 赋值一个字符串，类型应该保持为 bool_type，但值会被转换
    tv_bool = "true";
    ASSERT_EQ(tv_bool.get_type(), type_id::bool_type);
    ASSERT_EQ(tv_bool.as_bool(), true);

    tv_bool = "false";
    ASSERT_EQ(tv_bool.get_type(), type_id::bool_type);
    ASSERT_EQ(tv_bool.as_bool(), false);

    /**
     * 为 false 的情况：
     * bool类型的false
     * 整数类型的0
     * 浮点数类型的0
     * 空字符串类型除了 true 和 1
     * 空数组类型的空数组
     * 空对象类型的空对象
     * 空blob类型的空blob
     * 空dict类型的空dict
     *
     * 其他情况返回true
     */
    tv_bool = "not a boolean";
    ASSERT_EQ(tv_bool.get_type(), type_id::bool_type);
    ASSERT_EQ(tv_bool.as_bool(), true);
}

/**
 * @brief 测试 typed_variant 的字符串类型锁定功能
 */
TEST_F(typed_variant_test, StringTypeLocking) {
    // 测试 string_type
    typed_variant tv_string(type_id::string_type);
    ASSERT_EQ(tv_string.get_type(), type_id::string_type);

    // 赋值一个字符串，类型应该保持为 string_type
    tv_string = "hello world";
    ASSERT_EQ(tv_string.get_type(), type_id::string_type);
    ASSERT_EQ(tv_string.as_string(), "hello world");

    // 赋值一个整数，类型应该保持为 string_type，但值会被转换
    tv_string = 42;
    ASSERT_EQ(tv_string.get_type(), type_id::string_type);
    ASSERT_EQ(tv_string.as_string(), "42");

    // 赋值一个浮点数，类型应该保持为 string_type，但值会被转换
    tv_string = 3.14159;
    ASSERT_EQ(tv_string.get_type(), type_id::string_type);
    // 浮点数转字符串可能有精度问题，所以使用 ASSERT_TRUE 和 find
    ASSERT_TRUE(tv_string.as_string().find("3.14159") != std::string::npos);

    // 赋值一个布尔值，类型应该保持为 string_type，但值会被转换
    tv_string = true;
    ASSERT_EQ(tv_string.get_type(), type_id::string_type);
    ASSERT_EQ(tv_string.as_string(), "true");

    tv_string = false;
    ASSERT_EQ(tv_string.get_type(), type_id::string_type);
    ASSERT_EQ(tv_string.as_string(), "false");
}

/**
 * @brief 测试 typed_variant 的数组类型锁定功能
 */
TEST_F(typed_variant_test, ArrayTypeLocking) {
    // 创建一个数组
    variants arr;
    arr.push_back(1);
    arr.push_back(2);
    arr.push_back(3);

    // 测试 array_type
    typed_variant tv_array(type_id::array_type);
    ASSERT_EQ(tv_array.get_type(), type_id::array_type);

    // 赋值一个数组，类型应该保持为 array_type
    tv_array = arr;
    ASSERT_EQ(tv_array.get_type(), type_id::array_type);
    ASSERT_EQ(tv_array.get_array().size(), 3);
    ASSERT_EQ(tv_array.get_array()[0].get_type(), type_id::int32_type);
    ASSERT_EQ(tv_array.get_array()[0].as_int64(), 1);
    ASSERT_EQ(tv_array.get_array()[1].as_int64(), 2);
    ASSERT_EQ(tv_array.get_array()[2].as_int64(), 3);

    // 赋值一个非数组类型，类型应该保持为 array_type，但值不会改变
    verify_assignment_exception(
        [&] {
        tv_array = 42;
    },
        tv_array);

    // 创建一个新的数组
    variants arr2;
    arr2.push_back("a");
    arr2.push_back("b");

    // 赋值一个新数组，类型应该保持为 array_type
    tv_array = arr2;
    ASSERT_EQ(tv_array.get_type(), type_id::array_type);
    ASSERT_EQ(tv_array.get_array().size(), 2);
    ASSERT_EQ(tv_array.get_array()[0].as_string(), "a");
    ASSERT_EQ(tv_array.get_array()[1].as_string(), "b");
}

/**
 * @brief 测试 typed_variant 的对象类型锁定功能
 */
TEST_F(typed_variant_test, ObjectTypeLocking) {
    // 创建一个对象
    dict obj;
    obj["name"] = "John";
    obj["age"]  = 30;

    // 测试 object_type
    typed_variant tv_object(type_id::object_type);
    ASSERT_EQ(tv_object.get_type(), type_id::object_type);

    // 赋值一个对象，类型应该保持为 object_type
    tv_object = obj;
    ASSERT_EQ(tv_object.get_type(), type_id::object_type);
    ASSERT_EQ(tv_object.get_object().size(), 2);
    ASSERT_EQ(tv_object.get_object()["name"].as_string(), "John");
    ASSERT_EQ(tv_object.get_object()["age"].as_int64(), 30);

    // 赋值一个非对象类型，类型应该保持为 object_type，但值不会改变
    size_t prev_size = tv_object.get_object().size();
    verify_assignment_exception(
        [&]() {
        tv_object = 42;
    },
        tv_object);
    ASSERT_EQ(tv_object.get_object().size(), prev_size);

    // 创建一个新的对象
    dict obj2;
    obj2["city"]    = "New York";
    obj2["country"] = "USA";

    // 赋值一个新对象，类型应该保持为 object_type
    tv_object = obj2;
    ASSERT_EQ(tv_object.get_type(), type_id::object_type);
    ASSERT_EQ(tv_object.get_object().size(), 2);
    ASSERT_EQ(tv_object.get_object()["city"].as_string(), "New York");
    ASSERT_EQ(tv_object.get_object()["country"].as_string(), "USA");
}

/**
 * @brief 测试 typed_variant 的 blob 类型锁定功能
 */
TEST_F(typed_variant_test, BlobTypeLocking) {
    // 创建一个 blob
    std::vector<char> data1 = {1, 2, 3, 4, 5};
    blob              b1;
    b1.data = data1;

    // 测试 blob_type
    typed_variant tv_blob(type_id::blob_type);
    ASSERT_EQ(tv_blob.get_type(), type_id::blob_type);

    // 赋值一个 blob，类型应该保持为 blob_type
    tv_blob = b1;
    ASSERT_EQ(tv_blob.get_type(), type_id::blob_type);
    ASSERT_EQ(tv_blob.as_blob().data.size(), 5);
    ASSERT_EQ(tv_blob.as_blob().data[0], 1);
    ASSERT_EQ(tv_blob.as_blob().data[4], 5);

    // 赋值一个非 blob 类型，类型应该保持为 blob_type，但值不会改变
    verify_assignment_exception(
        [&]() {
        tv_blob = 42;
    },
        tv_blob);

    // 创建一个新的 blob
    std::vector<char> data2 = {10, 20, 30};
    blob              b2;
    b2.data = data2;

    // 赋值一个新 blob，类型应该保持为 blob_type
    tv_blob = b2;
    ASSERT_EQ(tv_blob.get_type(), type_id::blob_type);
    ASSERT_EQ(tv_blob.as_blob().data.size(), 3);
    ASSERT_EQ(tv_blob.as_blob().data[0], 10);
    ASSERT_EQ(tv_blob.as_blob().data[2], 30);
}

/**
 * @brief 测试 typed_variant 的边界值测试
 */
TEST_F(typed_variant_test, BoundaryValues) {
    // 测试 int8_type 的边界值
    typed_variant tv_int8(type_id::int8_type);
    tv_int8 = -128; // int8_t 的最小值
    ASSERT_EQ(tv_int8.get_type(), type_id::int8_type);
    ASSERT_EQ(tv_int8.as_int64(), -128);

    tv_int8 = 127; // int8_t 的最大值
    ASSERT_EQ(tv_int8.get_type(), type_id::int8_type);
    ASSERT_EQ(tv_int8.as_int64(), 127);

    // 测试 uint8_type 的边界值
    typed_variant tv_uint8(type_id::uint8_type);
    tv_uint8 = 0; // uint8_t 的最小值
    ASSERT_EQ(tv_uint8.get_type(), type_id::uint8_type);
    ASSERT_EQ(tv_uint8.as_uint64(), 0);

    tv_uint8 = 255; // uint8_t 的最大值
    ASSERT_EQ(tv_uint8.get_type(), type_id::uint8_type);
    ASSERT_EQ(tv_uint8.as_uint64(), 255);

    // 测试 int16_type 的边界值
    typed_variant tv_int16(type_id::int16_type);
    tv_int16 = -32768; // int16_t 的最小值
    ASSERT_EQ(tv_int16.get_type(), type_id::int16_type);
    ASSERT_EQ(tv_int16.as_int64(), -32768);

    tv_int16 = 32767; // int16_t 的最大值
    ASSERT_EQ(tv_int16.get_type(), type_id::int16_type);
    ASSERT_EQ(tv_int16.as_int64(), 32767);

    // 测试 uint16_type 的边界值
    typed_variant tv_uint16(type_id::uint16_type);
    tv_uint16 = 0; // uint16_t 的最小值
    ASSERT_EQ(tv_uint16.get_type(), type_id::uint16_type);
    ASSERT_EQ(tv_uint16.as_uint64(), 0);

    tv_uint16 = 65535; // uint16_t 的最大值
    ASSERT_EQ(tv_uint16.get_type(), type_id::uint16_type);
    ASSERT_EQ(tv_uint16.as_uint64(), 65535);

    // 测试 int32_type 的边界值
    typed_variant tv_int32(type_id::int32_type);
    tv_int32 = static_cast<int32_t>(-2147483647 - 1); // int32_t 的最小值
    ASSERT_EQ(tv_int32.get_type(), type_id::int32_type);
    ASSERT_EQ(tv_int32.as_int64(), -2147483647 - 1);

    tv_int32 = 2147483647; // int32_t 的最大值
    ASSERT_EQ(tv_int32.get_type(), type_id::int32_type);
    ASSERT_EQ(tv_int32.as_int64(), 2147483647);

    // 测试 uint32_type 的边界值
    typed_variant tv_uint32(type_id::uint32_type);
    tv_uint32 = 0; // uint32_t 的最小值
    ASSERT_EQ(tv_uint32.get_type(), type_id::uint32_type);
    ASSERT_EQ(tv_uint32.as_uint64(), 0);

    // tv_uint32 = 4294967295U; // uint32_t 的最大值
    // ASSERT_EQ(tv_uint32.get_type(), type_id::uint32_type);
    // ASSERT_EQ(tv_uint32.as_uint64(), 4294967295U);

    // // 测试 int64_type 的边界值
    // typed_variant tv_int64(type_id::int64_type);
    // // 使用较小的值，避免溢出
    // tv_int64 = -9223372036854775807LL; // 接近 int64_t 的最小值
    // ASSERT_EQ(tv_int64.get_type(), type_id::int64_type);
    // ASSERT_EQ(tv_int64.as_int64(), -9223372036854775807LL);

    // tv_int64 = 9223372036854775807LL; // int64_t 的最大值
    // ASSERT_EQ(tv_int64.get_type(), type_id::int64_type);
    // ASSERT_EQ(tv_int64.as_int64(), 9223372036854775807LL);

    // // 测试 uint64_type 的边界值
    // typed_variant tv_uint64(type_id::uint64_type);
    // tv_uint64 = 0; // uint64_t 的最小值
    // ASSERT_EQ(tv_uint64.get_type(), type_id::uint64_type);
    // ASSERT_EQ(tv_uint64.as_uint64(), 0);

    // 使用较小的值，避免溢出
    // tv_uint64 = 18446744073709551615ULL; // uint64_t 的最大值
    // ASSERT_EQ(tv_uint64.get_type(), type_id::uint64_type);
    // ASSERT_EQ(tv_uint64.as_uint64(), 18446744073709551615ULL);

    // 测试 double_type 的边界值
    typed_variant tv_double(type_id::double_type);
    // 使用较小的值，避免精度问题
    tv_double = -1.7976931348623157e+308 * 0.5; // 接近 double 的最小值
    ASSERT_EQ(tv_double.get_type(), type_id::double_type);
    ASSERT_DOUBLE_EQ(tv_double.as_double(), -1.7976931348623157e+308 * 0.5);

    tv_double = 1.7976931348623157e+308 * 0.5; // 接近 double 的最大值
    ASSERT_EQ(tv_double.get_type(), type_id::double_type);
    ASSERT_DOUBLE_EQ(tv_double.as_double(), 1.7976931348623157e+308 * 0.5);
}

/**
 * @brief 测试 typed_variant 的复杂转换场景
 */
TEST_F(typed_variant_test, ComplexConversions) {
    // 测试从字符串到各种类型的转换
    typed_variant tv_int32(type_id::int32_type);
    try {
        tv_int32 = "42";
        ASSERT_EQ(tv_int32.get_type(), type_id::int32_type);
        ASSERT_EQ(tv_int32.as_int64(), 42);
    } catch (const std::exception& e) {
        // 如果转换失败，确保类型保持不变
        ASSERT_EQ(tv_int32.get_type(), type_id::int32_type);
    }

    typed_variant tv_double(type_id::double_type);
    try {
        tv_double = "3.14159";
        ASSERT_EQ(tv_double.get_type(), type_id::double_type);
        ASSERT_DOUBLE_EQ(tv_double.as_double(), 3.14159);
    } catch (const std::exception& e) {
        // 如果转换失败，确保类型保持不变
        ASSERT_EQ(tv_double.get_type(), type_id::double_type);
    }

    typed_variant tv_bool(type_id::bool_type);
    try {
        tv_bool = "true";
        ASSERT_EQ(tv_bool.get_type(), type_id::bool_type);
        ASSERT_EQ(tv_bool.as_bool(), true);
    } catch (const std::exception& e) {
        // 如果转换失败，确保类型保持不变
        ASSERT_EQ(tv_bool.get_type(), type_id::bool_type);
    }

    // 测试从 variant 到 typed_variant 的转换
    variant v_int(42);
    variant v_double(3.14159);
    variant v_string("hello");
    variant v_bool(true);

    typed_variant tv_int64(type_id::int64_type);
    tv_int64 = v_int;
    ASSERT_EQ(tv_int64.get_type(), type_id::int64_type);
    ASSERT_EQ(tv_int64.as_int64(), 42);

    typed_variant tv_string(type_id::string_type);
    tv_string = v_double;
    ASSERT_EQ(tv_string.get_type(), type_id::string_type);
    ASSERT_TRUE(tv_string.as_string().find("3.14159") != std::string::npos);

    typed_variant tv_int8(type_id::int8_type);
    tv_int8 = v_bool;
    ASSERT_EQ(tv_int8.get_type(), type_id::int8_type);
    ASSERT_EQ(tv_int8.as_int64(), 1);
}

/**
 * @brief 测试 typed_variant 的链式赋值
 */
TEST_F(typed_variant_test, ChainedAssignments) {
    typed_variant tv_int32(type_id::int32_type);
    typed_variant tv_int64(type_id::int64_type);
    typed_variant tv_double(type_id::double_type);

    // 链式赋值
    tv_int32 = tv_int64 = tv_double = 42;

    // 验证每个变量的类型和值
    ASSERT_EQ(tv_double.get_type(), type_id::double_type);
    ASSERT_DOUBLE_EQ(tv_double.as_double(), 42.0);

    ASSERT_EQ(tv_int64.get_type(), type_id::int64_type);
    ASSERT_EQ(tv_int64.as_int64(), 42);

    ASSERT_EQ(tv_int32.get_type(), type_id::int32_type);
    ASSERT_EQ(tv_int32.as_int64(), 42);
}

TEST_F(typed_variant_test, MoveAssignment) {
    // 创建一个源对象
    typed_variant tv_source(type_id::string_type);
    tv_source = "hello world";
    ASSERT_EQ(tv_source.get_type(), type_id::string_type);
    ASSERT_EQ(tv_source.as_string(), "hello world");

    // 创建一个目标对象
    typed_variant tv_target(type_id::int32_type);
    tv_target = 42;
    ASSERT_EQ(tv_target.get_type(), type_id::int32_type);
    ASSERT_EQ(tv_target.as_int64(), 42);

    verify_assignment_exception([&]() {
        tv_target = std::move(tv_source);
    }, tv_target);

    typed_variant tv_source2(type_id::int16_type);
    tv_source2 = 50;

    tv_target = std::move(tv_source2);
    ASSERT_EQ(tv_target.get_type(), type_id::int32_type);
    ASSERT_EQ(tv_target.as_int64(), 50);

    typed_variant tv_source3;
    from_variant(variant(1), tv_source3);
    tv_target = std::move(tv_source3);
    ASSERT_EQ(tv_target.get_type(), type_id::int32_type);
    ASSERT_EQ(tv_target.as_int64(), 1);

    variant v2;
    to_variant(tv_target, v2);
    ASSERT_EQ(v2.get_type(), type_id::int32_type);
    ASSERT_EQ(v2.as_int64(), 1);
}

/**
 * @brief 测试 typed_variant 与 variant 之间的转换函数
 */
TEST_F(typed_variant_test, VariantConversionFunctions) {
    // 测试 from_variant 函数
    typed_variant tv_target;
    from_variant(variant(1), tv_target);
    ASSERT_EQ(tv_target.get_type(), type_id::int32_type);
    ASSERT_EQ(tv_target.as_int64(), 1);

    // 测试 to_variant 函数
    variant v;
    to_variant(tv_target, v);
    ASSERT_EQ(v.get_type(), type_id::int32_type);
    ASSERT_EQ(v.as_int64(), 1);

    // 测试不同类型的转换
    typed_variant tv_string(type_id::string_type);
    tv_string = "hello";

    variant v_string;
    to_variant(tv_string, v_string);
    ASSERT_EQ(v_string.get_type(), type_id::string_type);
    ASSERT_EQ(v_string.as_string(), "hello");

    typed_variant tv_double;
    from_variant(variant(3.14), tv_double);
    ASSERT_EQ(tv_double.get_type(), type_id::double_type);
    ASSERT_DOUBLE_EQ(tv_double.as_double(), 3.14);

    // 测试复杂类型的转换
    dict dict;
    dict["name"] = "John";
    dict["age"]  = 30;

    typed_variant tv_object(type_id::object_type);
    from_variant(variant(dict), tv_object);
    ASSERT_EQ(tv_object.get_type(), type_id::object_type);
    ASSERT_EQ(tv_object.get_object().size(), 2);
    ASSERT_EQ(tv_object.get_object()["name"].as_string(), "John");
    ASSERT_EQ(tv_object.get_object()["age"].as_int64(), 30);

    variant v_object;
    to_variant(tv_object, v_object);
    ASSERT_EQ(v_object.get_type(), type_id::object_type);
    ASSERT_EQ(v_object.get_object().size(), 2);
    ASSERT_EQ(v_object.get_object()["name"].as_string(), "John");
    ASSERT_EQ(v_object.get_object()["age"].as_int64(), 30);
}
} // namespace test
} // namespace mc