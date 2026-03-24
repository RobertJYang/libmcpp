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
 * @file test_variant_reference.cpp
 * @brief variant_reference 的测试用例
 */

#include <gtest/gtest.h>

#include <mc/variant.h>
#include <mc/variant/variant_extension.h>

using namespace mc;

namespace {
class reference_extension : public variant_extension<reference_extension> {
public:
    explicit reference_extension(bool allow_ref = false)
        : m_items({mc::variant(1), mc::variant("value"), mc::variant(true)}),
          m_attrs({{"flag", false}, {"token", "id"}}), m_allow_ref(allow_ref)
    {}

    reference_extension(const reference_extension&)            = default;
    reference_extension& operator=(const reference_extension&) = default;

    mc::shared_ptr<variant_extension_base> copy() const override
    {
        return mc::make_shared<reference_extension>(*this);
    }

    bool equals(const variant_extension_base& other) const override
    {
        auto* ext = dynamic_cast<const reference_extension*>(&other);
        if (ext == nullptr) {
            return false;
        }
        return m_items == ext->m_items && m_attrs == ext->m_attrs && m_allow_ref == ext->m_allow_ref;
    }

    bool supports_reference_access() const override
    {
        return m_allow_ref;
    }

    variant* get_ptr(std::size_t index) override
    {
        if (!m_allow_ref) {
            return nullptr;
        }
        return m_items.get_ptr(index);
    }

    const variant* get_ptr(std::size_t index) const override
    {
        if (!m_allow_ref) {
            return nullptr;
        }
        return m_items.get_ptr(index);
    }

    variant get(std::size_t index) const override
    {
        return m_items.at(index);
    }

    void set(std::size_t index, const variant& value) override
    {
        m_items.set(index, value);
    }

    variant get(mc::string_view key) const override
    {
        return m_attrs.at(key);
    }

    void set(mc::string_view key, const variant& value) override
    {
        m_attrs[mc::string(key)] = value;
    }

private:
    variants m_items;
    dict     m_attrs;
    bool     m_allow_ref;
};
} // namespace

class VariantReferenceTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // 创建测试用的嵌套结构
        nested_array = variants{
            1,
            2.5,
            "test",
            variants{10, 20, 30},         // 嵌套数组
            dict{{"x", 100}, {"y", 200}}, // 嵌套对象
        };

        nested_object = dict{
            {"name", "Alice"},
            {"age", 30},
            {"scores", variants{95, 88, 92}},
            {"address", dict{{"city", "Beijing"}, {"zip", 100000}}},
        };
    }

    variant nested_array;
    variant nested_object;
};

// ========== 基础功能测试 ==========

TEST_F(VariantReferenceTest, BasicIndexAccess)
{
    variant arr = variants{1, 2, 3, 4, 5};

    // 测试基本索引访问
    EXPECT_EQ(arr[0], 1);
    EXPECT_EQ(arr[1], 2);
    EXPECT_EQ(arr[4], 5);

    // 测试类型转换方法
    EXPECT_EQ(arr[0].as_int32(), 1);
    EXPECT_EQ(arr[1].as<int>(), 2);
}

TEST_F(VariantReferenceTest, BasicKeyAccess)
{
    variant obj(dict{{"name", "Bob"}, {"age", 25}, {"score", 95.5}});

    // 测试基本键访问
    EXPECT_EQ(obj["name"], "Bob");
    EXPECT_EQ(obj["age"], 25);
    EXPECT_DOUBLE_EQ(obj["score"].as_double(), 95.5);

    // 测试类型转换方法
    EXPECT_EQ(obj["name"].as_string(), "Bob");
    EXPECT_EQ(obj["age"].as<int>(), 25);
}

TEST_F(VariantReferenceTest, TypeConversionMethods)
{
    variant arr = variants{
        static_cast<int8_t>(1), static_cast<uint16_t>(100), static_cast<int64_t>(1000), 3.14, true, "hello",
    };

    // 测试各种类型转换方法
    EXPECT_EQ(arr[0].as_int8(), 1);
    EXPECT_EQ(arr[1].as_uint16(), 100);
    EXPECT_EQ(arr[2].as_int64(), 1000);
    EXPECT_DOUBLE_EQ(arr[3].as_double(), 3.14);
    EXPECT_TRUE(arr[4].as_bool());
    EXPECT_EQ(arr[5].as_string(), "hello");
}

