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
 * @file test_variant_edge_cases.cpp
 * @brief 测试 variant 模块的边缘情况、边界条件和异常处理
 */

#include "test_variant_helpers.h"
#include <gtest/gtest.h>
#include <limits>
#include <mc/exception.h>
#include <mc/variant.h>
#include <mc/variant/variant_common.h>
#include <mc/variant/variant_extension.h>
#include <test_utilities/test_base.h>

namespace mc {
namespace test {

// 不支持引用访问的扩展类型（用于测试值访问模式）
class test_extension_no_ref_access : public variant_extension<test_extension_no_ref_access> {
public:
    test_extension_no_ref_access() : m_value(0) {
    }
    explicit test_extension_no_ref_access(int value) : m_value(value) {
    }

    bool operator==(const test_extension_no_ref_access& other) const {
        return m_value == other.m_value;
    }

    std::size_t hash() const override {
        return std::hash<int>()(m_value);
    }

    // 不支持零开销引用访问
    bool supports_reference_access() const override {
        return false;
    }

    // 不支持 get_ptr，只能使用值访问
    mc::variant* get_ptr(std::string_view key) override {
        return nullptr;
    }

    const mc::variant* get_ptr(std::string_view key) const override {
        return nullptr;
    }

    mc::variant get(std::string_view key) const override {
        if (key == "value") {
            return mc::variant(m_value);
        }
        throw std::out_of_range("键不存在");
    }

    void set(std::string_view key, const mc::variant& value) override {
        if (key == "value") {
            m_value = value.as_int32();
        }
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
        return "test_extension_no_ref_access(" + std::to_string(m_value) + ")";
    }

    mc::shared_ptr<variant_extension_base> copy() const override {
        return mc::make_shared<test_extension_no_ref_access>(*this);
    }

private:
    int m_value;
};

class unsupported_extension : public variant_extension<unsupported_extension> {
public:
    unsupported_extension() = default;
    explicit unsupported_extension(int v) : m_value(v) {
    }

    bool operator==(const unsupported_extension& other) const {
        return m_value == other.m_value;
    }

    std::size_t hash() const override {
        return std::hash<int>()(m_value);
    }

    mc::shared_ptr<variant_extension_base> copy() const override {
        return mc::make_shared<unsupported_extension>(*this);
    }

    bool equals(const variant_extension_base& other) const override {
        auto* ptr = dynamic_cast<const unsupported_extension*>(&other);
        return ptr != nullptr && ptr->m_value == m_value;
    }

private:
    int m_value{0};
};

class VariantEdgeCasesTest : public mc::test::TestBase {
protected:
    void SetUp() override {
        TestBase::SetUp();
    }

