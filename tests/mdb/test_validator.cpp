/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <gtest/gtest.h>
#include <mc/variant.h>
#include <cstdint>
#include <stdexcept>

#include "../../../src/mdb/mdb_validator.h"

// mdb_validator 类在全局命名空间，直接使用即可

using namespace mc;

class mdb_validator_test : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }
};

// 测试整数类型验证 - uint8
TEST_F(mdb_validator_test, check_uint8_valid) {
    // 测试有效的 uint8 值
    EXPECT_NO_THROW(mdb_validator::check("test", "y", variant(static_cast<uint8_t>(0))));
    EXPECT_NO_THROW(mdb_validator::check("test", "y", variant(static_cast<uint8_t>(255))));
    EXPECT_NO_THROW(mdb_validator::check("test", "y", variant(static_cast<int32_t>(100))));  // int32 转 uint8
    EXPECT_NO_THROW(mdb_validator::check("test", "y", variant(static_cast<uint32_t>(200)))); // uint32 转 uint8
}

TEST_F(mdb_validator_test, check_uint8_invalid) {
    // 测试无效的 uint8 值
    EXPECT_THROW(mdb_validator::check("test", "y", variant(static_cast<int32_t>(-1))), std::runtime_error);
    EXPECT_THROW(mdb_validator::check("test", "y", variant(static_cast<int32_t>(256))), std::runtime_error);
    EXPECT_THROW(mdb_validator::check("test", "y", variant(std::string("123"))), std::runtime_error);
}

// 测试整数类型验证 - uint16
TEST_F(mdb_validator_test, check_uint16_valid) {
    EXPECT_NO_THROW(mdb_validator::check("test", "q", variant(static_cast<uint16_t>(0))));
    EXPECT_NO_THROW(mdb_validator::check("test", "q", variant(static_cast<uint16_t>(65535))));
    EXPECT_NO_THROW(mdb_validator::check("test", "q", variant(static_cast<int32_t>(32767)))); // int32 转 uint16
}

TEST_F(mdb_validator_test, check_uint16_invalid) {
    EXPECT_THROW(mdb_validator::check("test", "q", variant(static_cast<int32_t>(-1))), std::runtime_error);
    EXPECT_THROW(mdb_validator::check("test", "q", variant(static_cast<int32_t>(65536))), std::runtime_error);
}

// 测试整数类型验证 - uint32
TEST_F(mdb_validator_test, check_uint32_valid) {
    EXPECT_NO_THROW(mdb_validator::check("test", "u", variant(static_cast<uint32_t>(0))));
    EXPECT_NO_THROW(mdb_validator::check("test", "u", variant(static_cast<uint32_t>(4294967295U))));
    EXPECT_NO_THROW(mdb_validator::check("test", "u", variant(static_cast<int32_t>(1))));          // int32 转 uint32
    EXPECT_NO_THROW(mdb_validator::check("test", "u", variant(static_cast<int32_t>(2147483647)))); // int32 最大值转 uint32
}

TEST_F(mdb_validator_test, check_uint32_invalid) {
    EXPECT_THROW(mdb_validator::check("test", "u", variant(static_cast<int32_t>(-1))), std::runtime_error);
    EXPECT_THROW(mdb_validator::check("test", "u", variant(static_cast<int64_t>(4294967296LL))), std::runtime_error);
}

// 测试整数类型验证 - uint64
TEST_F(mdb_validator_test, check_uint64_valid) {
    EXPECT_NO_THROW(mdb_validator::check("test", "t", variant(static_cast<uint64_t>(0))));
    EXPECT_NO_THROW(mdb_validator::check("test", "t", variant(static_cast<int32_t>(1)))); // int32 转 uint64
    EXPECT_NO_THROW(mdb_validator::check("test", "t", variant(static_cast<uint64_t>(9223372036854775807ULL))));
}

