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
 * @file test_variant_bool_operator.cpp
 * @brief 测试variant的bool运算符：
 * - bool类型的false返回false
 * - 整数类型的0返回false
 * - 浮点数类型的0返回false
 * - 其他类型返回true
 */
#include "test_variant_helpers.h"
#include <gtest/gtest.h>
#include <limits>
#include <mc/variant.h>
#include <stdexcept>

namespace mc {
namespace test {

class VariantBoolOperatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 在每个测试前执行
    }

    void TearDown() override {
        // 在每个测试后执行
    }
};

/**
 * @brief 测试variant的bool运算符
 */
TEST_F(VariantBoolOperatorTest, BoolOperator) {
    // 测试bool类型
    variant v_true(true);
    variant v_false(false);
    ASSERT_TRUE(static_cast<bool>(v_true)) << "true类型的variant应该转换为true";
    ASSERT_FALSE(static_cast<bool>(v_false)) << "false类型的variant应该转换为false";

    // 测试整数类型
    variant v_int_zero(0);
    variant v_int_nonzero(42);
    ASSERT_FALSE(static_cast<bool>(v_int_zero)) << "值为0的整数variant应该转换为false";
    ASSERT_TRUE(static_cast<bool>(v_int_nonzero)) << "非0整数variant应该转换为true";

    // 测试不同整数类型
    variant v_int8_zero(int8_t(0));
    variant v_int16_zero(int16_t(0));
    variant v_int32_zero(int32_t(0));
    variant v_int64_zero(int64_t(0));
    variant v_uint8_zero(uint8_t(0));
    variant v_uint16_zero(uint16_t(0));
    variant v_uint32_zero(uint32_t(0));
    variant v_uint64_zero(uint64_t(0));

    ASSERT_FALSE(static_cast<bool>(v_int8_zero)) << "值为0的int8_t variant应该转换为false";
    ASSERT_FALSE(static_cast<bool>(v_int16_zero)) << "值为0的int16_t variant应该转换为false";
    ASSERT_FALSE(static_cast<bool>(v_int32_zero)) << "值为0的int32_t variant应该转换为false";
    ASSERT_FALSE(static_cast<bool>(v_int64_zero)) << "值为0的int64_t variant应该转换为false";
    ASSERT_FALSE(static_cast<bool>(v_uint8_zero)) << "值为0的uint8_t variant应该转换为false";
    ASSERT_FALSE(static_cast<bool>(v_uint16_zero)) << "值为0的uint16_t variant应该转换为false";
    ASSERT_FALSE(static_cast<bool>(v_uint32_zero)) << "值为0的uint32_t variant应该转换为false";
    ASSERT_FALSE(static_cast<bool>(v_uint64_zero)) << "值为0的uint64_t variant应该转换为false";

    // 测试浮点数类型
    variant v_double_zero(0.0);
    variant v_double_nonzero(3.14);
    variant v_float_zero(0.0f);
    variant v_float_nonzero(2.71f);

    ASSERT_FALSE(static_cast<bool>(v_double_zero)) << "值为0的double variant应该转换为false";
    ASSERT_TRUE(static_cast<bool>(v_double_nonzero)) << "非0 double variant应该转换为true";
    ASSERT_FALSE(static_cast<bool>(v_float_zero)) << "值为0的float variant应该转换为false";
    ASSERT_TRUE(static_cast<bool>(v_float_nonzero)) << "非0 float variant应该转换为true";

    // 测试null类型
    variant v_null;
    ASSERT_FALSE(static_cast<bool>(v_null)) << "null类型的variant应该转换为false";

    // 测试字符串类型
    variant v_empty_str("");
    variant v_str("test");
    ASSERT_TRUE(static_cast<bool>(v_empty_str)) << "空字符串variant应该转换为true";
    ASSERT_TRUE(static_cast<bool>(v_str)) << "非空字符串variant应该转换为true";

    // 测试数组类型
    variants empty_arr;
    variants arr = {1, 2};
    variant  v_empty_arr(empty_arr);
    variant  v_arr(arr);
    ASSERT_TRUE(static_cast<bool>(v_empty_arr)) << "空数组variant应该转换为true";
    ASSERT_TRUE(static_cast<bool>(v_arr)) << "非空数组variant应该转换为true";

    // 测试对象类型
    mutable_dict empty_dict;
    mutable_dict dict;
    dict["key"] = 42;
    variant v_empty_dict(empty_dict);
    variant v_dict(dict);
    ASSERT_TRUE(static_cast<bool>(v_empty_dict)) << "空对象variant应该转换为true";
    ASSERT_TRUE(static_cast<bool>(v_dict)) << "非空对象variant应该转换为true";

    // 测试二进制数据类型
    blob    empty_blob;
    blob    b{{1, 2, 3}};
    variant v_empty_blob(empty_blob);
    variant v_blob(b);
    ASSERT_TRUE(static_cast<bool>(v_empty_blob)) << "空二进制数据variant应该转换为true";
    ASSERT_TRUE(static_cast<bool>(v_blob)) << "非空二进制数据variant应该转换为true";

    // 测试在条件表达式中使用
    variant v_cond_true(true);
    variant v_cond_false(false);
    variant v_cond_int_zero(0);
    variant v_cond_int_nonzero(1);

    int count = 0;
    if (v_cond_true) {
        count++;
    }
    if (v_cond_false) {
        count++;
    }
    if (v_cond_int_zero) {
        count++;
    }
    if (v_cond_int_nonzero) {
        count++;
    }

    ASSERT_EQ(count, 2) << "在条件表达式中应该正确处理variant的布尔转换";
}

} // namespace test
} // namespace mc