// ========== 链式调用测试 ==========

TEST_F(VariantReferenceTest, ChainedArrayAccess)
{
    // 测试多层数组链式访问
    EXPECT_EQ(nested_array[3][0], 10);
    EXPECT_EQ(nested_array[3][1], 20);
    EXPECT_EQ(nested_array[3][2], 30);

    // 测试链式调用类型转换
    EXPECT_EQ(nested_array[3][0].as_int32(), 10);
    EXPECT_EQ(nested_array[3][1].as<int>(), 20);
}

TEST_F(VariantReferenceTest, ChainedObjectAccess)
{
    // 测试多层对象链式访问
    EXPECT_EQ(nested_object["address"]["city"], "Beijing");
    EXPECT_EQ(nested_object["address"]["zip"], 100000);

    // 测试链式调用类型转换
    EXPECT_EQ(nested_object["address"]["city"].as_string(), "Beijing");
    EXPECT_EQ(nested_object["address"]["zip"].as<int>(), 100000);
}

TEST_F(VariantReferenceTest, ChainedMixedAccess)
{
    // 测试混合访问（数组和对象）
    EXPECT_EQ(nested_array[4]["x"], 100);
    EXPECT_EQ(nested_array[4]["y"], 200);

    EXPECT_EQ(nested_object["scores"][0], 95);
    EXPECT_EQ(nested_object["scores"][1], 88);
    EXPECT_EQ(nested_object["scores"][2], 92);
}

TEST_F(VariantReferenceTest, DeepChainedAccess)
{
    // 创建深层嵌套结构
    variant deep = dict{
        {"level1", dict{{"level2", variants{dict{{"level3", variants{1, 2, 3}}}}}}},
    };

    // 测试深层链式访问
    EXPECT_EQ(deep["level1"]["level2"][0]["level3"][0], 1);
    EXPECT_EQ(deep["level1"]["level2"][0]["level3"][1], 2);
    EXPECT_EQ(deep["level1"]["level2"][0]["level3"][2], 3);
}

// ========== 算术操作符测试 ==========

TEST_F(VariantReferenceTest, ArithmeticOperators_BothReferences)
{
    variant arr = variants{10, 20, 30, 40, 50};

    // 两边都是 variant_reference
    EXPECT_EQ(arr[0] + arr[1], 30);  // 10 + 20
    EXPECT_EQ(arr[4] - arr[2], 20);  // 50 - 30
    EXPECT_EQ(arr[1] * arr[0], 200); // 20 * 10
    EXPECT_EQ(arr[3] / arr[1], 2);   // 40 / 20
    EXPECT_EQ(arr[4] % arr[2], 20);  // 50 % 30
}

TEST_F(VariantReferenceTest, ArithmeticOperators_ReferenceAndVariant)
{
    variant arr = variants{10, 20, 30};
    variant v1(5);
    variant v2(3);

    // 左边是 variant_reference，右边是 variant
    EXPECT_EQ(arr[0] + v1, 15); // 10 + 5
    EXPECT_EQ(arr[1] - v1, 15); // 20 - 5
    EXPECT_EQ(arr[2] * v2, 90); // 30 * 3
    EXPECT_EQ(arr[1] / v1, 4);  // 20 / 5
    EXPECT_EQ(arr[0] % v2, 1);  // 10 % 3

    // 右边是 variant_reference，左边是 variant（现已支持）
    EXPECT_EQ(v1 + arr[0], 15); // 5 + 10
    EXPECT_EQ(v1 - arr[0], -5); // 5 - 10
    EXPECT_EQ(v2 * arr[2], 90); // 3 * 30
}