TEST_F(mdb_validator_test, check_uint64_invalid) {
    EXPECT_THROW(mdb_validator::check("test", "t", variant(static_cast<int32_t>(-1))), std::runtime_error);
}

// 测试整数类型验证 - int16
TEST_F(mdb_validator_test, check_int16_valid) {
    EXPECT_NO_THROW(mdb_validator::check("test", "n", variant(static_cast<int16_t>(-32768))));
    EXPECT_NO_THROW(mdb_validator::check("test", "n", variant(static_cast<int16_t>(32767))));
    EXPECT_NO_THROW(mdb_validator::check("test", "n", variant(static_cast<int32_t>(0)))); // int32 转 int16
}

TEST_F(mdb_validator_test, check_int16_invalid) {
    EXPECT_THROW(mdb_validator::check("test", "n", variant(static_cast<int32_t>(-32769))), std::runtime_error);
    EXPECT_THROW(mdb_validator::check("test", "n", variant(static_cast<int32_t>(32768))), std::runtime_error);
}

// 测试整数类型验证 - int32
TEST_F(mdb_validator_test, check_int32_valid) {
    EXPECT_NO_THROW(mdb_validator::check("test", "i", variant(static_cast<int32_t>(-2147483648))));
    EXPECT_NO_THROW(mdb_validator::check("test", "i", variant(static_cast<int32_t>(2147483647))));
    EXPECT_NO_THROW(mdb_validator::check("test", "i", variant(static_cast<int16_t>(0)))); // int16 转 int32
}

TEST_F(mdb_validator_test, check_int32_invalid) {
    EXPECT_THROW(mdb_validator::check("test", "i", variant(static_cast<int64_t>(-2147483649LL))), std::runtime_error);
    EXPECT_THROW(mdb_validator::check("test", "i", variant(static_cast<int64_t>(2147483648LL))), std::runtime_error);
}

// 测试整数类型验证 - int64
TEST_F(mdb_validator_test, check_int64_valid) {
    // 使用 INT64_MIN 和 INT64_MAX 避免警告
    EXPECT_NO_THROW(mdb_validator::check("test", "x", variant(static_cast<int64_t>(INT64_MIN))));
    EXPECT_NO_THROW(mdb_validator::check("test", "x", variant(static_cast<int64_t>(INT64_MAX))));
    EXPECT_NO_THROW(mdb_validator::check("test", "x", variant(static_cast<int32_t>(0)))); // int32 转 int64
}

// 测试字符串类型验证
TEST_F(mdb_validator_test, check_string_valid) {
    EXPECT_NO_THROW(mdb_validator::check("test", "s", variant(std::string("hello"))));
    EXPECT_NO_THROW(mdb_validator::check("test", "s", variant(std::string(""))));
}

TEST_F(mdb_validator_test, check_string_invalid) {
    EXPECT_THROW(mdb_validator::check("test", "s", variant(static_cast<int32_t>(123))), std::runtime_error);
    EXPECT_THROW(mdb_validator::check("test", "s", variant(true)), std::runtime_error);
}

// 测试布尔类型验证
TEST_F(mdb_validator_test, check_bool_valid) {
    EXPECT_NO_THROW(mdb_validator::check("test", "b", variant(true)));
    EXPECT_NO_THROW(mdb_validator::check("test", "b", variant(false)));
}

TEST_F(mdb_validator_test, check_bool_invalid) {
    EXPECT_THROW(mdb_validator::check("test", "b", variant(static_cast<int32_t>(1))), std::runtime_error);
    EXPECT_THROW(mdb_validator::check("test", "b", variant(std::string("true"))), std::runtime_error);
}

// 测试双精度浮点数类型验证
TEST_F(mdb_validator_test, check_double_valid) {
    EXPECT_NO_THROW(mdb_validator::check("test", "d", variant(3.14)));
    EXPECT_NO_THROW(mdb_validator::check("test", "d", variant(0.0)));
}

