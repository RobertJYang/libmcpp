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

#include <gtest/gtest.h>

#include <glib-2.0/glib.h>
#include <mc/expr/context.h>
#include <mc/expr/engine.h>
#include <mc/variant.h>
#include <mc/variant_c_api.h>

#include <vector>

namespace {

class variant_c_api_test : public ::testing::Test {
protected:
    void TearDown() override
    {
        if (m_context != nullptr) {
            mc_context_delete(m_context);
        }
        if (m_engine != nullptr) {
            mc_engine_delete(m_engine);
        }
        for (auto* value : m_variants) {
            mc_variant_delete(value);
        }
    }

    void register_variant(mc_variant_t* value)
    {
        ASSERT_NE(value, nullptr);
        m_variants.push_back(value);
    }

    mc_engine_t*               m_engine{nullptr};
    mc_context_t*              m_context{nullptr};
    std::vector<mc_variant_t*> m_variants;
};

TEST_F(variant_c_api_test, basic_creation_and_cleanup)
{
    auto* i64 = mc_variant_from_int64(123);
    auto* dbl = mc_variant_from_double(3.14);
    auto* bl  = mc_variant_from_bool(true);
    auto* str = mc_variant_from_string("hello");

    ASSERT_NE(i64, nullptr);
    ASSERT_NE(dbl, nullptr);
    ASSERT_NE(bl, nullptr);
    ASSERT_NE(str, nullptr);

    const auto* i64_variant = reinterpret_cast<const mc::variant*>(i64);
    const auto* dbl_variant = reinterpret_cast<const mc::variant*>(dbl);
    const auto* bl_variant  = reinterpret_cast<const mc::variant*>(bl);
    const auto* str_variant = reinterpret_cast<const mc::variant*>(str);

    ASSERT_TRUE(i64_variant->is_int64());
    EXPECT_EQ(i64_variant->as_int64(), 123);

    ASSERT_TRUE(dbl_variant->is_double());
    EXPECT_DOUBLE_EQ(dbl_variant->as_double(), 3.14);

    ASSERT_TRUE(bl_variant->is_bool());
    EXPECT_TRUE(bl_variant->as_bool());

    ASSERT_TRUE(str_variant->is_string());
    EXPECT_EQ(str_variant->get_string(), "hello");

    mc_variant_delete(nullptr);
    mc_variant_delete(i64);
    mc_variant_delete(dbl);
    mc_variant_delete(bl);
    mc_variant_delete(str);
}

TEST_F(variant_c_api_test, context_and_evaluate_expression)
{
    m_engine = mc_engine_new();
    ASSERT_NE(m_engine, nullptr);

    EXPECT_EQ(mc_context_new(nullptr), nullptr);

    m_context = mc_context_new(m_engine);
    ASSERT_NE(m_context, nullptr);

    auto* a = mc_variant_from_int64(2);
    auto* b = mc_variant_from_int64(3);
    register_variant(a);
    register_variant(b);

    EXPECT_EQ(mc_context_register_variable(nullptr, "a", a), -1);
    EXPECT_EQ(mc_context_register_variable(m_context, nullptr, a), -1);
    EXPECT_EQ(mc_context_register_variable(m_context, "a", nullptr), -1);

    EXPECT_EQ(mc_context_register_variable(m_context, "a", a), 0);
    EXPECT_EQ(mc_context_register_variable(m_context, "b", b), 0);

    auto* result = static_cast<GVariant*>(mc_engine_evaluate_as_gvariant(m_engine, "a + b", m_context));
    ASSERT_NE(result, nullptr);
    EXPECT_STREQ(g_variant_get_type_string(result), "x");
    EXPECT_EQ(g_variant_get_int64(result), 5);
    g_variant_unref(result);

    auto* invalid = static_cast<GVariant*>(mc_engine_evaluate_as_gvariant(nullptr, "a + b", m_context));
    EXPECT_EQ(invalid, nullptr);
    invalid = static_cast<GVariant*>(mc_engine_evaluate_as_gvariant(m_engine, nullptr, m_context));
    EXPECT_EQ(invalid, nullptr);
    invalid = static_cast<GVariant*>(mc_engine_evaluate_as_gvariant(m_engine, "a + b", nullptr));
    EXPECT_EQ(invalid, nullptr);
}

// 测试 mc_variant_from_string 的 null 检查路径
TEST_F(variant_c_api_test, VariantFromStringNull)
{
    auto* variant = mc_variant_from_string(nullptr);
    EXPECT_EQ(variant, nullptr);
}

} // namespace