TEST_F(VariantReferenceTest, ArithmeticOperators_ReferenceAndPrimitive)
{
    variant arr = variants{10, 20, 30};

    // 左边是 variant_reference，右边是基础类型
    EXPECT_EQ(arr[0] + 5, 15); // 10 + 5
    EXPECT_EQ(arr[1] - 5, 15); // 20 - 5
    EXPECT_EQ(arr[2] * 3, 90); // 30 * 3
    EXPECT_EQ(arr[1] / 4, 5);  // 20 / 4
    EXPECT_EQ(arr[0] % 3, 1);  // 10 % 3

    // 右边是 variant_reference，左边是基础类型（全局操作符）
    EXPECT_EQ(5 + arr[0], 15);  // 5 + 10
    EXPECT_EQ(25 - arr[1], 5);  // 25 - 20
    EXPECT_EQ(3 * arr[2], 90);  // 3 * 30
    EXPECT_EQ(100 / arr[1], 5); // 100 / 20
    EXPECT_EQ(13 % arr[0], 3);  // 13 % 10

    // 测试浮点数
    EXPECT_DOUBLE_EQ((arr[0] + 2.5).as_double(), 12.5);
    EXPECT_DOUBLE_EQ((arr[1] * 1.5).as_double(), 30.0);
    EXPECT_DOUBLE_EQ((2.5 + arr[0]).as_double(), 12.5); // 基础类型在左边
    EXPECT_DOUBLE_EQ((1.5 * arr[1]).as_double(), 30.0); // 基础类型在左边
}

TEST_F(VariantReferenceTest, StringConcatenation)
{
    variant arr = variants{"Hello", " ", "World"};

    // 字符串拼接（两边都是 variant_reference）
    EXPECT_EQ((arr[0] + arr[1]).as_string(), "Hello ");
    // 链式拼接
    EXPECT_EQ((arr[0] + arr[1] + arr[2]).as_string(), "Hello World");

    // 与字符串字面量拼接
    EXPECT_EQ((arr[0] + " World").as_string(), "Hello World");
}

// ========== 位操作符测试 ==========

TEST_F(VariantReferenceTest, BitwiseOperators_BothReferences)
{
    variant arr = variants{12, 10, 5, 3, 2}; // 二进制: 1100, 1010, 0101, 0011, 0010

    // 两边都是 variant_reference
    EXPECT_EQ(arr[0] & arr[1], 8);   // 1100 & 1010 = 1000 (8)
    EXPECT_EQ(arr[0] | arr[2], 13);  // 1100 | 0101 = 1101 (13)
    EXPECT_EQ(arr[0] ^ arr[1], 6);   // 1100 ^ 1010 = 0110 (6)
    EXPECT_EQ(arr[1] << arr[4], 40); // 10 << 2 = 40
    EXPECT_EQ(arr[1] >> arr[4], 2);  // 10 >> 2 = 2
}

TEST_F(VariantReferenceTest, BitwiseOperators_ReferenceAndVariant)
{
    variant arr = variants{12, 10, 5};
    variant v1(3);
    variant v2(2);

    // 左边是 variant_reference，右边是 variant
    EXPECT_EQ(arr[0] & v1, 0);   // 1100 & 0011 = 0000
    EXPECT_EQ(arr[1] | v1, 11);  // 1010 | 0011 = 1011
    EXPECT_EQ(arr[2] ^ v1, 6);   // 0101 ^ 0011 = 0110
    EXPECT_EQ(arr[1] << v2, 40); // 10 << 2 = 40
    EXPECT_EQ(arr[1] >> v2, 2);  // 10 >> 2 = 2
}

TEST_F(VariantReferenceTest, BitwiseOperators_ReferenceAndPrimitive)
{
    variant arr = variants{12, 10, 5};

    // 左边是 variant_reference，右边是基础类型
    EXPECT_EQ(arr[0] & 3, 0);   // 1100 & 0011 = 0000
    EXPECT_EQ(arr[1] | 3, 11);  // 1010 | 0011 = 1011
    EXPECT_EQ(arr[2] ^ 3, 6);   // 0101 ^ 0011 = 0110
    EXPECT_EQ(arr[1] << 2, 40); // 10 << 2 = 40
    EXPECT_EQ(arr[1] >> 2, 2);  // 10 >> 2 = 2

    // 右边是 variant_reference，左边是基础类型（全局操作符）
    EXPECT_EQ(3 & arr[0], 0);   // 0011 & 1100 = 0000
    EXPECT_EQ(3 | arr[1], 11);  // 0011 | 1010 = 1011
    EXPECT_EQ(3 ^ arr[2], 6);   // 0011 ^ 0101 = 0110
    EXPECT_EQ(2 << arr[2], 64); // 2 << 5 = 64
    EXPECT_EQ(40 >> arr[1], 0); // 40 >> 10 = 0

    // 测试按位取反
    EXPECT_EQ((~arr[0]).as_int32(), ~12);
}

// ========== 比较操作符测试 ==========