    void TearDown() override {
        TestBase::TearDown();
    }
};

// 测试 size() 函数中的 blob_type 分支
TEST_F(VariantEdgeCasesTest, SizeBlobType) {
    std::string test_str = "test data";
    mc::blob blob_data(test_str.data(), test_str.size());
    variant v(blob_data);
    EXPECT_EQ(v.size(), blob_data.data.size());
}

// 注意：extension null 错误分支很难直接测试，因为 variant_base 的构造函数会检查 extension
// 这个分支可能只在内部实现中出现，或者通过特殊的移动构造/赋值操作触发

// 测试 operator[] 中 extension 不支持 reference access 的分支（值访问模式）
TEST_F(VariantEdgeCasesTest, OperatorIndexExtensionNoRefAccess) {
    auto ext = mc::make_shared<test_extension_no_ref_access>(42);
    variant v(ext);
    // 使用值访问模式（不支持引用访问时会调用 get() 方法）
    variant_reference ref = v["value"];
    EXPECT_EQ(ref.get().as_int32(), 42);
}

// 测试 operator[] 中抛出 type_error 的分支（非 object 和非 extension）
TEST_F(VariantEdgeCasesTest, OperatorIndexTypeError) {
    variant v(123);
    EXPECT_THROW({ v["key"]; }, mc::exception);
}

// 测试 const operator[] 中 extension 相关分支
TEST_F(VariantEdgeCasesTest, OperatorIndexConstExtension) {
    auto ext = mc::make_shared<test_extension_no_ref_access>(100);
    const variant v(ext);
    // const 版本的 operator[] 也应该支持 extension
    variant_reference ref = v["value"];
    EXPECT_EQ(ref.get().as_int32(), 100);
}

// 注意：const operator[] 中 extension null 错误分支也很难直接测试

// 测试 get() 函数（完全未覆盖）
TEST_F(VariantEdgeCasesTest, GetWithDefaultValue) {
    variant obj = mc::dict{{"key1", 123}, {"key2", "value"}};
    variant default_val(456);

    // 测试存在的键
    const variant& result1 = obj.get("key1", default_val);
    EXPECT_EQ(result1.as_int32(), 123);

    // 测试不存在的键
    const variant& result2 = obj.get("nonexistent", default_val);
    EXPECT_EQ(result2.as_int32(), 456);

    // 测试非 object 类型
    variant non_obj(789);
    const variant& result3 = non_obj.get("key", default_val);
    EXPECT_EQ(result3.as_int32(), 456); // 应该返回默认值
}

// 测试 contains() 中非 object 的分支
TEST_F(VariantEdgeCasesTest, ContainsNonObject) {
    variant v(123);
    EXPECT_FALSE(v.contains("key"));

    variant arr = mc::array{1, 2, 3};
    EXPECT_FALSE(arr.contains("key"));
}

// 测试 operator==(const dict&) 中非 object 的分支
TEST_F(VariantEdgeCasesTest, OperatorEqualDictNonObject) {
    variant v(123);
    mc::dict d{{"key", "value"}};
    EXPECT_FALSE(v == d);
}

// 测试 set_value() 中的 extension_type 分支
TEST_F(VariantEdgeCasesTest, SetValueExtensionType) {
    auto ext1 = mc::make_shared<test_extension_no_ref_access>(10);
    auto ext2 = mc::make_shared<test_extension_no_ref_access>(20);
    variant v1(ext1);
    variant v2(ext2);

    v1.set_value(v2);
    EXPECT_EQ(v1.as_int64(), 20);
}

// 注意：set_value() 中的 default 分支很难直接测试，因为 variant_base 的构造函数会验证 type_id
// 这个分支可能在内部实现中作为保护性代码存在

// 注意：visit() 中的 extension null 检查很难直接测试
// 因为 variant_base 的构造函数会检查 extension，很难创建 null extension

// 测试 operator+ 中的字符串+blob 分支
TEST_F(VariantEdgeCasesTest, OperatorPlusStringBlob) {
    variant str("hello");
    std::string blob_str = " world";
    mc::blob blob_data(blob_str.data(), blob_str.size());
    variant blob(blob_data);
    variant result = str + blob;
    EXPECT_TRUE(result.is_string());
    EXPECT_EQ(result.as_string(), "hello world");
}

// 测试 operator+ 中的 blob+string 分支
TEST_F(VariantEdgeCasesTest, OperatorPlusBlobString) {
    std::string blob_str = "hello";
    mc::blob blob_data(blob_str.data(), blob_str.size());
    variant blob(blob_data);
    variant str(" world");
    variant result = blob + str;
    // 注意：blob + string 实际上会走到 other.is_string() 分支，返回字符串类型
    // 因为 operator+ 首先检查 other.is_string()，如果为真则调用 as_string() + other.get_string()
    EXPECT_TRUE(result.is_string());
    EXPECT_EQ(result.as_string(), "hello world");
}

// 测试 operator+ 中的 blob 异常分支（blob 与非 blob/string 相加）
TEST_F(VariantEdgeCasesTest, OperatorPlusBlobInvalidType) {
    std::string blob_str = "test";
    mc::blob blob_data(blob_str.data(), blob_str.size());
    variant blob(blob_data);
    variant num(123);
    EXPECT_THROW({ blob + num; }, mc::exception);
}

// 测试 operator+ 中的对象拼接异常分支（对象与非对象相加）
TEST_F(VariantEdgeCasesTest, OperatorPlusObjectInvalidType) {
    variant obj = mc::dict{{"key1", 1}};
    variant num(123);
    EXPECT_THROW({ obj + num; }, mc::exception);
}

// 测试 operator- 中的异常处理分支
TEST_F(VariantEdgeCasesTest, OperatorMinusException) {
    variant v1 = mc::dict{{"key", "value"}};
    variant v2(123);
    EXPECT_THROW({ v1 - v2; }, mc::exception);
}

// 测试 operator* 中的异常处理分支
TEST_F(VariantEdgeCasesTest, OperatorMultiplyException) {
    variant v1 = mc::dict{{"key", "value"}};
    variant v2(123);
    EXPECT_THROW({ v1 * v2; }, mc::exception);
}

// 测试 operator/ 中的异常处理分支
TEST_F(VariantEdgeCasesTest, OperatorDivideException) {
    variant v1 = mc::dict{{"key", "value"}};
    variant v2(123);
    EXPECT_THROW({ v1 / v2; }, mc::exception);
}

// 测试 operator% 中的异常处理分支
TEST_F(VariantEdgeCasesTest, OperatorModuloException) {
    variant v1 = mc::dict{{"key", "value"}};
    variant v2(123);
    EXPECT_THROW({ v1 % v2; }, mc::exception);
}

// 测试 operator& 中的异常处理分支
TEST_F(VariantEdgeCasesTest, OperatorBitwiseAndException) {
    variant v1 = mc::dict{{"key", "value"}};
    variant v2(123);
    EXPECT_THROW({ v1 & v2; }, mc::exception);
}

// 测试 operator| 中的异常处理分支
TEST_F(VariantEdgeCasesTest, OperatorBitwiseOrException) {
    variant v1 = mc::dict{{"key", "value"}};
    variant v2(123);
    EXPECT_THROW({ v1 | v2; }, mc::exception);
}

// 测试 operator^ 中的异常处理分支
TEST_F(VariantEdgeCasesTest, OperatorBitwiseXorException) {
    variant v1 = mc::dict{{"key", "value"}};
    variant v2(123);
    EXPECT_THROW({ v1 ^ v2; }, mc::exception);
}

// 测试 variant_base(type_id type) 构造函数中的 extension_type 分支
// 注意：variant 没有直接接受 type_id 的公共构造函数，但 typed_variant 有
// 我们可以通过 typed_variant 来测试，或者直接测试 extension 的构造
TEST_F(VariantEdgeCasesTest, ConstructorWithExtensionType) {
    // 使用 typed_variant 来测试 extension_type 构造
    typed_variant v(mc::type_id::extension_type);
    EXPECT_TRUE(v.is_extension());
    // extension_type 构造出来的是空 extension（null）
    auto ext = v.as_extension();
    EXPECT_FALSE(ext); // extension 应该为 null
}

// 测试移动构造中的 blob_type 分支
TEST_F(VariantEdgeCasesTest, MoveConstructorBlobType) {
    std::string blob_str = "test blob data";
    mc::blob blob_data(blob_str.data(), blob_str.size());
    variant v1(blob_data);
    variant v2(std::move(v1));
    EXPECT_TRUE(v2.is_blob());
    EXPECT_EQ(v2.as_blob().as_string_view(), blob_str);
    // v1 应该已经变为 null
    EXPECT_TRUE(v1.is_null());
}

// 测试移动构造中的 extension_type 分支
TEST_F(VariantEdgeCasesTest, MoveConstructorExtensionType) {
    auto ext = mc::make_shared<test_extension_no_ref_access>(50);
    variant v1(ext);
    variant v2(std::move(v1));
    EXPECT_TRUE(v2.is_extension());
    EXPECT_EQ(v2.as_int64(), 50);
    // v1 应该已经变为 null
    EXPECT_TRUE(v1.is_null());
}

// 测试 set_value(variant_base&& other) 中的相同类型分支
TEST_F(VariantEdgeCasesTest, SetValueMoveSameType) {
    variant v1(100);
    variant v2(200);
    // 当类型相同时，应该直接交换内容（swap）
    v1.set_value(std::move(v2));
    EXPECT_EQ(v1.as_int32(), 200);
    // v2 应该已经变为 v1 原来的值（因为 swap）
    EXPECT_EQ(v2.as_int32(), 100);
}

// 测试位移运算符中的边界情况：shift_amount >= 64
TEST_F(VariantEdgeCasesTest, OperatorLeftShiftLargeAmount) {
    variant v1(1);
    variant v2(64); // 位移量 >= 64
    variant result = v1 << v2;
    EXPECT_EQ(result.as_int64(), 0);
}

// 测试位移运算符中的边界情况：右移大位移量
TEST_F(VariantEdgeCasesTest, OperatorRightShiftLargeAmount) {
    variant v1(100);
    variant v2(64); // 位移量 >= 64
    variant result = v1 >> v2;
    EXPECT_EQ(result.as_int64(), 0);
}

// 测试位移运算符中的边界情况：右移大位移量，负数情况
TEST_F(VariantEdgeCasesTest, OperatorRightShiftLargeAmountNegative) {
    variant v1(-1);
    variant v2(64); // 位移量 >= 64，且是负数
    variant result = v1 >> v2;
    EXPECT_EQ(result.as_int64(), -1); // 对负数，保持符号位
}

// 测试 visit() 中的各种类型分支
TEST_F(VariantEdgeCasesTest, VisitVariousTypes) {
    // 自定义访问者类
    class TestVisitor : public variant::visitor {
    public:
        mutable bool visited_int64 = false;
        mutable bool visited_string = false;
        mutable bool visited_blob = false;
        mutable int64_t int_value = 0;
        mutable std::string string_value;
        mutable std::string blob_value;

        void handle() const override {
        }

        void handle(const int64_t& v) const override {
            visited_int64 = true;
            int_value = v;
        }

        void handle(const uint64_t& v) const override {
            visited_int64 = true;
            int_value = static_cast<int64_t>(v);
        }

        void handle(const double& v) const override {
        }

        void handle(const bool& v) const override {
        }

        void handle(const std::string& v) const override {
            visited_string = true;
            string_value = v;
        }

        void handle(const dict& v) const override {
        }

        void handle(const variants& v) const override {
        }

        void handle(const blob& v) const override {
            visited_blob = true;
            blob_value = std::string(v.as_string_view());
        }

        void handle(const variant_extension_base& v) const override {
        }
    };

    // 测试整数类型
    variant v1(123);
    TestVisitor visitor1;
    v1.visit(visitor1);
    EXPECT_TRUE(visitor1.visited_int64);
    EXPECT_EQ(visitor1.int_value, 123);

    // 测试字符串类型
    variant v2("test");
    TestVisitor visitor2;
    v2.visit(visitor2);
    EXPECT_TRUE(visitor2.visited_string);
    EXPECT_EQ(visitor2.string_value, "test");

    // 测试 blob 类型
    std::string blob_str = "blob data";
    mc::blob blob_data(blob_str.data(), blob_str.size());
    variant v3(blob_data);
    TestVisitor visitor3;
    v3.visit(visitor3);
    EXPECT_TRUE(visitor3.visited_blob);
    EXPECT_EQ(visitor3.blob_value, "blob data");
}

// 测试 visit() 中的 extension null 检查（extension 为 null 时不调用 handle）
TEST_F(VariantEdgeCasesTest, VisitExtensionNull) {
    // 自定义访问者类
    class TestVisitor : public variant::visitor {
    public:
        mutable bool visited = false;

        void handle() const override {
            visited = true;
        }

        void handle(const int64_t&) const override {
            visited = true;
        }

        void handle(const uint64_t&) const override {
            visited = true;
        }

        void handle(const double&) const override {
            visited = true;
        }

        void handle(const bool&) const override {
            visited = true;
        }

        void handle(const std::string&) const override {
            visited = true;
        }

        void handle(const dict&) const override {
            visited = true;
        }

        void handle(const variants&) const override {
            visited = true;
        }

        void handle(const blob&) const override {
            visited = true;
        }

        void handle(const variant_extension_base&) const override {
            visited = true;
        }
    };

    // 创建一个 extension 类型的 variant，但 extension 为 null
    typed_variant v(mc::type_id::extension_type);
    TestVisitor visitor;
    v.visit(visitor);
    // extension 为 null 时，visit 应该不会调用任何 handle 方法
    EXPECT_FALSE(visitor.visited);
}

// 测试 as_uint64() 中 default 分支的 return 0（在抛出异常后）
// 注意：这个分支实际上不会被执行，因为 throw 会终止函数
// 但为了完整性，我们可以测试 as_uint64() 的 default 分支抛出异常
TEST_F(VariantEdgeCasesTest, AsUint64DefaultBranch) {
    variant v = mc::dict{{"key", "value"}};
    EXPECT_THROW({ v.as_uint64(); }, mc::exception);
}

// 测试 operator=(const char* s) 当 s 为 nullptr 时的各种类型分支
TEST_F(VariantEdgeCasesTest, OperatorAssignNullptrChar) {
    // 测试 null_type
    variant v1;
    v1 = nullptr;
    EXPECT_TRUE(v1.is_null());

    // 测试整数类型
    variant v2(42);
    v2 = nullptr;
    EXPECT_EQ(v2.as_int64(), 0);

    // 测试无符号整数类型
    variant v3(uint64_t(100));
    v3 = nullptr;
    EXPECT_EQ(v3.as_uint64(), 0);

    // 测试 double 类型
    variant v4(3.14);
    v4 = nullptr;
    EXPECT_EQ(v4.as_double(), 0.0);

    // 测试 bool 类型
    variant v5(true);
    v5 = nullptr;
    EXPECT_FALSE(v5.as_bool());

    // 测试 string 类型（固定类型，应该清空字符串）
    typed_variant v6(mc::type_id::string_type);
    v6 = "test";
    v6 = nullptr;
    EXPECT_TRUE(v6.get_string().empty());

    // 测试非固定类型，应该变为 null
    variant v7("test");
    v7 = nullptr;
    EXPECT_TRUE(v7.is_null());

    // 测试固定类型但不是 string（如 array_type），应该抛出异常
    typed_variant v8(mc::type_id::array_type);
    v8 = variants{1, 2, 3};
    EXPECT_THROW({ v8 = nullptr; }, mc::exception);
}

// 测试 operator=(std::string_view s) 当 variant 是 blob 类型时
TEST_F(VariantEdgeCasesTest, OperatorAssignStringViewToBlob) {
    typed_variant v(mc::type_id::blob_type);
    std::string_view sv = "test data";
    v = sv;
    EXPECT_TRUE(v.is_blob());
    EXPECT_EQ(v.as_blob().as_string_view(), "test data");
}

// 测试 operator=(const blob& b) 当 variant 是 blob 类型时
TEST_F(VariantEdgeCasesTest, OperatorAssignBlobToBlob) {
    typed_variant v(mc::type_id::blob_type);
    std::string blob_str = "blob content";
    mc::blob blob_data(blob_str.data(), blob_str.size());
    v = blob_data;
    EXPECT_TRUE(v.is_blob());
    EXPECT_EQ(v.as_blob().as_string_view(), "blob content");
}

// 测试 get_string() 异常情况
TEST_F(VariantEdgeCasesTest, GetStringException) {
    variant v(42);
    EXPECT_THROW({ v.get_string(); }, mc::exception);
}

// 测试 get_blob() 异常情况
TEST_F(VariantEdgeCasesTest, GetBlobException) {
    variant v("string");
    EXPECT_THROW({ v.get_blob(); }, mc::exception);
}

// 测试 get_array() 异常情况
TEST_F(VariantEdgeCasesTest, GetArrayException) {
    variant v("string");
    EXPECT_THROW({ v.get_array(); }, mc::exception);
}

// 测试 get_object() 异常情况
TEST_F(VariantEdgeCasesTest, GetObjectException) {
    variant v("string");
    EXPECT_THROW({ v.get_object(); }, mc::exception);
}

// 测试 as_extension() 异常情况
TEST_F(VariantEdgeCasesTest, AsExtensionException) {
    variant v("string");
    EXPECT_THROW({ v.as_extension(); }, mc::exception);
}

// 测试 operator+(detail::numeric_t rhs) 与数组类型
TEST_F(VariantEdgeCasesTest, OperatorPlusNumericWithArray) {
    variant v = variants{1, 2, 3};
    variant result = v + 4;
    EXPECT_TRUE(result.is_array());
    EXPECT_EQ(result.size(), 4);
    EXPECT_EQ(result[3], 4);
}

// 测试 operator+(detail::numeric_t rhs) 与对象类型（应该抛出异常）
TEST_F(VariantEdgeCasesTest, OperatorPlusNumericWithObject) {
    variant v = dict{{"key", "value"}};
    EXPECT_THROW({ v + 42; }, mc::exception);
}

// 测试 operator+(detail::numeric_t rhs) 与字符串类型
TEST_F(VariantEdgeCasesTest, OperatorPlusNumericWithString) {
    variant v("test");
    variant result = v + 42;
    EXPECT_TRUE(result.is_string());
    EXPECT_EQ(result.as_string(), "test42");
}

// 测试 operator-(detail::numeric_t rhs) 下溢情况（无符号整数）
TEST_F(VariantEdgeCasesTest, OperatorMinusNumericUnderflow) {
    variant v(uint64_t(10));
    variant result = v - uint64_t(20);
    // 下溢时应该转换为有符号整数
    EXPECT_TRUE(result.is_signed_integer());
    EXPECT_EQ(result.as_int64(), -10);
}

// 测试 operator/(detail::numeric_t rhs) 除零错误
TEST_F(VariantEdgeCasesTest, OperatorDivideNumericByZero) {
    variant v(42);
    EXPECT_THROW({ v / 0; }, mc::exception);
    EXPECT_THROW({ v / 0.0; }, mc::exception);
}

// 测试 operator%(detail::numeric_t rhs) 除零错误
TEST_F(VariantEdgeCasesTest, OperatorModuloNumericByZero) {
    variant v(42);
    EXPECT_THROW({ v % 0; }, mc::exception);
}

// 测试 operator==(const variants& other) 数组比较
TEST_F(VariantEdgeCasesTest, OperatorEqualWithVariants) {
    variant v1 = variants{1, 2, 3};
    variants arr = {1, 2, 3};
    EXPECT_TRUE(v1 == arr);

    variants arr2 = {1, 2, 4};
    EXPECT_FALSE(v1 == arr2);
}

// 测试 operator<(const variants& other) 数组比较
TEST_F(VariantEdgeCasesTest, OperatorLessWithVariants) {
    variant v1 = variants{1, 2, 3};
    variants arr1 = {1, 2, 2};
    EXPECT_FALSE(v1 < arr1);

    variants arr2 = {1, 2, 4};
    EXPECT_TRUE(v1 < arr2);

    variants arr3 = {1, 2, 3};
    EXPECT_FALSE(v1 < arr3);
}

// 测试 operator>(const variants& other) 数组比较
TEST_F(VariantEdgeCasesTest, OperatorGreaterWithVariants) {
    variant v1 = variants{1, 2, 3};
    variants arr1 = {1, 2, 2};
    EXPECT_TRUE(v1 > arr1);

    variants arr2 = {1, 2, 4};
    EXPECT_FALSE(v1 > arr2);

    variants arr3 = {1, 2, 3};
    EXPECT_FALSE(v1 > arr3);
}

// 测试 operator<(detail::numeric_t rhs) 与 double NaN
TEST_F(VariantEdgeCasesTest, OperatorLessNumericWithNaN) {
    variant v(std::numeric_limits<double>::quiet_NaN());
    EXPECT_FALSE(v < 0.0);
    EXPECT_FALSE(v < 42);
}

// 测试 operator>(detail::numeric_t rhs) 与 double NaN
TEST_F(VariantEdgeCasesTest, OperatorGreaterNumericWithNaN) {
    variant v(std::numeric_limits<double>::quiet_NaN());
    EXPECT_FALSE(v > 0.0);
    EXPECT_FALSE(v > 42);
}

// 测试 operator<(detail::numeric_t rhs) 与字符串类型
TEST_F(VariantEdgeCasesTest, OperatorLessNumericWithString) {
    variant v("42");
    EXPECT_TRUE(v < 100);
    EXPECT_FALSE(v < 10);
}

// 测试 operator>(detail::numeric_t rhs) 与字符串类型
TEST_F(VariantEdgeCasesTest, OperatorGreaterNumericWithString) {
    variant v("42");
    EXPECT_TRUE(v > 10);
    EXPECT_FALSE(v > 100);
}

// 测试 operator<(detail::numeric_t rhs) 与 blob 类型
TEST_F(VariantEdgeCasesTest, OperatorLessNumericWithBlob) {
    variant v = mc::blob{'4', '2'};
    EXPECT_TRUE(v < 100);
    EXPECT_FALSE(v < 10);
}

// 测试 operator>(detail::numeric_t rhs) 与 blob 类型
TEST_F(VariantEdgeCasesTest, OperatorGreaterNumericWithBlob) {
    variant v = mc::blob{'4', '2'};
    EXPECT_TRUE(v > 10);
    EXPECT_FALSE(v > 100);
}

// 测试 operator<=(detail::numeric_t rhs) 与 double NaN
TEST_F(VariantEdgeCasesTest, OperatorLessEqualNumericWithNaN) {
    variant v(std::numeric_limits<double>::quiet_NaN());
    EXPECT_FALSE(v <= 0.0);
    EXPECT_FALSE(v <= 42);
}

// 测试 operator>=(detail::numeric_t rhs) 与 double NaN
TEST_F(VariantEdgeCasesTest, OperatorGreaterEqualNumericWithNaN) {
    variant v(std::numeric_limits<double>::quiet_NaN());
    EXPECT_FALSE(v >= 0.0);
    EXPECT_FALSE(v >= 42);
}

// 测试 operator==(detail::numeric_t rhs) 与各种类型
TEST_F(VariantEdgeCasesTest, OperatorEqualNumericWithVariousTypes) {
    // 测试 double
    variant v1(3.14);
    EXPECT_TRUE(v1 == 3.14);
    EXPECT_FALSE(v1 == 3.15);

    // 测试 int64
    variant v2(42);
    EXPECT_TRUE(v2 == 42);
    EXPECT_FALSE(v2 == 43);

    // 测试 uint64
    variant v3(uint64_t(100));
    EXPECT_TRUE(v3 == uint64_t(100));
    EXPECT_FALSE(v3 == uint64_t(101));

    // 测试 bool
    variant v4(true);
    EXPECT_TRUE(v4 == true);
    EXPECT_FALSE(v4 == false);

    // 测试字符串
    variant v5("42");
    EXPECT_TRUE(v5 == 42);
    EXPECT_FALSE(v5 == 43);

    // 测试 blob
    variant v6 = mc::blob{'4', '2'};
    EXPECT_TRUE(v6 == 42);
    EXPECT_FALSE(v6 == 43);
}

// 测试 operator<(const variants& other) 异常情况（非数组类型）
TEST_F(VariantEdgeCasesTest, OperatorLessVariantsException) {
    variant v("string");
    variants arr = {1, 2, 3};
    EXPECT_THROW({ bool result = v < arr; MC_UNUSED(result); }, mc::exception);
}

// 测试 operator>(const variants& other) 异常情况（非数组类型）
TEST_F(VariantEdgeCasesTest, OperatorGreaterVariantsException) {
    variant v("string");
    variants arr = {1, 2, 3};
    EXPECT_THROW({ bool result = v > arr; MC_UNUSED(result); }, mc::exception);
}

TEST_F(VariantEdgeCasesTest, ExtensionNullVariant) {
    auto ext_ptr = mc::make_shared<unsupported_extension>(42);
    variant v(ext_ptr);

    auto base = v.as_extension();
    ASSERT_TRUE(base != nullptr);

    EXPECT_THROW(base->get(0), mc::invalid_op_exception);
    EXPECT_THROW(base->set(0, variant(1)), mc::invalid_op_exception);
    EXPECT_THROW(base->get("key"), mc::invalid_op_exception);
    EXPECT_THROW(base->set("key", variant(1)), mc::invalid_op_exception);
}

TEST_F(VariantEdgeCasesTest, ExtensionBaseAccessThrows) {
    auto ext_ptr = mc::make_shared<unsupported_extension>(42);
    variant v(ext_ptr);

    auto base = v.as_extension();
    ASSERT_TRUE(base != nullptr);

    EXPECT_THROW(base->get(0), mc::invalid_op_exception);
    EXPECT_THROW(base->set(0, variant(1)), mc::invalid_op_exception);
    EXPECT_THROW(base->get("key"), mc::invalid_op_exception);
    EXPECT_THROW(base->set("key", variant(1)), mc::invalid_op_exception);
}

TEST_F(VariantEdgeCasesTest, VariantThrowHelperFunctions) {
    EXPECT_STREQ(mc::get_type_name_internal(mc::type_id::int32_type), "int32");
    EXPECT_STREQ(mc::get_type_name_internal(static_cast<mc::type_id>(999)), "unknown");

    EXPECT_THROW(mc::throw_type_error("array", mc::type_id::bool_type), mc::invalid_arg_exception);
    EXPECT_THROW(mc::throw_type_error("custom", static_cast<mc::type_id>(999)), mc::invalid_arg_exception);
    EXPECT_THROW(mc::throw_unknow_type_error(static_cast<mc::type_id>(999)), mc::invalid_arg_exception);
    EXPECT_THROW(mc::detail::throw_method_arg_not_match("compute", "int", "string"), mc::invalid_arg_exception);

    EXPECT_THROW(mc::throw_invalid_type_comparison_error("int", "dict", ">"), mc::invalid_op_exception);
    EXPECT_THROW(mc::throw_invalid_type_operation_error("dict", "string", "+"), mc::invalid_op_exception);
    EXPECT_THROW(mc::throw_divide_by_zero_exception("divide"), mc::divide_by_zero_exception);

    EXPECT_THROW(mc::throw_out_of_range_error("index out"), mc::out_of_range_exception);
    EXPECT_THROW(mc::throw_out_of_range_error(5, 3), mc::out_of_range_exception);
    EXPECT_THROW(mc::throw_bad_cast_error("bad cast"), mc::bad_cast_exception);
    EXPECT_THROW(mc::throw_runtime_error("runtime"), mc::runtime_exception);
    EXPECT_THROW(mc::throw_not_supported_error("operation"), mc::invalid_op_exception);
    EXPECT_THROW(mc::throw_extension_null_error(), mc::runtime_exception);
    EXPECT_THROW(mc::throw_container_overflow_error("array"), mc::overflow_exception);

    EXPECT_EQ(mc::calculate_str_hash(""), 0U);
    EXPECT_NE(mc::calculate_str_hash("hash"), 0U);
}

} // namespace test
} // namespace mc

