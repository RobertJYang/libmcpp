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

    mc::shared_ptr<variant_extension_base> clone() const override {
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
    mutable_dict obj;

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

} // namespace test
} // namespace mc