TEST_F(VariantReferenceTest, ComparisonOperators_BothReferences)
{
    variant arr = variants{10, 20, 30, 20, 10};

    // 两边都是 variant_reference
    EXPECT_TRUE(arr[0] == arr[4]); // 10 == 10
    EXPECT_TRUE(arr[1] == arr[3]); // 20 == 20
    EXPECT_TRUE(arr[0] != arr[1]); // 10 != 20
    EXPECT_TRUE(arr[0] < arr[1]);  // 10 < 20
    EXPECT_TRUE(arr[2] > arr[1]);  // 30 > 20
    EXPECT_TRUE(arr[0] <= arr[4]); // 10 <= 10
    EXPECT_TRUE(arr[2] >= arr[1]); // 30 >= 20
}

TEST_F(VariantReferenceTest, ComparisonOperators_ReferenceAndVariant)
{
    variant arr = variants{10, 20, 30};
    variant v1(10);
    variant v2(20);

    // 左边是 variant_reference，右边是 variant
    EXPECT_TRUE(arr[0] == v1);
    EXPECT_TRUE(arr[1] == v2);
    EXPECT_TRUE(arr[0] != v2);
    EXPECT_TRUE(arr[0] < v2);
    EXPECT_TRUE(arr[2] > v2);
    EXPECT_TRUE(arr[0] <= v1);
    EXPECT_TRUE(arr[2] >= v2);

    // 右边是 variant_reference，左边是 variant
    EXPECT_TRUE(v1 == arr[0]);
    EXPECT_TRUE(v2 == arr[1]);
    EXPECT_TRUE(v2 != arr[0]);
    EXPECT_TRUE(v1 < arr[1]);
    EXPECT_TRUE(v2 < arr[2]);
}

TEST_F(VariantReferenceTest, ComparisonOperators_ReferenceAndPrimitive)
{
    variant arr = variants{10, 20, 30};

    // 左边是 variant_reference，右边是基础类型（整数）
    EXPECT_TRUE(arr[0] == 10);
    EXPECT_TRUE(arr[1] == 20);
    EXPECT_TRUE(arr[0] != 20);
    EXPECT_TRUE(arr[0] < 20);
    EXPECT_TRUE(arr[2] > 20);
    EXPECT_TRUE(arr[0] <= 10);
    EXPECT_TRUE(arr[2] >= 30);

    // 右边是 variant_reference，左边是基础类型（全局操作符）
    EXPECT_TRUE(10 == arr[0]);
    EXPECT_TRUE(20 == arr[1]);
    EXPECT_TRUE(20 != arr[0]);
    EXPECT_TRUE(5 < arr[1]);  // 5 < 20
    EXPECT_TRUE(40 > arr[2]); // 40 > 30
    EXPECT_TRUE(10 <= arr[0]);
    EXPECT_TRUE(30 >= arr[2]);

    // 测试浮点数比较
    variant arr2 = variants{1.5, 2.5, 3.5};
    EXPECT_TRUE(arr2[0] == 1.5);
    EXPECT_TRUE(arr2[1] > 2.0);
    EXPECT_TRUE(arr2[2] < 4.0);
    // 基础类型在左边
    EXPECT_TRUE(1.5 == arr2[0]);
    EXPECT_TRUE(2.0 < arr2[1]);
    EXPECT_TRUE(4.0 > arr2[2]);

    // 测试布尔值比较
    variant arr3 = variants{true, false};
    EXPECT_TRUE(arr3[0] == true);
    EXPECT_TRUE(arr3[1] == false);
    // 基础类型在左边
    EXPECT_TRUE(true == arr3[0]);
    EXPECT_TRUE(false == arr3[1]);
}

TEST_F(VariantReferenceTest, ComparisonOperators_StringView)
{
    variant obj(dict{{"name", "Alice"}, {"city", "Beijing"}});

    // 左边是 variant_reference，右边是 string_view
    EXPECT_TRUE(obj["name"] == "Alice");
    EXPECT_TRUE(obj["city"] == "Beijing");
    EXPECT_TRUE(obj["name"] != "Bob");
    EXPECT_TRUE(obj["name"] < "Bob"); // 字典序比较
    EXPECT_TRUE(obj["city"] > "Abc");

    // 右边是 variant_reference，左边是 string_view（全局操作符）
    EXPECT_TRUE("Alice" == obj["name"]);
    EXPECT_TRUE("Beijing" == obj["city"]);
    EXPECT_TRUE("Bob" != obj["name"]);
    EXPECT_TRUE("Bob" > obj["name"]); // "Bob" > "Alice"
    EXPECT_TRUE("Abc" < obj["city"]); // "Abc" < "Beijing"
}

