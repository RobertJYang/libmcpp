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
 * @file test_variant_equality.cpp
 * @brief 测试 variant、typed_variant、dict 和 mutable_dict 类的相等运算符
 */
#include "test_variant_helpers.h"
#include <gtest/gtest.h>
#include <limits>
#include <mc/dict.h>
#include <mc/variant.h>
#include <stdexcept>

namespace mc {
namespace test {

class VariantEqualityTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 在每个测试前执行
    }

    void TearDown() override {
        // 在每个测试后执行
    }
};

/**
 * @brief 测试 variant 类型之间的相等比较
 */
TEST_F(VariantEqualityTest, VariantEquality) {
    // 测试基本类型的相等性
    variant v1(42), v2(42);
    ASSERT_EQ(v1, v2) << "相同的整数值应该相等";

    variant v3(3.14), v4(3.14);
    ASSERT_EQ(v3, v4) << "相同的浮点数值应该相等";

    variant v5("test"), v6("test");
    ASSERT_EQ(v5, v6) << "相同的字符串应该相等";

    // 测试不同类型的比较
    variant v7(42), v8(42.0);
    ASSERT_EQ(v7, v8) << "不同类型的值相等则应该相等";

    // 测试边界值
    variant v9(std::numeric_limits<int64_t>::max());
    variant v10(std::numeric_limits<int64_t>::max());
    ASSERT_EQ(v9, v10) << "最大整数值应该相等";

    // 测试空值比较
    variant v11, v12;
    ASSERT_EQ(v11, v12) << "空值应该相等";

    // 测试二进制数据比较
    blob    b1{{1, 2, 3}}, b2{{1, 2, 3}};
    variant v13(b1), v14(b2);
    ASSERT_EQ(v13, v14) << "相同的二进制数据应该相等";
}

/**
 * @brief 测试 typed_variant 类型之间的相等比较
 */
TEST_F(VariantEqualityTest, TypedVariantEquality) {
    // 测试相同类型的比较
    typed_variant tv1(42), tv2(42);
    ASSERT_EQ(tv1, tv2) << "相同类型和值的 typed_variant 应该相等";

    // 测试类型锁定后的比较
    typed_variant tv3(type_id::int32_type);
    typed_variant tv4(type_id::int32_type);
    tv3 = 42;
    tv4 = 42;
    ASSERT_EQ(tv3, tv4) << "类型锁定后的相同值应该相等";

    // 测试与普通 variant 的比较
    variant v(42);
    ASSERT_EQ(tv1, v) << "typed_variant 应该可以与普通 variant 比较";

    // 测试不同类型的比较
    typed_variant tv5(type_id::int32_type);
    typed_variant tv6(type_id::double_type);
    tv5 = 42;
    tv6 = 42.0;
    ASSERT_EQ(tv5, tv6) << "不同类型的值相等则应该相等";
}

/**
 * @brief 测试 dict 类型之间的相等比较
 */
TEST_F(VariantEqualityTest, DictEquality) {
    // 创建两个相同的字典
    mutable_dict md1, md2;
    md1["int"]    = 42;
    md1["string"] = "test";
    md1["bool"]   = true;

    md2["int"]    = 42;
    md2["string"] = "test";
    md2["bool"]   = true;

    dict d1(md1), d2(md2);
    ASSERT_EQ(d1, d2) << "相同内容的字典应该相等";

    // 测试不同顺序的键值对
    mutable_dict md3;
    md3["bool"]   = true;
    md3["string"] = "test";
    md3["int"]    = 42;

    dict d3(md3);
    ASSERT_EQ(d1, d3) << "键值对顺序不同的字典应该相等";

    // 测试嵌套字典
    mutable_dict md4, md5;
    md4["nested"] = md1;
    md5["nested"] = md2;

    dict d4(md4), d5(md5);
    ASSERT_EQ(d4, d5) << "包含嵌套字典的字典应该相等";

    // 测试空字典
    dict d6, d7;
    ASSERT_EQ(d6, d7) << "空字典应该相等";
}

/**
 * @brief 测试 mutable_dict 类型之间的相等比较
 */
TEST_F(VariantEqualityTest, MutableDictEquality) {
    // 创建两个相同的可变字典
    mutable_dict md1, md2;
    md1["int"]    = 42;
    md1["string"] = "test";

    md2["int"]    = 42;
    md2["string"] = "test";

    ASSERT_EQ(md1, md2) << "相同内容的可变字典应该相等";

    // 测试与普通字典的比较
    dict d(md1);
    ASSERT_EQ(md1, d) << "可变字典应该可以与普通字典比较";

    // 测试修改后的比较
    md2["int"] = 43;
    ASSERT_NE(md1, md2) << "内容不同的可变字典不应该相等";

    // 测试复杂数据结构
    mutable_dict md3, md4;
    variants     arr1{1, 2, 3};
    variants     arr2{1, 2, 3};

    md3["array"] = arr1;
    md3["dict"]  = md1;

    md4["array"] = arr2;
    md4["dict"]  = md1;

    ASSERT_EQ(md3, md4) << "包含数组和嵌套字典的可变字典应该相等";
}

/**
 * @brief 测试混合类型的相等比较
 */
TEST_F(VariantEqualityTest, MixedTypeEquality) {
    // 测试 variant 中的 dict 比较
    mutable_dict md1, md2;
    md1["key"] = "value";
    md2["key"] = "value";

    variant v1(md1), v2(md2);
    ASSERT_EQ(v1, v2) << "包含相同字典的 variant 应该相等";

    // 测试 variant 中的数组比较
    variants arr1{1, "test", true};
    variants arr2{1, "test", true};

    variant v3(arr1), v4(arr2);
    ASSERT_EQ(v3, v4) << "包含相同数组的 variant 应该相等";

    // 测试复杂嵌套结构
    mutable_dict md3;
    md3["array"] = arr1;
    md3["dict"]  = md1;
    md3["value"] = 42;

    mutable_dict md4;
    md4["array"] = arr2;
    md4["dict"]  = md2;
    md4["value"] = 42;

    variant v5(md3), v6(md4);
    ASSERT_EQ(v5, v6) << "包含复杂嵌套结构的 variant 应该相等";

    // 测试边界情况
    variant v7(std::numeric_limits<int64_t>::max());
    variant v8(std::numeric_limits<uint64_t>::max());
    ASSERT_NE(v7, v8) << "不同类型的最大值不应该相等";

    // 测试特殊值
    variant v9(0.0), v10(-0.0);
    ASSERT_EQ(v9, v10) << "正零和负零应该相等";
}

} // namespace test
} // namespace mc