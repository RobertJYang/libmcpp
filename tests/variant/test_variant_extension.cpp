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
 * @file test_variant_extension.cpp
 * @brief 测试 variant 扩展类型功能
 */
#include <chrono>
#include <gtest/gtest.h>
#include <iostream>
#include <mc/exception.h>
#include <mc/variant.h>
#include <mc/variant/variant_extension.h>
#include <test_utilities/test_base.h>

namespace mc {
namespace test {

// 测试用的扩展类型
class test_extension : public variant_extension<test_extension> {
public:
    test_extension() : m_value(0) {
    }
    explicit test_extension(int value) : m_value(value) {
    }
    test_extension(const test_extension& other) : m_value(other.m_value) {
    }
    test_extension& operator=(const test_extension& other) {
        if (this != &other) {
            m_value = other.m_value;
        }
        return *this;
    }

    bool operator==(const test_extension& other) const {
        return m_value == other.m_value;
    }

    std::size_t hash() const override {
        return std::hash<int>()(m_value);
    }

    int get_value() const {
        return m_value;
    }
    void set_value(int value) {
        m_value = value;
    }

    int64_t as_int64() const override {
        return m_value;
    }

    uint64_t as_uint64() const override {
        return m_value;
    }

    double as_double() const override {
        return m_value;
    }

    bool as_bool() const override {
        return m_value != 0;
    }

    std::string as_string() const override {
        return "test_extension(" + std::to_string(m_value) + ")";
    }

    mc::shared_ptr<variant_extension_base> copy() const override {
        return mc::make_shared<test_extension>(*this);
    }

private:
    int m_value;
};

// 另一个测试用的扩展类型
class complex_extension : public variant_extension<complex_extension> {
public:
    complex_extension() : m_name(""), m_count(0) {
    }
    complex_extension(std::string name, int count) : m_name(std::move(name)), m_count(count) {
    }
    complex_extension(const complex_extension& other) : m_name(other.m_name), m_count(other.m_count) {
    }
    complex_extension& operator=(const complex_extension& other) {
        if (this != &other) {
            m_name  = other.m_name;
            m_count = other.m_count;
        }
        return *this;
    }

    bool operator==(const complex_extension& other) const {
        return m_name == other.m_name && m_count == other.m_count;
    }

    std::size_t hash() const override {
        std::size_t h1 = std::hash<std::string>()(m_name);
        std::size_t h2 = std::hash<int>()(m_count);
        return h1 ^ (h2 << 1);
    }

    std::string as_string() const override {
        return "complex_extension(name=" + m_name + ", count=" + std::to_string(m_count) + ")";
    }

    const std::string& get_name() const {
        return m_name;
    }
    int get_count() const {
        return m_count;
    }

private:
    std::string m_name;
    int         m_count;
};

// 支持零开销引用访问的数组扩展类型
class variant_array_extension : public variant_extension<variant_array_extension> {
public:
    variant_array_extension() = default;

    explicit variant_array_extension(variants data)
        : m_data(std::move(data)) {
    }

    bool operator==(const variant_array_extension& other) const {
        return m_data == other.m_data;
    }

    // ✅ 启用零开销引用访问
    bool supports_reference_access() const override {
        return true;
    }

    // ✅ 返回内部 variant 指针（零开销）
    mc::variant* get_ptr(std::size_t index) override {
        if (index < m_data.size()) {
            return &m_data[index];
        }
        return nullptr;
    }

    const mc::variant* get_ptr(std::size_t index) const override {
        if (index < m_data.size()) {
            return &m_data[index];
        }
        return nullptr;
    }

    // 值访问（向后兼容，但优化路径不会使用）
    mc::variant get(std::size_t index) const override {
        if (index >= m_data.size()) {
            throw std::out_of_range("索引越界");
        }
        return m_data[index];
    }

    void set(std::size_t index, const mc::variant& value) override {
        if (index >= m_data.size()) {
            throw std::out_of_range("索引越界");
        }
        m_data[index] = value;
    }

    std::size_t hash() const override {
        std::size_t h = 0;
        for (const auto& v : m_data) {
            h ^= v.hash() + 0x9e3779b9 + (h << 6) + (h >> 2);
        }
        return h;
    }

    std::string as_string() const override {
        return "variant_array_extension[" + std::to_string(m_data.size()) + "]";
    }

    const variants& get_data() const {
        return m_data;
    }

private:
    variants m_data;
};

// 支持零开销引用访问的对象扩展类型
class variant_object_extension : public variant_extension<variant_object_extension> {
public:
    variant_object_extension() = default;

    explicit variant_object_extension(dict data)
        : m_data(std::move(data)) {
    }

    bool operator==(const variant_object_extension& other) const {
        return m_data == other.m_data;
    }

    // ✅ 启用零开销引用访问
    bool supports_reference_access() const override {
        return true;
    }

    // ✅ 返回内部 variant 指针（键访问，零开销）
    mc::variant* get_ptr(std::string_view key) override {
        if (m_data.contains(key)) {
            return &m_data[key];
        }
        return nullptr;
    }

    const mc::variant* get_ptr(std::string_view key) const override {
        if (m_data.contains(key)) {
            return &const_cast<dict&>(m_data)[key];
        }
        return nullptr;
    }

    // 值访问（向后兼容，但优化路径不会使用）
    mc::variant get(std::string_view key) const override {
        if (!m_data.contains(key)) {
            throw std::out_of_range("键不存在");
        }
        return m_data[key];
    }