TEST_F(VariantReferenceTest, ComparisonOperators_Array)
{
    variant arr = variants{variants{1, 2, 3}, variants{1, 2, 3}, variants{1, 2, 4}};

    // 与数组比较（只支持 == 和 !=）
    EXPECT_TRUE(arr[0] == arr[1]);
    EXPECT_TRUE(arr[0] != arr[2]);
    // array 类型不支持 < 比较操作符

    // 与 std::vector 比较
    std::vector<variant> vec1{1, 2, 3};
    std::vector<variant> vec2{1, 2, 4};
    EXPECT_TRUE(arr[0] == vec1);
    EXPECT_TRUE(arr[2] == vec2);
    // EXPECT_TRUE(arr[0] < vec2);  // array 不支持 < 比较
}

TEST_F(VariantReferenceTest, ComparisonOperators_Dict)
{
    variant arr = variants{
        dict{{"x", 1}, {"y", 2}},
        dict{{"x", 1}, {"y", 2}},
        dict{{"x", 1}, {"y", 3}},
    };

    // 与 dict 比较
    EXPECT_TRUE(arr[0] == arr[1]);
    EXPECT_TRUE(arr[0] != arr[2]);

    // 与 dict 对象比较
    dict d1{{"x", 1}, {"y", 2}};
    dict d2{{"x", 1}, {"y", 3}};
    EXPECT_TRUE(arr[0] == d1);
    EXPECT_TRUE(arr[2] == d2);
}

// ========== 复合赋值操作符测试 ==========

TEST_F(VariantReferenceTest, CompoundAssignmentOperators_Variant)
{
    variant arr = variants{10, 20, 30, 40};

    // 复合赋值操作符
    arr[0] += variant(5);
    EXPECT_EQ(arr[0], 15);

    arr[1] -= variant(5);
    EXPECT_EQ(arr[1], 15);

    arr[2] *= variant(2);
    EXPECT_EQ(arr[2], 60);

    arr[3] /= variant(4);
    EXPECT_EQ(arr[3], 10);
}

TEST_F(VariantReferenceTest, CompoundAssignmentOperators_Primitive)
{
    variant arr = variants{10, 20, 30, 40, 12};

    // 与基础类型的复合赋值
    arr[0] += 5;
    EXPECT_EQ(arr[0], 15);

    arr[1] -= 5;
    EXPECT_EQ(arr[1], 15);

    arr[2] *= 2;
    EXPECT_EQ(arr[2], 60);

    arr[3] /= 4;
    EXPECT_EQ(arr[3], 10);

    arr[4] %= 5;
    EXPECT_EQ(arr[4], 2);
}

TEST_F(VariantReferenceTest, CompoundAssignmentOperators_Bitwise)
{
    variant arr = variants{12, 10, 5, 40, 10};

    // 位操作的复合赋值
    arr[0] &= 3;
    EXPECT_EQ(arr[0], 0);

    arr[1] |= 5;
    EXPECT_EQ(arr[1], 15);

    arr[2] ^= 3;
    EXPECT_EQ(arr[2], 6);

    arr[3] <<= 2;
    EXPECT_EQ(arr[3], 160);

    arr[4] >>= 2;
    EXPECT_EQ(arr[4], 2);
}

TEST_F(VariantReferenceTest, CompoundAssignmentOperators_String)
{
    variant obj(dict{{"greeting", "Hello"}});

    // 字符串拼接复合赋值
    obj["greeting"] += " World";
    EXPECT_EQ(obj["greeting"], "Hello World");
}

// ========== 自增自减操作符测试 ==========

TEST_F(VariantReferenceTest, IncrementDecrementOperators)
{
    variant arr = variants{10, 20};

    // 前置自增
    ++arr[0];
    EXPECT_EQ(arr[0], 11);

    // 前置自减
    --arr[1];
    EXPECT_EQ(arr[1], 19);

    // 后置自增
    variant old_val = arr[0]++;
    EXPECT_EQ(old_val, 11);
    EXPECT_EQ(arr[0], 12);

    // 后置自减
    old_val = arr[1]--;
    EXPECT_EQ(old_val, 19);
    EXPECT_EQ(arr[1], 18);
}