TEST_F(mdb_validator_test, check_double_invalid) {
    EXPECT_THROW(mdb_validator::check("test", "d", variant(static_cast<int32_t>(123))), std::runtime_error);
    EXPECT_THROW(mdb_validator::check("test", "d", variant(std::string("3.14"))), std::runtime_error);
}

// 测试数组类型验证 - ay (byte array)
TEST_F(mdb_validator_test, check_array_ay_valid) {
    // blob 类型
    mc::blob blob_data;
    blob_data.data = {0x01, 0x02, 0x03};
    EXPECT_NO_THROW(mdb_validator::check("test", "ay", variant(blob_data)));

    // 数组类型
    variants arr = {variant(static_cast<uint8_t>(1)), variant(static_cast<uint8_t>(2))};
    EXPECT_NO_THROW(mdb_validator::check("test", "ay", variant(arr)));
}

TEST_F(mdb_validator_test, check_array_ay_invalid) {
    EXPECT_THROW(mdb_validator::check("test", "ay", variant(std::string("123"))), std::runtime_error);
}

// 测试数组类型验证 - 通用数组
TEST_F(mdb_validator_test, check_array_valid) {
    variants arr = {variant(static_cast<int32_t>(1)), variant(static_cast<int32_t>(2))};
    EXPECT_NO_THROW(mdb_validator::check("test", "ai", variant(arr)));
}

TEST_F(mdb_validator_test, check_array_invalid) {
    EXPECT_THROW(mdb_validator::check("test", "ai", variant(static_cast<int32_t>(1))), std::runtime_error);
}

// 测试结构体类型验证
TEST_F(mdb_validator_test, check_struct_valid) {
    variants struct_data = {variant(static_cast<int32_t>(1)), variant(std::string("test"))};
    EXPECT_NO_THROW(mdb_validator::check("test", "(is)", variant(struct_data)));
}

TEST_F(mdb_validator_test, check_struct_invalid) {
    // 元素数量不匹配
    variants struct_data = {variant(static_cast<int32_t>(1))};
    EXPECT_THROW(mdb_validator::check("test", "(is)", variant(struct_data)), std::runtime_error);

    // 不是数组
    EXPECT_THROW(mdb_validator::check("test", "(is)", variant(static_cast<int32_t>(1))), std::runtime_error);
}

// 测试字典类型验证
TEST_F(mdb_validator_test, check_dict_valid) {
    dict dict_data;
    dict_data.insert(variant(std::string("key1")), variant(std::string("value1")));
    EXPECT_NO_THROW(mdb_validator::check("test", "a{ss}", variant(dict_data)));
}

TEST_F(mdb_validator_test, check_dict_invalid) {
    EXPECT_THROW(mdb_validator::check("test", "a{ss}", variant(static_cast<int32_t>(1))), std::runtime_error);
}

// 测试方法参数验证
TEST_F(mdb_validator_test, check_method_args_valid) {
    variants args = {variant(static_cast<int32_t>(1)), variant(std::string("test"))};
    EXPECT_NO_THROW(mdb_validator::check_method_args("test_method", "is", variant(args)));
}

TEST_F(mdb_validator_test, check_method_args_invalid_count) {
    // 参数数量不匹配
    variants args = {variant(static_cast<int32_t>(1))};
    EXPECT_THROW(mdb_validator::check_method_args("test_method", "is", variant(args)), std::runtime_error);
}

TEST_F(mdb_validator_test, check_method_args_invalid_type) {
    // 参数类型不匹配
    variants args = {variant(std::string("test")), variant(static_cast<int32_t>(1))};
    EXPECT_THROW(mdb_validator::check_method_args("test_method", "is", variant(args)), std::runtime_error);
}

TEST_F(mdb_validator_test, check_method_args_not_array) {
    // 参数不是数组
    EXPECT_THROW(mdb_validator::check_method_args("test_method", "i", variant(static_cast<int32_t>(1))), std::runtime_error);
}