    void set(std::string_view key, const mc::variant& value) override {
        m_data[std::string(key)] = value;
    }

    std::size_t hash() const override {
        return m_data.hash();
    }

    std::string as_string() const override {
        return "variant_object_extension{" + std::to_string(m_data.size()) + " keys}";
    }

    const dict& get_data() const {
        return m_data;
    }

private:
    dict m_data;
};

class VariantExtensionTest : public mc::test::TestBase {
protected:
    void SetUp() override {
    }
    void TearDown() override {
    }
};

TEST_F(VariantExtensionTest, VariantExtensionConstruction) {
    // 测试 variant 从扩展类型构造
    variant v(mc::make_shared<complex_extension>("test", 10));

    EXPECT_TRUE(v.is_extension());

    auto result = mc::dynamic_pointer_cast<complex_extension>(v.as_extension());
    EXPECT_TRUE(result != nullptr);
    EXPECT_EQ(result->get_name(), "test");
    EXPECT_EQ(result->get_count(), 10);
}

TEST_F(VariantExtensionTest, VariantExtensionCopy) {
    // 测试 variant 扩展类型的拷贝
    auto    ext = mc::make_shared<test_extension>(600);
    variant v1(std::move(ext));
    variant v2(v1);

    EXPECT_TRUE(v1.is_extension());
    EXPECT_TRUE(v2.is_extension());
    EXPECT_EQ(v1, v2);

    auto ext1 = mc::dynamic_pointer_cast<test_extension>(v1.as_extension());
    auto ext2 = mc::dynamic_pointer_cast<test_extension>(v2.as_extension());
    EXPECT_TRUE(ext1 != nullptr);
    EXPECT_TRUE(ext2 != nullptr);
    EXPECT_EQ(ext1->get_value(), ext2->get_value());
}

TEST_F(VariantExtensionTest, VariantExtensionMove) {
    // 测试 variant 扩展类型的移动
    auto    ext = std::make_unique<test_extension>(700);
    variant v1(std::move(ext));
    variant v2(std::move(v1));

    EXPECT_FALSE(v1.is_extension());
    EXPECT_TRUE(v2.is_extension());

    auto ext2 = mc::dynamic_pointer_cast<test_extension>(v2.as_extension());
    EXPECT_NE(ext2, nullptr);
    EXPECT_EQ(ext2->get_value(), 700);
}

TEST_F(VariantExtensionTest, ExtensionTypeConversionMethods) {
    // 测试扩展类型的转换方法
    auto    ext = mc::make_shared<test_extension>(800);
    variant v(std::move(ext));

    // 测试各种转换方法
    EXPECT_EQ(v.as_int64(), 800);
    EXPECT_EQ(v.as_uint64(), 800);
    EXPECT_EQ(v.as_double(), 800.0);
    EXPECT_TRUE(v.as_bool()); // 非零值转换为 true
    EXPECT_EQ(v.as_string(), "test_extension(800)");

    auto blob = v.as_blob();
    EXPECT_EQ(blob.as_string_view(), "test_extension(800)");
}

TEST_F(VariantExtensionTest, ComplexExtensionType) {
    // 测试复杂扩展类型
    auto    ext = mc::make_shared<complex_extension>("complex_test", 15);
    variant v(std::move(ext));

    EXPECT_TRUE(v.is_extension());

    auto result = mc::dynamic_pointer_cast<complex_extension>(v.as_extension());
    EXPECT_TRUE(result != nullptr);
    EXPECT_EQ(result->get_name(), "complex_test");
    EXPECT_EQ(result->get_count(), 15);
}

TEST_F(VariantExtensionTest, ExtensionTypeHash) {
    // 测试扩展类型的哈希值
    auto ext1 = mc::make_shared<test_extension>(1000);
    auto ext2 = mc::make_shared<test_extension>(1000);
    auto ext3 = mc::make_shared<test_extension>(1001);

    EXPECT_EQ(ext1->hash(), ext2->hash());
    EXPECT_NE(ext1->hash(), ext3->hash());
}

TEST_F(VariantExtensionTest, ExtensionTypeInArray) {
    // 测试扩展类型在数组中的使用
    variants array;

    auto ext1 = mc::make_shared<test_extension>(1100);
    auto ext2 = mc::make_shared<complex_extension>("array_test", 20);

    array.emplace_back(std::move(ext1));
    array.emplace_back(std::move(ext2));
    array.emplace_back(static_cast<int64_t>(42)); // 普通整数

    EXPECT_EQ(array.size(), 3);
    EXPECT_TRUE(array[0].is_extension());
    EXPECT_TRUE(array[1].is_extension());
    EXPECT_TRUE(array[2].is_int64());

    auto test_ext    = mc::dynamic_pointer_cast<test_extension>(array[0].as_extension());
    auto complex_ext = mc::dynamic_pointer_cast<complex_extension>(array[1].as_extension());

    EXPECT_TRUE(test_ext != nullptr);
    EXPECT_TRUE(complex_ext != nullptr);
    EXPECT_EQ(test_ext->get_value(), 1100);
    EXPECT_EQ(complex_ext->get_name(), "array_test");
    EXPECT_EQ(complex_ext->get_count(), 20);
    EXPECT_EQ(array[2].as_int64(), 42);
}

TEST_F(VariantExtensionTest, ExtensionTypeInObject) {
    // 测试扩展类型在对象中的使用
    dict obj;

    auto ext1 = mc::make_shared<test_extension>(1200);
    auto ext2 = mc::make_shared<complex_extension>("object_test", 25);

    obj["test_ext"]     = variant(std::move(ext1));
    obj["complex_ext"]  = variant(std::move(ext2));
    obj["normal_value"] = variant("string_value");

    EXPECT_EQ(obj.size(), 3);
    EXPECT_TRUE(obj["test_ext"].is_extension());
    EXPECT_TRUE(obj["complex_ext"].is_extension());
    EXPECT_TRUE(obj["normal_value"].is_string());

    auto test_ext    = mc::dynamic_pointer_cast<test_extension>(obj["test_ext"].as_extension());
    auto complex_ext = mc::dynamic_pointer_cast<complex_extension>(obj["complex_ext"].as_extension());

    EXPECT_TRUE(test_ext != nullptr);
    EXPECT_TRUE(complex_ext != nullptr);
    EXPECT_EQ(test_ext->get_value(), 1200);
    EXPECT_EQ(complex_ext->get_name(), "object_test");
    EXPECT_EQ(complex_ext->get_count(), 25);
    EXPECT_EQ(obj["normal_value"].as_string(), "string_value");
}

TEST_F(VariantExtensionTest, ExtensionSerialization) {
    // 测试扩展类型的序列化和反序列化
    auto    ext = mc::make_shared<complex_extension>("serialization_test", 42);
    variant v(std::move(ext));

    // 转换为字符串
    std::string serialized = v.to_string();
    EXPECT_FALSE(serialized.empty());
    EXPECT_TRUE(serialized.find("serialization_test") != std::string::npos);
    EXPECT_TRUE(serialized.find("42") != std::string::npos);

    // 验证扩展类型的类型信息
    EXPECT_EQ(v.get_type_name(), mc::pretty_name<complex_extension>());
    auto result = v.as_extension();
    EXPECT_EQ(result->get_type_name(), mc::pretty_name<complex_extension>());

    // 验证扩展类型的内容
    auto complex_ext = mc::dynamic_pointer_cast<complex_extension>(v.as_extension());
    EXPECT_TRUE(complex_ext != nullptr);
    EXPECT_EQ(complex_ext->get_name(), "serialization_test");
    EXPECT_EQ(complex_ext->get_count(), 42);
}

TEST_F(VariantExtensionTest, FromVariant) {
    complex_extension ext("serialization_test", 42);
    variant           v(ext);

    complex_extension ext2 = v.as<complex_extension>();

    EXPECT_EQ(ext2.get_name(), "serialization_test");
    EXPECT_EQ(ext2.get_count(), 42);
}

// ========== 零开销引用访问测试 ==========

TEST_F(VariantExtensionTest, ArrayExtensionWithZeroCopyAccess) {
    // 创建支持零开销访问的数组扩展
    auto ext = mc::make_shared<variant_array_extension>(
        variants{10, 20, 30, 40, 50});
    variant v(ext);

    // 验证是 extension 类型
    EXPECT_TRUE(v.is_extension());
    EXPECT_TRUE(ext->supports_reference_access());

    // ✅ 测试索引读取（零开销）
    EXPECT_EQ(v[0], 10);
    EXPECT_EQ(v[1], 20);
    EXPECT_EQ(v[4], 50);

    // ✅ 测试类型转换
    EXPECT_EQ(v[0].as_int32(), 10);
    EXPECT_EQ(v[2].as<int>(), 30);

    // ✅ 测试索引写入（零开销）
    v[0] = 100;
    v[1] = 200;
    EXPECT_EQ(v[0], 100);
    EXPECT_EQ(v[1], 200);

    // 验证内部数据确实被修改
    EXPECT_EQ(ext->get_data()[0], 100);
    EXPECT_EQ(ext->get_data()[1], 200);
}

TEST_F(VariantExtensionTest, ObjectExtensionWithZeroCopyAccess) {
    // 创建支持零开销访问的对象扩展
    auto ext = mc::make_shared<variant_object_extension>(
        dict{{"name", "Alice"}, {"age", 30}, {"score", 95.5}});
    variant v(ext);

    // 验证是 extension 类型
    EXPECT_TRUE(v.is_extension());
    EXPECT_TRUE(ext->supports_reference_access());

    // ✅ 测试键读取（零开销）
    EXPECT_EQ(v["name"], "Alice");
    EXPECT_EQ(v["age"], 30);
    EXPECT_DOUBLE_EQ(v["score"].as_double(), 95.5);

    // ✅ 测试类型转换
    EXPECT_EQ(v["name"].as_string(), "Alice");
    EXPECT_EQ(v["age"].as<int>(), 30);

    // ✅ 测试键写入（零开销）
    v["name"] = "Bob";
    v["age"]  = 35;
    EXPECT_EQ(v["name"], "Bob");
    EXPECT_EQ(v["age"], 35);

    // 验证内部数据确实被修改
    EXPECT_EQ(ext->get_data()["name"], "Bob");
    EXPECT_EQ(ext->get_data()["age"], 35);
}

TEST_F(VariantExtensionTest, ExtensionOperatorChainedAccess) {
    // 测试嵌套的 extension + 链式访问
    auto inner_ext = mc::make_shared<variant_array_extension>(
        variants{100, 200, 300});

    auto outer_ext = mc::make_shared<variant_object_extension>(
        dict{
            {"data", variant(inner_ext)},
            {"count", 3},
        });

    variant v(outer_ext);

    // ✅ 链式访问（零开销）
    EXPECT_EQ(v["data"][0], 100);
    EXPECT_EQ(v["data"][1], 200);
    EXPECT_EQ(v["data"][2], 300);
    EXPECT_EQ(v["count"], 3);

    // ✅ 链式修改（零开销）
    v["data"][1] = 250;
    EXPECT_EQ(v["data"][1], 250);
}

TEST_F(VariantExtensionTest, ExtensionOperatorArithmetic) {
    // 测试扩展对象的算术操作
    auto ext = mc::make_shared<variant_array_extension>(
        variants{10, 20, 30});
    variant v(ext);

    // ✅ 两个 reference 之间的算术操作（零开销）
    EXPECT_EQ(v[0] + v[1], 30);
    EXPECT_EQ(v[2] - v[0], 20);
    EXPECT_EQ(v[1] * v[0], 200);

    // ✅ reference 与基础类型
    EXPECT_EQ(v[0] + 5, 15);
    EXPECT_EQ(5 + v[0], 15);

    // ✅ reference 与 variant
    variant num(5);
    EXPECT_EQ(v[0] + num, 15);
    EXPECT_EQ(num + v[0], 15);
}

TEST_F(VariantExtensionTest, ExtensionOperatorComparison) {
    // 测试扩展对象的比较操作
    auto ext = mc::make_shared<variant_array_extension>(
        variants{10, 20, 30, 20, 10});
    variant v(ext);

    // ✅ 两个 reference 之间的比较（零开销）
    EXPECT_TRUE(v[0] == v[4]);
    EXPECT_TRUE(v[1] == v[3]);
    EXPECT_TRUE(v[0] != v[1]);
    EXPECT_TRUE(v[0] < v[1]);
    EXPECT_TRUE(v[2] > v[1]);

    // ✅ reference 与基础类型
    EXPECT_TRUE(v[0] == 10);
    EXPECT_TRUE(10 == v[0]);
    EXPECT_TRUE(v[1] > 15);
    EXPECT_TRUE(15 < v[1]);

    // ✅ reference 与 variant
    variant num(20);
    EXPECT_TRUE(v[1] == num);
    EXPECT_TRUE(num == v[1]);
}

TEST_F(VariantExtensionTest, ExtensionOperatorCompoundAssignment) {
    // 测试扩展对象的复合赋值操作
    auto ext = mc::make_shared<variant_array_extension>(
        variants{10, 20, 30});
    variant v(ext);

    // ✅ 复合赋值操作（零开销）
    v[0] += 5;
    EXPECT_EQ(v[0], 15);

    v[1] *= 2;
    EXPECT_EQ(v[1], 40);

    v[2] -= 10;
    EXPECT_EQ(v[2], 20);

    // 验证内部数据被修改
    EXPECT_EQ(ext->get_data()[0], 15);
    EXPECT_EQ(ext->get_data()[1], 40);
    EXPECT_EQ(ext->get_data()[2], 20);
}

TEST_F(VariantExtensionTest, ExtensionMixedWithNormalVariant) {
    // 测试扩展对象与普通 variant 的混合使用
    auto ext = mc::make_shared<variant_array_extension>(
        variants{
            10,
            variants{100, 200},       // 嵌套普通数组
            dict{{"x", 1}, {"y", 2}}, // 嵌套 dict
        });
    variant v(ext);

    // ✅ 访问扩展数组的元素
    EXPECT_EQ(v[0], 10);

    // ✅ 链式访问嵌套的普通数组
    EXPECT_EQ(v[1][0], 100);
    EXPECT_EQ(v[1][1], 200);

    // ✅ 链式访问嵌套的 dict
    EXPECT_EQ(v[2]["x"], 1);
    EXPECT_EQ(v[2]["y"], 2);

    // ✅ 修改嵌套元素
    v[1][0]   = 150;
    v[2]["x"] = 10;
    EXPECT_EQ(v[1][0], 150);
    EXPECT_EQ(v[2]["x"], 10);
}

TEST_F(VariantExtensionTest, ExtensionNestedExtensions) {
    // 测试嵌套的 extension（extension 中包含 extension）
    auto inner_ext1 = mc::make_shared<variant_array_extension>(
        variants{1, 2, 3});
    auto inner_ext2 = mc::make_shared<variant_array_extension>(
        variants{10, 20, 30});

    auto outer_ext = mc::make_shared<variant_object_extension>(
        dict{
            {"first", variant(inner_ext1)},
            {"second", variant(inner_ext2)},
            {"scalar", 999},
        });

    variant v(outer_ext);

    // ✅ 深层链式访问（全部零开销）
    EXPECT_EQ(v["first"][0], 1);
    EXPECT_EQ(v["first"][2], 3);
    EXPECT_EQ(v["second"][0], 10);
    EXPECT_EQ(v["second"][2], 30);
    EXPECT_EQ(v["scalar"], 999);

    // ✅ 深层链式修改
    v["first"][1]  = 20;
    v["second"][1] = 200;
    EXPECT_EQ(v["first"][1], 20);
    EXPECT_EQ(v["second"][1], 200);

    // ✅ 深层链式算术操作
    EXPECT_EQ(v["first"][0] + v["second"][0], 11); // 1 + 10
    EXPECT_EQ(v["first"][2] * v["second"][0], 30); // 3 * 10
}

TEST_F(VariantExtensionTest, ExtensionZeroCopyVerification) {
    // 验证确实是零开销（通过指针地址验证）
    auto ext = mc::make_shared<variant_array_extension>(
        variants{10, 20, 30});
    variant v(ext);

    // 获取内部数据的地址
    const variants& internal_data = ext->get_data();
    const void*     internal_ptr0 = &internal_data[0];
    const void*     internal_ptr1 = &internal_data[1];

    // 通过 operator[] 获取 reference，再转换为指针
    const void* ref_ptr0 = &(*v[0]);
    const void* ref_ptr1 = &(*v[1]);

    // ✅ 验证地址相同，证明是零开销引用
    EXPECT_EQ(internal_ptr0, ref_ptr0) << "v[0] 应该直接引用内部数据";
    EXPECT_EQ(internal_ptr1, ref_ptr1) << "v[1] 应该直接引用内部数据";
}

TEST_F(VariantExtensionTest, ExtensionOperatorComplexExpression) {
    // 测试复杂表达式
    auto ext = mc::make_shared<variant_array_extension>(
        variants{2, 3, 4, 5, 6});
    variant v(ext);

    // ✅ 复杂算术表达式（零开销）
    auto result1 = v[0] + v[1] * v[2]; // 2 + 3 * 4 = 14
    EXPECT_EQ(result1, 14);

    auto result2 = (v[0] + v[1]) * v[2]; // (2 + 3) * 4 = 20
    EXPECT_EQ(result2, 20);

    auto result3 = v[4] - v[3] + v[2] * v[1]; // 6 - 5 + 4 * 3 = 13
    EXPECT_EQ(result3, 13);

    // ✅ 复杂比较表达式
    EXPECT_TRUE(v[0] < v[1] && v[1] < v[2]);
    EXPECT_TRUE((v[0] + v[1]) == 5); // 2 + 3 = 5
    EXPECT_TRUE((v[4] - v[3]) == 1); // 6 - 5 = 1
}

TEST_F(VariantExtensionTest, ExtensionStringConcatenation) {
    // 测试字符串扩展的拼接操作
    auto ext = mc::make_shared<variant_array_extension>(
        variants{"Hello", " ", "World", "!"});
    variant v(ext);

    // ✅ 字符串拼接（零开销）
    EXPECT_EQ((v[0] + v[1]).as_string(), "Hello ");
    EXPECT_EQ((v[0] + v[1] + v[2]).as_string(), "Hello World");
    EXPECT_EQ((v[0] + v[1] + v[2] + v[3]).as_string(), "Hello World!");

    // ✅ 与字符串字面量拼接
    EXPECT_EQ((v[0] + " World").as_string(), "Hello World");
    EXPECT_EQ(("Prefix: " + v[0]).as_string(), "Prefix: Hello");
}

TEST_F(VariantExtensionTest, ExtensionModificationPersistence) {
    // 验证通过 operator[] 修改的数据持久化到 extension 内部
    auto ext = mc::make_shared<variant_object_extension>(
        dict{{"x", 10}, {"y", 20}, {"z", 30}});
    variant v(ext);

    // 修改前的值
    EXPECT_EQ(ext->get_data()["x"], 10);
    EXPECT_EQ(ext->get_data()["y"], 20);

    // ✅ 通过 operator[] 修改
    v["x"] = 100;
    v["y"] = 200;
    v["z"] = 300;

    // 验证修改持久化
    EXPECT_EQ(ext->get_data()["x"], 100);
    EXPECT_EQ(ext->get_data()["y"], 200);
    EXPECT_EQ(ext->get_data()["z"], 300);

    // 验证可以继续访问
    EXPECT_EQ(v["x"], 100);
    EXPECT_EQ(v["y"], 200);
}

TEST_F(VariantExtensionTest, ExtensionMixedOperations) {
    // 测试扩展对象的混合操作场景
    auto arr_ext = mc::make_shared<variant_array_extension>(
        variants{5, 10, 15});
    auto obj_ext = mc::make_shared<variant_object_extension>(
        dict{{"multiplier", 2}, {"offset", 100}});

    variant arr_var(arr_ext);
    variant obj_var(obj_ext);

    // ✅ 跨 extension 的操作
    EXPECT_EQ(arr_var[0] + obj_var["offset"], 105);    // 5 + 100
    EXPECT_EQ(arr_var[1] * obj_var["multiplier"], 20); // 10 * 2

    // ✅ 复合赋值
    arr_var[0] += obj_var["offset"];
    EXPECT_EQ(arr_var[0], 105);

    obj_var["offset"] += arr_var[1];
    EXPECT_EQ(obj_var["offset"], 110); // 100 + 10
}

TEST_F(VariantExtensionTest, ExtensionOperatorAllTypes) {
    // 测试扩展对象支持所有操作符类型
    auto ext = mc::make_shared<variant_array_extension>(
        variants{12, 10, 5, true, "text"});
    variant v(ext);

    // ✅ 位操作
    EXPECT_EQ(v[0] & v[1], 8);  // 1100 & 1010 = 1000
    EXPECT_EQ(v[0] | v[2], 13); // 1100 | 0101 = 1101
    EXPECT_EQ(v[1] << 2, 40);   // 10 << 2 = 40

    // ✅ 自增自减
    ++v[0];
    EXPECT_EQ(v[0], 13);

    v[1]--;
    EXPECT_EQ(v[1], 9);

    // ✅ 一元操作符
    EXPECT_EQ(-v[2], -5);
    EXPECT_EQ(!v[3], false);

    // ✅ 字符串操作
    EXPECT_EQ(v[4].as_string(), "text");
    EXPECT_TRUE(v[4] == "text");
    EXPECT_TRUE("text" == v[4]);
}

// ========== mc::array extension 测试 ==========

TEST_F(VariantExtensionTest, ArrayIntExtension) {
    // 测试 mc::array<int> 的 extension 支持
    mc::array<int> arr = {1, 2, 3, 4, 5};
    mc::variant    v   = arr; // 隐式转换

    // 验证是 extension 类型
    EXPECT_TRUE(v.is_extension());

    // ✅ 测试索引读取（带类型转换）
    EXPECT_EQ(v[0].as_int32(), 1);
    EXPECT_EQ(v[1].as_int32(), 2);
    EXPECT_EQ(v[4].as_int32(), 5);

    // ✅ 测试索引写入（带类型转换）
    v[0] = 100;
    v[1] = 200;
    EXPECT_EQ(v[0].as_int32(), 100);
    EXPECT_EQ(v[1].as_int32(), 200);

    // ✅ 测试取回原始类型
    mc::array<int> arr2 = v.as<mc::array<int>>();
    EXPECT_EQ(arr2.size(), 5);
    EXPECT_EQ(arr2[0], 100);
    EXPECT_EQ(arr2[1], 200);
    EXPECT_EQ(arr2[2], 3);
}

TEST_F(VariantExtensionTest, ArrayStringExtension) {
    // 测试 mc::array<std::string> 的 extension 支持
    mc::array<std::string> arr = {"hello", "world", "test"};
    mc::variant            v   = arr;

    EXPECT_TRUE(v.is_extension());

    // 读取字符串
    EXPECT_EQ(v[0].as_string(), "hello");
    EXPECT_EQ(v[1].as_string(), "world");

    // 修改字符串
    v[0] = "HELLO";
    EXPECT_EQ(v[0].as_string(), "HELLO");

    // 取回原始类型
    mc::array<std::string> arr2 = v.as<mc::array<std::string>>();
    EXPECT_EQ(arr2[0], "HELLO");
    EXPECT_EQ(arr2[1], "world");
}

TEST_F(VariantExtensionTest, ArrayExtensionSharedSemantics) {
    // 验证共享语义
    mc::array<int> arr1 = {10, 20, 30};
    mc::variant    v1   = arr1;

    // 拷贝 variant
    mc::variant v2 = v1;

    // 修改 v1
    v1[0] = 100;

    // v2 应该受影响（因为共享内部数据）
    EXPECT_EQ(v2[0].as_int32(), 100);

    // 取回的 array 也共享数据
    mc::array<int> arr2 = v1.as<mc::array<int>>();
    EXPECT_EQ(arr2[0], 100);

    // 修改 arr2
    arr2[1] = 200;

    // v1 应该受影响
    EXPECT_EQ(v1[1].as_int32(), 200);
}

TEST_F(VariantExtensionTest, ArrayExtensionInDict) {
    // 测试 array extension 在 dict 中的使用
    mc::array<int>         arr1 = {1, 2, 3};
    mc::array<std::string> arr2 = {"a", "b", "c"};

    mc::dict obj;
    obj["numbers"] = arr1;
    obj["strings"] = arr2;

    EXPECT_TRUE(obj["numbers"].is_extension());
    EXPECT_TRUE(obj["strings"].is_extension());

    // 访问嵌套元素
    EXPECT_EQ(obj["numbers"][0].as_int32(), 1);
    EXPECT_EQ(obj["strings"][1].as_string(), "b");

    // 修改嵌套元素
    obj["numbers"][0] = 100;
    EXPECT_EQ(obj["numbers"][0].as_int32(), 100);
}

TEST_F(VariantExtensionTest, ArrayExtensionMixedWithVariants) {
    // 测试 extension 与普通 variants 混合
    mc::array<int> int_arr = {1, 2, 3};
    mc::variants   var_arr = {10, 20, 30};

    mc::variant  v1 = int_arr;       // extension
    mc::variant  v2 = var_arr;       // 普通 array
    mc::variants v3 = v1.as_array(); // extension 数组可以转换成 mc::variants

    EXPECT_TRUE(v1.is_extension());
    EXPECT_TRUE(v2.is_array());
    EXPECT_FALSE(v2.is_extension());

    // 两种方式都可以访问
    EXPECT_EQ(v1[0].as_int32(), 1);
    EXPECT_EQ(v2[0].as_int32(), 10);
    EXPECT_EQ(v3[0].as_int32(), int_arr[0]);

    // 放入同一个 dict
    mc::dict obj;
    obj["ext_arr"] = v1;
    obj["var_arr"] = v2;

    EXPECT_EQ(obj["ext_arr"][0].as_int32(), 1);
    EXPECT_EQ(obj["var_arr"][0].as_int32(), 10);
}

TEST_F(VariantExtensionTest, ArrayExtensionVisitInt) {
    // 测试 mc::array<int> 通过 extension.visit() 遍历
    mc::array<int> arr = {10, 20, 30, 40, 50};
    mc::variant    v   = arr;

    EXPECT_TRUE(v.is_extension());
    std::vector<int> collected;
    v.visit_with([&collected](const mc::variant_extension_base& ext) {
        ext.visit([&collected](const mc::variant& item) {
            collected.push_back(item.as_int32());
        });
    });

    // 验证收集到的元素
    ASSERT_EQ(collected.size(), 5);
    EXPECT_EQ(collected[0], 10);
    EXPECT_EQ(collected[1], 20);
    EXPECT_EQ(collected[2], 30);
    EXPECT_EQ(collected[3], 40);
    EXPECT_EQ(collected[4], 50);

    // 使用 visit_with + visit 计算总和
    int sum = 0;
    v.visit_with([&sum](const mc::variant_extension_base& ext) {
        ext.visit([&sum](const mc::variant& item) {
            sum += item.as_int32();
        });
    });
    EXPECT_EQ(sum, 150); // 10 + 20 + 30 + 40 + 50 = 150
}

TEST_F(VariantExtensionTest, ArrayExtensionVisitString) {
    // 测试 mc::array<std::string> 通过 extension.visit() 遍历
    mc::array<std::string> arr = {"Hello", "World", "Test", "Visit"};
    mc::variant            v   = arr;

    EXPECT_TRUE(v.is_extension());

    std::string concatenated;
    v.visit_with([&concatenated](const mc::variant_extension_base& ext) {
        ext.visit([&concatenated](const mc::variant& item) {
            if (!concatenated.empty()) {
                concatenated += " ";
            }
            concatenated += item.as_string();
        });
    });

    EXPECT_EQ(concatenated, "Hello World Test Visit");

    // 使用 visit_with + visit 统计字符串总长度
    size_t total_length = 0;
    v.visit_with([&total_length](const mc::variant_extension_base& ext) {
        ext.visit([&total_length](const mc::variant& item) {
            total_length += item.as_string().length();
        });
    });
    EXPECT_EQ(total_length, 19); // 5 + 5 + 4 + 5 = 19
}

TEST_F(VariantExtensionTest, VariantsVisit) {
    // 测试 mc::variants (mc::array<variant>) 的遍历
    mc::variants arr = {1, "hello", 3.14, true, mc::variants{10, 20}};
    mc::variant  v   = arr;

    EXPECT_TRUE(v.is_array());
    EXPECT_FALSE(v.is_extension()); // mc::variants 是内置类型，不是 extension

    std::vector<std::string> type_names;
    v.visit_with([&type_names](const mc::variants& array) {
        for (const auto& item : array) {
            type_names.push_back(item.get_type_name());
        }
    });

    ASSERT_EQ(type_names.size(), 5);
    EXPECT_EQ(type_names[0], arr[0].get_type_name());
    EXPECT_EQ(type_names[1], arr[1].get_type_name());
    EXPECT_EQ(type_names[2], arr[2].get_type_name());
    EXPECT_EQ(type_names[3], arr[3].get_type_name());
    EXPECT_EQ(type_names[4], arr[4].get_type_name());

    // 验证可以访问嵌套数组
    int nested_count = 0;
    v.visit_with([&nested_count](const mc::variants& array) {
        for (const auto& item : array) {
            item.visit_with([&nested_count](const mc::variants& nested_array) {
                nested_count += nested_array.size();
            });
        }
    });
    EXPECT_EQ(nested_count, 2); // 嵌套数组有 2 个元素
}

TEST_F(VariantExtensionTest, ArrayExtensionVisitDouble) {
    // 测试 mc::array<double> 通过 extension.visit() 遍历
    mc::array<double> arr = {1.5, 2.5, 3.5, 4.5, 5.5};
    mc::variant       v   = arr;

    EXPECT_TRUE(v.is_extension());

    // 使用 visit_with + visit 计算平均值
    double sum   = 0.0;
    int    count = 0;
    v.visit_with([&sum, &count](const mc::variant_extension_base& ext) {
        ext.visit([&sum, &count](const mc::variant& item) {
            sum += item.as_double();
            count++;
        });
    });

    EXPECT_EQ(count, 5);
    EXPECT_DOUBLE_EQ(sum / count, 3.5); // (1.5 + 2.5 + 3.5 + 4.5 + 5.5) / 5 = 3.5

    // 使用 visit_with + visit 查找最大值
    double max_val = std::numeric_limits<double>::lowest();
    v.visit_with([&max_val](const mc::variant_extension_base& ext) {
        ext.visit([&max_val](const mc::variant& item) {
            double val = item.as_double();
            if (val > max_val) {
                max_val = val;
            }
        });
    });
    EXPECT_DOUBLE_EQ(max_val, 5.5);
}

TEST_F(VariantExtensionTest, ArrayExtensionVisitModification) {
    // 测试通过 extension.visit() 遍历并修改元素
    mc::array<int> arr = {1, 2, 3, 4, 5};
    mc::variant    v   = arr;

    // 先验证原始值
    std::vector<int> original;
    v.visit_with([&original](const mc::variant_extension_base& ext) {
        ext.visit([&original](const mc::variant& item) {
            original.push_back(item.as_int32());
        });
    });
    EXPECT_EQ(original, std::vector<int>({1, 2, 3, 4, 5}));

    // 通过索引修改（不在 visit 中）
    for (size_t i = 0; i < arr.size(); i++) {
        v[i] = arr[i] * 2;
    }

    std::vector<int> modified;
    v.visit_with([&modified](const mc::variant_extension_base& ext) {
        ext.visit([&modified](const mc::variant& item) {
            modified.push_back(item.as_int32());
        });
    });
    EXPECT_EQ(modified, std::vector<int>({2, 4, 6, 8, 10}));
}

TEST_F(VariantExtensionTest, VariantsVisitNested) {
    // 测试嵌套 variants 的递归遍历
    mc::variants arr = {
        1,
        mc::variants{10, 20, 30},
        "test",
        mc::dict{{"x", 100}, {"y", 200}},
    };
    mc::variant v = arr;

    int                                     total_count     = 0;
    std::function<void(const mc::variant&)> recursive_visit = [&](const mc::variant& item) {
        total_count++;
        if (item.is_array()) {
            item.visit_with([&](const mc::variants& nested_array) {
                for (const auto& nested_item : nested_array) {
                    recursive_visit(nested_item);
                }
            });
        }
    };

    v.visit_with([&](const mc::variants& array) {
        for (const auto& item : array) {
            recursive_visit(item);
        }
    });

    // 顶层 4 个元素 + 嵌套数组 3 个元素 = 7
    EXPECT_EQ(total_count, 7);
}

// 测试非数组的 extension 不应该被转换为数组
TEST_F(VariantExtensionTest, NonArrayExtensionCannotConvertToArray) {
    // 创建一个非数组的 extension
    auto ext = mc::make_shared<test_extension>(42);

    // 检查类型信息
    auto type_info = ext->get_type_info();
    EXPECT_NE(type_info.id, mc::extension_type_ids::typed_array) << "test_extension should not be typed_array";

    mc::variant v(mc::static_pointer_cast<mc::variant_extension_base>(ext));

    EXPECT_TRUE(v.is_extension());
    EXPECT_FALSE(v.is_array());

    // 尝试转换为 mc::variants 应该失败（抛出异常）
    EXPECT_THROW({ mc::variants arr = v.as_array(); }, mc::bad_cast_exception);

    // 同样，complex_extension 也不应该被转换为数组
    auto complex_ext       = mc::make_shared<complex_extension>("test", 100);
    auto complex_type_info = complex_ext->get_type_info();
    EXPECT_NE(complex_type_info.id, mc::extension_type_ids::typed_array) << "complex_extension should not be typed_array";

    mc::variant v2(mc::static_pointer_cast<mc::variant_extension_base>(complex_ext));

    EXPECT_TRUE(v2.is_extension());
    EXPECT_FALSE(v2.is_array());

    EXPECT_THROW({ mc::variants arr = v2.as_array(); }, mc::bad_cast_exception);
}

// 测试内置数组转换到强类型数组
TEST_F(VariantExtensionTest, BuiltinArrayToTypedArray) {
    // mc::variants -> mc::array<int>
    mc::variants var_arr = {1, 2, 3, 4, 5};
    mc::variant  v       = var_arr;

    mc::array<int> int_arr;
    EXPECT_NO_THROW({ from_variant(v, int_arr); });
    EXPECT_EQ(int_arr.size(), 5);
    EXPECT_EQ(int_arr[0], 1);
    EXPECT_EQ(int_arr[4], 5);

    // mc::variants -> mc::array<std::string>
    mc::variants str_var_arr = {"hello", "world", "test"};
    mc::variant  v2          = str_var_arr;

    mc::array<std::string> str_arr;
    EXPECT_NO_THROW({ from_variant(v2, str_arr); });
    EXPECT_EQ(str_arr.size(), 3);
    EXPECT_EQ(str_arr[0], "hello");
    EXPECT_EQ(str_arr[2], "test");

    // mc::variants -> mc::array<double>
    mc::variants double_var_arr = {1.1, 2.2, 3.3};
    mc::variant  v3             = double_var_arr;

    mc::array<double> double_arr;
    EXPECT_NO_THROW({ from_variant(v3, double_arr); });
    EXPECT_EQ(double_arr.size(), 3);
    EXPECT_DOUBLE_EQ(double_arr[0], 1.1);
    EXPECT_DOUBLE_EQ(double_arr[2], 3.3);
}

// 测试空数组的 extension 可以正确转换
TEST_F(VariantExtensionTest, EmptyArrayExtensionConversion) {
    // 创建空的 mc::array<int>
    mc::array<int> empty_int_arr;
    mc::variant    v1 = empty_int_arr;

    EXPECT_TRUE(v1.is_extension());
    EXPECT_EQ(v1.size(), 0);

    // 应该能够转换为 mc::variants（空数组）
    mc::variants result;
    EXPECT_NO_THROW({
        result = v1.as_array();
    });
    EXPECT_EQ(result.size(), 0);

    // 创建空的 mc::array<std::string>
    mc::array<std::string> empty_string_arr;
    mc::variant            v2 = empty_string_arr;

    EXPECT_TRUE(v2.is_extension());
    EXPECT_EQ(v2.size(), 0);

    // 应该能够转换为 mc::variants（空数组）
    mc::variants result2;
    EXPECT_NO_THROW({
        result2 = v2.as_array();
    });
    EXPECT_EQ(result2.size(), 0);
}

} // namespace test
} // namespace mc