// ========== 一元操作符测试 ==========

TEST_F(VariantReferenceTest, UnaryOperators)
{
    variant arr = variants{10, 0, true, false};

    // 一元负号
    EXPECT_EQ(-arr[0], -10);

    // 逻辑非
    EXPECT_EQ(!arr[1], true);
    EXPECT_EQ(!arr[2], false);
    EXPECT_EQ(!arr[3], true);

    // 显式转换为 bool
    EXPECT_TRUE(static_cast<bool>(arr[0]));
    EXPECT_FALSE(static_cast<bool>(arr[1]));
    EXPECT_TRUE(static_cast<bool>(arr[2]));
    EXPECT_FALSE(static_cast<bool>(arr[3]));
}

// ========== 修改操作测试 ==========

TEST_F(VariantReferenceTest, ModificationThroughReference)
{
    variant arr = variants{1, 2, 3};

    // 通过引用修改值
    arr[0] = 10;
    arr[1] = 20;
    arr[2] = 30;

    EXPECT_EQ(arr[0], 10);
    EXPECT_EQ(arr[1], 20);
    EXPECT_EQ(arr[2], 30);

    // 修改为不同类型
    arr[0] = "text";
    arr[1] = 3.14;
    arr[2] = true;

    EXPECT_EQ(arr[0], "text");
    EXPECT_DOUBLE_EQ(arr[1].as_double(), 3.14);
    EXPECT_EQ(arr[2], true);
}

TEST_F(VariantReferenceTest, ModificationInNestedStructure)
{
    // 修改嵌套数组中的值
    nested_array[3][0] = 100;
    EXPECT_EQ(nested_array[3][0], 100);

    // 修改嵌套对象中的值
    nested_object["address"]["city"] = "Shanghai";
    EXPECT_EQ(nested_object["address"]["city"], "Shanghai");
}

// ========== 其他方法测试 ==========

TEST_F(VariantReferenceTest, UtilityMethods)
{
    variant obj(dict{{"name", "Alice"}, {"age", 30}, {"scores", variants{95, 88, 92}}});

    // 测试 size
    EXPECT_EQ(obj["scores"].size(), 3);

    // 测试 contains（用于对象键检查）
    EXPECT_TRUE(obj.contains("name"));
    EXPECT_TRUE(obj.contains("scores"));
    EXPECT_FALSE(obj.contains("not_exist"));

    // 测试 get_type_name
    EXPECT_EQ(obj["name"].get_type_name(), mc::string_view("string"));
    EXPECT_EQ(obj["age"].get_type_name(), mc::string_view("int32"));
    EXPECT_EQ(obj["scores"].get_type_name(), mc::string_view("array"));
}

TEST_F(VariantReferenceTest, GetterMethods)
{
    variant arr = variants{
        "hello",
        variants{1, 2, 3},
        dict{{"x", 10}},
    };

    // 测试 get_string
    EXPECT_EQ(arr[0].get_string(), "hello");

    // 测试 get_array
    const auto& array_ref = arr[1].get_array();
    EXPECT_EQ(array_ref.size(), 3);
    EXPECT_EQ(array_ref[0], 1);

    // 测试 get_object
    const auto& obj_ref = arr[2].get_object();
    EXPECT_EQ(obj_ref["x"], 10);
}

TEST_F(VariantReferenceTest, DeepCopy)
{
    variant arr({1, 2, variants{3, 4}});

    // 深拷贝
    variant copy = arr[2].deep_copy();

    // 修改拷贝不影响原对象
    copy[0] = 100;
    EXPECT_EQ(arr[2][0], 3);
    EXPECT_EQ(copy[0], 100);
}

TEST_F(VariantReferenceTest, ClearMethod)
{
    variant arr({1, 2, variants{3, 4, 5}});

    // 清空嵌套数组
    arr[2].clear();
    EXPECT_TRUE(arr[2].is_null());
}

// ========== 隐式转换测试 ==========

TEST_F(VariantReferenceTest, ImplicitConversion)
{
    variant arr = variants{1, 2, 3};

    // 隐式转换为 variant
    variant v1 = arr[0];
    variant v2 = arr[1];

    EXPECT_EQ(v1, 1);
    EXPECT_EQ(v2, 2);

    // 作为参数传递
    auto test_func = [](const variant& v) {
        return v.as_int32();
    };
    EXPECT_EQ(test_func(arr[0]), 1);
    EXPECT_EQ(test_func(arr[1]), 2);
}

