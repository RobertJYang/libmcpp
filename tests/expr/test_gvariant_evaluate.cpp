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

#include <glib-2.0/glib.h>
#include <gtest/gtest.h>
#include <mc/expr/engine.h>

namespace {
class gvariant_evaluate_test : public ::testing::Test {
protected:
    gvariant_evaluate_test()
    {
    }

    void SetUp() override
    {
    }

    void TearDown() override
    {
    }

    mc::expr::engine engine;
};

TEST_F(gvariant_evaluate_test, BasicTypes)
{
    auto& ctx = engine.get_global_context();

    // 测试整数表达式
    GVariant* result = engine.evaluate_as_gvariant("42", ctx);
    ASSERT_NE(result, nullptr);

    // 检查 GVariant 类型
    if (g_variant_is_of_type(result, G_VARIANT_TYPE_INT32)) {
        EXPECT_EQ(g_variant_get_int32(result), 42);
    } else if (g_variant_is_of_type(result, G_VARIANT_TYPE_INT64)) {
        EXPECT_EQ(g_variant_get_int64(result), 42);
    } else if (g_variant_is_of_type(result, G_VARIANT_TYPE_UINT32)) {
        EXPECT_EQ(g_variant_get_uint32(result), 42);
    } else if (g_variant_is_of_type(result, G_VARIANT_TYPE_UINT64)) {
        EXPECT_EQ(g_variant_get_uint64(result), 42);
    } else {
        GTEST_FAIL() << "Unexpected GVariant type for integer 42";
    }

    g_variant_unref(result);

    // 测试浮点数表达式
    result = engine.evaluate_as_gvariant("3.14", ctx);
    ASSERT_NE(result, nullptr);

    if (g_variant_is_of_type(result, G_VARIANT_TYPE_DOUBLE)) {
        EXPECT_DOUBLE_EQ(g_variant_get_double(result), 3.14);
    } else {
        GTEST_FAIL() << "Unexpected GVariant type for double 3.14";
    }

    g_variant_unref(result);

    // 测试字符串表达式
    result = engine.evaluate_as_gvariant("\"hello\"", ctx);
    ASSERT_NE(result, nullptr);

    if (g_variant_is_of_type(result, G_VARIANT_TYPE_STRING)) {
        EXPECT_STREQ(g_variant_get_string(result, nullptr), "hello");
    } else {
        GTEST_FAIL() << "Unexpected GVariant type for string \"hello\"";
    }

    g_variant_unref(result);
}

TEST_F(gvariant_evaluate_test, ArithmeticOperations)
{
    auto& ctx = engine.get_global_context();

    // 测试加法
    GVariant* result = engine.evaluate_as_gvariant("10 + 5", ctx);
    ASSERT_NE(result, nullptr);

    if (g_variant_is_of_type(result, G_VARIANT_TYPE_INT32)) {
        EXPECT_EQ(g_variant_get_int32(result), 15);
    } else if (g_variant_is_of_type(result, G_VARIANT_TYPE_INT64)) {
        EXPECT_EQ(g_variant_get_int64(result), 15);
    } else {
        GTEST_FAIL() << "Unexpected GVariant type for arithmetic result";
    }

    g_variant_unref(result);

    // 测试乘法
    result = engine.evaluate_as_gvariant("6 * 7", ctx);
    ASSERT_NE(result, nullptr);

    if (g_variant_is_of_type(result, G_VARIANT_TYPE_INT32)) {
        EXPECT_EQ(g_variant_get_int32(result), 42);
    } else if (g_variant_is_of_type(result, G_VARIANT_TYPE_INT64)) {
        EXPECT_EQ(g_variant_get_int64(result), 42);
    } else {
        GTEST_FAIL() << "Unexpected GVariant type for arithmetic result";
    }

    g_variant_unref(result);
}

TEST_F(gvariant_evaluate_test, BooleanOperations)
{
    auto& ctx = engine.get_global_context();

    // 测试布尔表达式
    GVariant* result = engine.evaluate_as_gvariant("true", ctx);
    ASSERT_NE(result, nullptr);

    if (g_variant_is_of_type(result, G_VARIANT_TYPE_BOOLEAN)) {
        EXPECT_TRUE(g_variant_get_boolean(result));
    } else {
        GTEST_FAIL() << "Unexpected GVariant type for boolean true";
    }

    g_variant_unref(result);

    // 测试比较表达式
    result = engine.evaluate_as_gvariant("5 > 3", ctx);
    ASSERT_NE(result, nullptr);

    if (g_variant_is_of_type(result, G_VARIANT_TYPE_BOOLEAN)) {
        EXPECT_TRUE(g_variant_get_boolean(result));
    } else {
        GTEST_FAIL() << "Unexpected GVariant type for comparison result";
    }

    g_variant_unref(result);
}

} // namespace