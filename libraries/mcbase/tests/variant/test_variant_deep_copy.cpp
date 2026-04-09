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
 * @file test_variant_deep_copy.cpp
 * @brief 测试 variant 深拷贝功能，包括跨 dict、array、variant_extension 的场景
 */
#include <gtest/gtest.h>
#include <mc/dict.h>
#include <mc/memory.h>
#include <mc/variant.h>
#include <mc/variant/copy_context.h>
#include <mc/variant/variant_extension.h>
#include <test_utilities/base.h>

namespace mc {
namespace test {

class VariantDeepCopyTest : public mc::test::TestBase {
protected:
    void SetUp() override
    {
        TestBase::SetUp();
    }

    void TearDown() override
    {
        TestBase::TearDown();
    }
};

class copy_context_stub : public mc::enable_shared_from_this<copy_context_stub> {
public:
    explicit copy_context_stub(int value) : m_value(value)
    {}

    int value() const
    {
        return m_value;
    }

private:
    int m_value;
};

/**
 * @brief 测试 variant 基本类型的深拷贝
 */
TEST_F(VariantDeepCopyTest, BasicTypes)
{
    // 基本类型的深拷贝应该创建新的独立副本
    variant v1(42);
    variant v2 = v1.deep_copy();

    EXPECT_EQ(v1, v2);
    EXPECT_EQ(v2.as_int32(), 42);

    // 修改原值不应该影响拷贝
    v1 = 100;
    EXPECT_EQ(v1.as_int32(), 100);
    EXPECT_EQ(v2.as_int32(), 42);

    // 字符串测试
    variant v3("test");
    variant v4 = v3.deep_copy();
    EXPECT_EQ(v3, v4);
    v3 = "modified";
    EXPECT_EQ(v3.as_string(), "modified");
    EXPECT_EQ(v4.as_string(), "test");
}

/**
 * @brief 测试 variant 中 array 的深拷贝
 */
TEST_F(VariantDeepCopyTest, ArrayInVariant)
{
    // 创建包含数组的 variant
    variants arr1 = {1, 2, 3};
    variant  v1(arr1);
    variant  v2 = v1.deep_copy();

    // 验证拷贝成功
    EXPECT_EQ(v2.get_array().size(), 3);
    EXPECT_EQ(v2[0], 1);
    EXPECT_EQ(v2[1], 2);
    EXPECT_EQ(v2[2], 3);

    // 验证独立性：修改原数组不应该影响拷贝
    v1[0] = 100;
    EXPECT_EQ(v1[0].as_int32(), 100);
    EXPECT_EQ(v2[0].as_int32(), 1);

    // 验证内部数据指针不同
    EXPECT_NE(v1.get_array().data(), v2.get_array().data());
}

/**
 * @brief 测试 variant 中 dict 的深拷贝
 */
TEST_F(VariantDeepCopyTest, DictInVariant)
{
    // 创建包含字典的 variant
    dict    d1({{"key1", 123}, {"key2", "value"}});
    variant v1(d1);
    variant v2 = v1.deep_copy();

    // 验证拷贝成功
    EXPECT_EQ(v2.get_object().size(), 2);
    EXPECT_EQ(v2["key1"], 123);
    EXPECT_EQ(v2["key2"], "value");

    // 验证独立性：修改原字典不应该影响拷贝
    v1["key1"] = 456;
    EXPECT_EQ(v1["key1"].as_int32(), 456);
    EXPECT_EQ(v2["key1"].as_int32(), 123);

    // 验证内部数据指针不同
    EXPECT_NE(v1.get_object().data(), v2.get_object().data());
}

/**
 * @brief 测试嵌套结构的深拷贝（dict 中包含 array）
 */
TEST_F(VariantDeepCopyTest, NestedDictWithArray)
{
    // 创建嵌套结构：dict 包含 array
    variants inner_arr = {10, 20, 30};
    dict     d1({{"data", inner_arr}, {"count", 3}});
    variant  v1(d1);
    variant  v2 = v1.deep_copy();

    // 验证拷贝成功
    EXPECT_EQ(v2["count"], 3);
    EXPECT_EQ(v2["data"][0], 10);
    EXPECT_EQ(v2["data"][1], 20);
    EXPECT_EQ(v2["data"][2], 30);

    // 验证独立性：修改嵌套数组不应该影响拷贝
    v1["data"][0] = 100;
    EXPECT_EQ(v1["data"][0].as_int32(), 100);
    EXPECT_EQ(v2["data"][0].as_int32(), 10);

    // 验证内部数据指针不同
    EXPECT_NE(v1["data"].get_array().data(), v2["data"].get_array().data());
}

/**
 * @brief 测试嵌套结构的深拷贝（array 中包含 dict）
 */
TEST_F(VariantDeepCopyTest, NestedArrayWithDict)
{
    // 创建嵌套结构：array 包含 dict
    dict     inner_dict({{"name", "Alice"}, {"age", 30}});
    variants arr1 = {inner_dict, 123, "test"};
    variant  v1(arr1);
    variant  v2 = v1.deep_copy();

    // 验证拷贝成功
    EXPECT_EQ(v2.get_array().size(), 3);
    EXPECT_EQ(v2[0]["name"], "Alice");
    EXPECT_EQ(v2[0]["age"], 30);
    EXPECT_EQ(v2[1], 123);

    // 验证独立性：修改嵌套字典不应该影响拷贝
    v1[0]["name"] = "Bob";
    EXPECT_EQ(v1[0]["name"].as_string(), "Bob");
    EXPECT_EQ(v2[0]["name"].as_string(), "Alice");

    // 验证内部数据指针不同
    EXPECT_NE(v1[0].get_object().data(), v2[0].get_object().data());
}

/**
 * @brief 测试复杂嵌套结构的深拷贝
 */
TEST_F(VariantDeepCopyTest, ComplexNestedStructure)
{
    // 创建复杂嵌套：dict -> array -> dict -> array
    variants inner_inner_arr = {1, 2, 3};
    dict     inner_dict({{"data", inner_inner_arr}});
    variants inner_arr = {inner_dict, 100};
    dict     outer_dict({{"nested", inner_arr}, {"value", 999}});
    variant  v1(outer_dict);
    variant  v2 = v1.deep_copy();

    // 验证拷贝成功
    EXPECT_EQ(v2["value"], 999);
    EXPECT_EQ(v2["nested"][0]["data"][0], 1);
    EXPECT_EQ(v2["nested"][0]["data"][1], 2);
    EXPECT_EQ(v2["nested"][1], 100);

    // 验证独立性：修改深层嵌套不应该影响拷贝
    v1["nested"][0]["data"][0] = 111;
    EXPECT_EQ(v1["nested"][0]["data"][0].as_int32(), 111);
    EXPECT_EQ(v2["nested"][0]["data"][0].as_int32(), 1);
}

/**
 * @brief 测试 variant 中 dict 的循环引用深拷贝
 */
TEST_F(VariantDeepCopyTest, DictCyclicReference)
{
    // 创建自引用的 dict
    dict d1({{"key1", 123}});
    d1["self"] = d1;

    variant v1(d1);
    variant v2 = v1.deep_copy();

    // 验证拷贝成功且保持循环结构
    EXPECT_EQ(v2["key1"], 123);
    EXPECT_TRUE(v2["self"].is_object());

    dict self_dict = v2["self"].as<dict>();
    EXPECT_EQ(self_dict.data(), v2.get_object().data()); // 验证是同一个对象

    // 验证独立性：修改拷贝不应该影响原始对象
    v2["key1"] = 456;
    EXPECT_EQ(v1["key1"].as_int32(), 123);
    EXPECT_EQ(v2["key1"].as_int32(), 456);

    // 验证拷贝和原始是不同的对象
    EXPECT_NE(v1.get_object().data(), v2.get_object().data());
}

/**
 * @brief 测试 variant 中 dict 的相互引用深拷贝
 */
TEST_F(VariantDeepCopyTest, DictMutualReference)
{
    // 创建相互引用的 dict
    dict d1({{"name", "d1"}});
    dict d2({{"name", "d2"}});
    d1["ref"] = d2;
    d2["ref"] = d1;

    variant v1(d1);
    variant v2 = v1.deep_copy();

    // 验证拷贝成功
    EXPECT_EQ(v2["name"], "d1");
    dict d2_copy = v2["ref"].as<dict>();
    EXPECT_EQ(d2_copy["name"], "d2");

    // 验证循环引用保持
    dict d1_back = d2_copy["ref"].as<dict>();
    EXPECT_EQ(d1_back.data(), v2.get_object().data()); // 验证循环

    // 验证独立性
    v2["name"] = "d1_copy";
    EXPECT_EQ(v1["name"].as_string(), "d1");
    EXPECT_EQ(v2["name"].as_string(), "d1_copy");
    EXPECT_NE(v1.get_object().data(), v2.get_object().data());
}

/**
 * @brief 测试混合 dict 和 array 的循环引用
 */
TEST_F(VariantDeepCopyTest, MixedDictArrayCyclicReference)
{
    // 创建 dict 包含 array，array 又引用回 dict
    dict     d1({{"key", 100}});
    variants arr1 = {1, 2, 3};
    d1["array"]   = arr1;
    arr1.push_back(d1); // array 引用 dict
    d1["array"] = arr1; // 更新 dict 中的 array

    variant v1(d1);
    variant v2 = v1.deep_copy();

    // 验证拷贝成功
    EXPECT_EQ(v2["key"], 100);
    EXPECT_EQ(v2["array"][0], 1);
    EXPECT_EQ(v2["array"][3].is_object(), true);

    // 验证循环引用保持
    dict d1_back = v2["array"][3].as<dict>();
    EXPECT_EQ(d1_back.data(), v2.get_object().data());

    // 验证独立性
    v2["key"] = 200;
    EXPECT_EQ(v1["key"].as_int32(), 100);
    EXPECT_EQ(v2["key"].as_int32(), 200);
}

/**
 * @brief 测试 variant 数组的循环引用
 */
TEST_F(VariantDeepCopyTest, ArrayCyclicReference)
{
    // 创建包含自引用的 variants
    variants arr1 = {1, 2, 3};
    variant  v_arr(arr1);
    arr1.push_back(v_arr); // array 引用自己

    variant v1(arr1);
    variant v2 = v1.deep_copy();

    // 验证拷贝成功
    EXPECT_EQ(v2.get_array().size(), 4);
    EXPECT_EQ(v2[0], 1);
    EXPECT_EQ(v2[1], 2);
    EXPECT_EQ(v2[2], 3);
    EXPECT_TRUE(v2[3].is_array());

    // 验证循环引用保持
    variants arr_back = v2[3].as<variants>();
    EXPECT_EQ(arr_back.data(), v2.get_array().data());

    // 验证独立性
    v2[0] = 100;
    EXPECT_EQ(v1[0].as_int32(), 1);
    EXPECT_EQ(v2[0].as_int32(), 100);
}

/**
 * @brief 测试三层循环引用（d1 -> d2 -> d3 -> d1）
 */
TEST_F(VariantDeepCopyTest, ThreeWayCyclicReference)
{
    dict d1({{"name", "d1"}});
    dict d2({{"name", "d2"}});
    dict d3({{"name", "d3"}});

    d1["next"] = d2;
    d2["next"] = d3;
    d3["next"] = d1; // 形成循环

    variant v1(d1);
    variant v2 = v1.deep_copy();

    // 验证拷贝成功
    EXPECT_EQ(v2["name"], "d1");

    dict d2_copy = v2["next"].as<dict>();
    EXPECT_EQ(d2_copy["name"], "d2");

    dict d3_copy = d2_copy["next"].as<dict>();
    EXPECT_EQ(d3_copy["name"], "d3");

    dict d1_back = d3_copy["next"].as<dict>();
    EXPECT_EQ(d1_back["name"], "d1");

    // 验证循环完整性
    EXPECT_EQ(d1_back.data(), v2.get_object().data());
    EXPECT_EQ(d2_copy.data(), v2["next"].as<dict>().data());
    EXPECT_EQ(d3_copy.data(), d2_copy["next"].as<dict>().data());

    // 验证独立性
    EXPECT_NE(v1.get_object().data(), v2.get_object().data());
}

/**
 * @brief 测试包含 null、基本类型和复杂类型的混合深拷贝
 */
TEST_F(VariantDeepCopyTest, MixedTypesDeepCopy)
{
    dict d1({{"null_val", variant()},
             {"int_val", 42},
             {"string_val", "test"},
             {"array_val", variants{1, 2, 3}},
             {"dict_val", dict({{"nested", 100}})}});

    variant v1(d1);
    variant v2 = v1.deep_copy();

    // 验证所有类型都正确拷贝
    EXPECT_TRUE(v2["null_val"].is_null());
    EXPECT_EQ(v2["int_val"], 42);
    EXPECT_EQ(v2["string_val"], "test");
    EXPECT_EQ(v2["array_val"][0], 1);
    EXPECT_EQ(v2["dict_val"]["nested"], 100);

    // 验证独立性
    v1["int_val"]            = 100;
    v1["array_val"][0]       = 111;
    v1["dict_val"]["nested"] = 222;

    EXPECT_EQ(v2["int_val"].as_int32(), 42);
    EXPECT_EQ(v2["array_val"][0].as_int32(), 1);
    EXPECT_EQ(v2["dict_val"]["nested"].as_int32(), 100);
}

/**
 * @brief 测试超大嵌套深度的深拷贝
 */
TEST_F(VariantDeepCopyTest, DeepNestingDeepCopy)
{
    // 创建深度嵌套结构
    variant v = 0;
    for (int i = 0; i < 10; ++i) {
        dict d({{"level", i}, {"data", v}});
        v = d;
    }

    variant v_copy = v.deep_copy();

    // 验证拷贝成功
    variant current = v_copy;
    for (int i = 9; i >= 0; --i) {
        EXPECT_TRUE(current.is_object());
        EXPECT_EQ(current["level"], i);
        current = current["data"];
    }
    EXPECT_EQ(current, 0);

    // 验证独立性（修改原始不影响拷贝）
    v["level"] = 999;
    EXPECT_EQ(v_copy["level"].as_int32(), 9);
}

/**
 * @brief 测试空容器的深拷贝
 *
 * @note 修复默认构造后拷贝的共享语义问题后，空容器在拷贝时会创建共享数据块。
 *       因此深拷贝后会有独立的数据块（虽然是空的），data() 不再是 nullptr。
 *       这是正确的行为，因为它确保了共享引用语义的正确性。
 */
TEST_F(VariantDeepCopyTest, EmptyContainersDeepCopy) {
    // 空 dict - 拷贝时会创建共享数据块，深拷贝后会有独立的数据块
    dict    empty_dict;
    variant v1(empty_dict);
    variant v2 = v1.deep_copy();
    EXPECT_TRUE(v2.is_object());
    EXPECT_EQ(v2.get_object().size(), 0);
    // 由于拷贝构造函数会调用 ensure_data()，数据块会被创建
    EXPECT_NE(v1.get_object().data(), nullptr);
    EXPECT_NE(v2.get_object().data(), nullptr);
    // 验证深拷贝的独立性
    EXPECT_NE(v1.get_object().data(), v2.get_object().data());

    // 空 array - 拷贝时会创建共享数据块，深拷贝后会有独立的数据块
    variants empty_arr;
    variant  v3(empty_arr);
    variant  v4 = v3.deep_copy();
    EXPECT_TRUE(v4.is_array());
    EXPECT_EQ(v4.get_array().size(), 0);
    // 由于拷贝构造函数会调用 ensure_data()，数据块会被创建
    EXPECT_NE(v3.get_array().data(), nullptr);
    EXPECT_NE(v4.get_array().data(), nullptr);
    // 验证深拷贝的独立性
    EXPECT_NE(v3.get_array().data(), v4.get_array().data());
}

TEST_F(VariantDeepCopyTest, CopyContextTracksCopiedObjects)
{
    detail::copy_context ctx;
    EXPECT_TRUE(ctx.empty());
    EXPECT_EQ(ctx.size(), 0U);

    auto original = mc::make_shared<copy_context_stub>(42);
    ctx.record_copied(original.get(), original);

    EXPECT_FALSE(ctx.empty());
    EXPECT_EQ(ctx.size(), 1U);
    EXPECT_TRUE(ctx.has_copied(original.get()));

    auto retrieved = ctx.get_copied(original.get());
    ASSERT_TRUE(retrieved != nullptr);
    EXPECT_EQ(retrieved.get(), original.get());
    EXPECT_EQ(retrieved->value(), 42);

    ctx.clear();
    EXPECT_TRUE(ctx.empty());
    EXPECT_EQ(ctx.size(), 0U);
    EXPECT_FALSE(ctx.has_copied(original.get()));
    EXPECT_EQ(ctx.get_copied(original.get()), nullptr);

    ctx.record_copied(static_cast<copy_context_stub*>(nullptr), original);
    EXPECT_TRUE(ctx.empty());
}

} // namespace test
} // namespace mc