// ========== 混合操作测试 ==========

TEST_F(VariantReferenceTest, MixedOperations)
{
    variant arr = variants{10, 20, 30, 40};

    // 混合操作：链式调用 + 算术操作 + 比较
    EXPECT_TRUE((arr[0] + arr[1]) == 30);
    EXPECT_FALSE((arr[2] - arr[0]) > arr[1]); // 30 - 10 = 20, 不大于 20
    EXPECT_TRUE((arr[1] * 2) == arr[3]);      // 20 * 2 = 40

    // 复杂表达式
    auto result = (arr[0] + arr[1]) * arr[2] / 10;
    EXPECT_EQ(result, 90);
}

TEST_F(VariantReferenceTest, ChainedOperations)
{
    // 测试链式操作的综合使用
    variant complex = dict{
        {"data",
         variants{
             dict{{"value", 10}},
             dict{{"value", 20}},
             dict{{"value", 30}},
         }},
    };

    // 复杂的链式访问和操作
    EXPECT_EQ(complex["data"][0]["value"], 10);
    EXPECT_EQ(complex["data"][1]["value"], 20);
    EXPECT_TRUE(complex["data"][0]["value"] < complex["data"][1]["value"]);
    EXPECT_TRUE((complex["data"][0]["value"] + complex["data"][1]["value"]) == 30);

    // 修改通过链式访问
    complex["data"][2]["value"] = 40;
    EXPECT_EQ(complex["data"][2]["value"], 40);
}

TEST_F(VariantReferenceTest, OperatorPrecedence)
{
    variant arr = variants{2, 3, 4, 5};

    // 测试运算符优先级
    EXPECT_EQ(arr[0] + arr[1] * arr[2], 14);   // 2 + 3 * 4 = 14
    EXPECT_EQ((arr[0] + arr[1]) * arr[2], 20); // (2 + 3) * 4 = 20
    EXPECT_TRUE(arr[0] < arr[1] && arr[2] < arr[3]);
}

// ========== 边界和特殊情况测试 ==========

TEST_F(VariantReferenceTest, NullValue)
{
    variant arr = variants{nullptr, 10, nullptr};

    // 测试 null 值
    EXPECT_TRUE(arr[0].is_null());
    EXPECT_FALSE(arr[1].is_null());
    EXPECT_TRUE(arr[2].is_null());
}

TEST_F(VariantReferenceTest, BooleanContext)
{
    variant arr = variants{0, 1, false, true, "", "text", nullptr};

    // 在布尔上下文中使用
    EXPECT_FALSE(static_cast<bool>(arr[0])); // 0
    EXPECT_TRUE(static_cast<bool>(arr[1]));  // 1
    EXPECT_FALSE(static_cast<bool>(arr[2])); // false
    EXPECT_TRUE(static_cast<bool>(arr[3]));  // true
}

TEST_F(VariantReferenceTest, LargeNumbers)
{
    variant arr = variants{
        static_cast<int64_t>(9223372036854775807LL),    // max int64
        static_cast<uint64_t>(18446744073709551615ULL), // max uint64
    };

    // 测试大数值
    EXPECT_EQ(arr[0].as_int64(), 9223372036854775807LL);
    EXPECT_EQ(arr[1].as_uint64(), 18446744073709551615ULL);
}

// ========== from_variant 测试 ==========

TEST_F(VariantReferenceTest, FromVariantConversion)
{
    variant arr = variants{10, 20, 30};

    // 测试 from_variant 重载
    int         val1 = 0;
    double      val2 = 0.0;
    mc::string val3;

    from_variant(arr[0], val1);
    EXPECT_EQ(val1, 10);

    variant arr2 = variants{3.14, "hello"};
    from_variant(arr2[0], val2);
    EXPECT_DOUBLE_EQ(val2, 3.14);

    from_variant(arr2[1], val3);
    EXPECT_EQ(val3, "hello");
}

// ========== std::vector 构造函数测试 ==========

TEST_F(VariantReferenceTest, VectorConstructorWeakType)
{
    // 测试从 std::vector 构造为弱类型数组
    std::vector<int> vec        = {1, 2, 3};
    variants         weak_array = vec;

    EXPECT_EQ(weak_array.size(), 3);
    EXPECT_EQ(weak_array[0], 1);
    EXPECT_EQ(weak_array[1], 2);
    EXPECT_EQ(weak_array[2], 3);

    // 测试可以修改为不同类型（弱类型特性）
    weak_array[0] = "text";
    weak_array[1] = 3.14;
    weak_array[2] = true;

    EXPECT_EQ(weak_array[0], "text");
    EXPECT_DOUBLE_EQ(weak_array[1].as_double(), 3.14);
    EXPECT_EQ(weak_array[2], true);

    std::vector<mc::variant> vec_1       = {1, 2, 3};
    variants                 weak_array1 = std::move(vec_1);
    EXPECT_EQ(weak_array1.size(), 3);
    EXPECT_EQ(weak_array1[0], 1);
    EXPECT_EQ(weak_array1[1], 2);
    EXPECT_EQ(weak_array1[2], 3);
}

TEST_F(VariantReferenceTest, ExtensionAccessWithoutDirectReference)
{
    auto    ext = mc::make_shared<reference_extension>(false);
    variant v(ext);

    variant_reference index_ref = v[1];
    EXPECT_EQ(index_ref.get().as_string(), "value");
    index_ref = variant("patched");
    EXPECT_EQ(v[1].as_string(), "patched");

    variant_reference key_ref = v["flag"];
    EXPECT_FALSE(key_ref.get().as_bool());
    key_ref = variant(true);
    EXPECT_TRUE(v["flag"].as_bool());
}

TEST_F(VariantReferenceTest, ExtensionAccessWithDirectReference)
{
    auto    ext = mc::make_shared<reference_extension>(true);
    variant v(ext);

    variant_reference first_ref = v[0];
    first_ref.get()             = variant(9);
    EXPECT_EQ(v[0].as_int32(), 9);

    variant_reference bool_ref = v[2];
    bool_ref                   = variant(false);
    EXPECT_FALSE(v[2].as_bool());

    variant_reference target_ref = v[0];
    target_ref                   = std::move(bool_ref);
    EXPECT_FALSE(target_ref.get().as_bool());
    EXPECT_FALSE(v[0].as_bool());
}

// 测试 variant_reference 的 swap 和 operator*/operator->
TEST_F(VariantReferenceTest, ReferenceSwapAndDereference)
{
    dict d;
    d["key1"] = 100;
    d["key2"] = 200;
    variant v(d);

    // 获取字典元素的引用
    variant_reference ref1 = v["key1"];
    variant_reference ref2 = v["key2"];

    // 测试 operator*
    EXPECT_EQ((*ref1).as_int32(), 100);
    EXPECT_EQ((*ref2).as_int32(), 200);

    // 测试 operator->
    EXPECT_EQ(ref1->as_int32(), 100);
    EXPECT_EQ(ref2->as_int32(), 200);

    // 测试 swap
    ref1.swap(ref2);
    EXPECT_EQ((*ref1).as_int32(), 200);
    EXPECT_EQ((*ref2).as_int32(), 100);

    // 验证原始字典也被交换
    EXPECT_EQ(v["key1"].as_int32(), 200);
    EXPECT_EQ(v["key2"].as_int32(), 100);

    // 测试 array 的 variant_reference 触发 cached_value
    variants          arr{10, 20, 30};
    variant           v_arr(arr);
    variant_reference arr_ref = v_arr[1];

    // 访问引用应该触发 cached_value 的惰性加载
    EXPECT_EQ((*arr_ref).as_int32(), 20);
    EXPECT_EQ(arr_ref->as_int32(), 20);
}

TEST_F(VariantReferenceTest, TypeCheckMethods)
{
    variant arr = variants{1, 2.5, "text", true, nullptr};

    // 测试类型判断方法
    // EXPECT_TRUE(arr[0].is_int()); // is_int() 不存在，使用 is<int>() 替代
    EXPECT_TRUE(arr[1].is_double());
    EXPECT_TRUE(arr[2].is_string());
    EXPECT_TRUE(arr[3].is_bool());
    EXPECT_TRUE(arr[4].is_null());

    EXPECT_FALSE(arr[0].is_string());
    EXPECT_FALSE(arr[1].is_int64()); // 默认整数类型是 int64
}
