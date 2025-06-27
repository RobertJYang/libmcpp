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
 * @file test_variant_visitor.cpp
 * @brief 测试 variant 的访问者模式
 */
#include <gtest/gtest.h>
#include <mc/dict.h>
#include <mc/variant.h>
#include <string>
#include <test_utilities/test_base.h>
#include <type_traits>

namespace mc {
namespace test {

class VariantVisitorTest : public mc::test::TestBase {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }

    // 自定义访问者类
    class TestVisitor : public variant::visitor {
    public:
        mutable std::string result;

        void handle() const override {
            result = "null";
        }

        void handle(const int64_t& value) const override {
            result = "int64: " + std::to_string(value);
        }

        void handle(const uint64_t& value) const override {
            result = "uint64: " + std::to_string(value);
        }

        void handle(const double& value) const override {
            result = "double: " + std::to_string(value);
        }

        void handle(const bool& value) const override {
            result = value ? "bool: true" : "bool: false";
        }

        void handle(const std::string& value) const override {
            result = "string: " + value;
        }

        void handle(const dict& value) const override {
            result = "dict: " + std::to_string(value.size()) + " items";
        }

        void handle(const variants& value) const override {
            result = "array: " + std::to_string(value.size()) + " items";
        }

        void handle(const blob& value) const override {
            result = "blob: " + std::to_string(value.data.size()) + " bytes";
        }

        void handle(const variant_extension_base& value) const override {
            result = "extension: ";
            result += value.get_type_name();
        }
    };
};

/**
 * @brief 测试 visit 成员函数
 */
TEST_F(VariantVisitorTest, VisitNull) {
    variant     v; // null
    TestVisitor visitor;
    v.visit(visitor);
    EXPECT_EQ(visitor.result, "null");
}

TEST_F(VariantVisitorTest, VisitInt32) {
    variant     v(42); // int32
    TestVisitor visitor;
    v.visit(visitor);
    EXPECT_EQ(visitor.result, "int64: 42");
}

TEST_F(VariantVisitorTest, VisitUInt32) {
    variant     v(42u); // uint32
    TestVisitor visitor;
    v.visit(visitor);
    EXPECT_EQ(visitor.result, "uint64: 42");
}

TEST_F(VariantVisitorTest, VisitDouble) {
    variant     v(3.14); // double
    TestVisitor visitor;
    v.visit(visitor);
    EXPECT_EQ(visitor.result, "double: 3.140000");
}

TEST_F(VariantVisitorTest, VisitBool) {
    variant     v(true); // bool
    TestVisitor visitor;
    v.visit(visitor);
    EXPECT_EQ(visitor.result, "bool: true");
}

TEST_F(VariantVisitorTest, VisitString) {
    variant     v("hello"); // string
    TestVisitor visitor;
    v.visit(visitor);
    EXPECT_EQ(visitor.result, "string: hello");
}

TEST_F(VariantVisitorTest, VisitDict) {
    mutable_dict md;
    md["key"]     = "value";
    dict        d = md;
    variant     v(d); // dict
    TestVisitor visitor;
    v.visit(visitor);
    EXPECT_EQ(visitor.result, "dict: 1 items");
}

TEST_F(VariantVisitorTest, VisitArray) {
    variants arr;
    arr.push_back(1);
    arr.push_back(2);
    variant     v(arr); // array
    TestVisitor visitor;
    v.visit(visitor);
    EXPECT_EQ(visitor.result, "array: 2 items");
}

TEST_F(VariantVisitorTest, VisitBlob) {
    blob b;
    b.data = {'a', 'b', 'c'};
    variant     v(b); // blob
    TestVisitor visitor;
    v.visit(visitor);
    EXPECT_EQ(visitor.result, "blob: 3 bytes");
}

/**
 * @brief 测试 visit_with 函数
 */
TEST_F(VariantVisitorTest, VisitWithNull) {
    variant v; // null
    auto    result = v.visit_with([](auto&& value) -> std::string {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, std::nullptr_t>) {
            return "null";
        } else {
            return "not null";
        }
    });
    EXPECT_EQ(result, "null");
}

TEST_F(VariantVisitorTest, VisitWithInt32) {
    variant v(42); // int32
    auto    result = v.visit_with([](auto&& value) -> std::string {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, int64_t>) {
            return "int64: " + std::to_string(value);
        } else {
            return "not int64";
        }
    });
    EXPECT_EQ(result, "int64: 42");
}

TEST_F(VariantVisitorTest, VisitWithUInt32) {
    variant v(42u); // uint32
    auto    result = v.visit_with([](auto&& value) -> std::string {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, uint64_t>) {
            return "uint64: " + std::to_string(value);
        } else {
            return "not uint64";
        }
    });
    EXPECT_EQ(result, "uint64: 42");
}

TEST_F(VariantVisitorTest, VisitWithDouble) {
    variant v(3.14); // double
    auto    result = v.visit_with([](auto&& value) -> std::string {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, double>) {
            return "double: " + std::to_string(value);
        } else {
            return "not double";
        }
    });
    EXPECT_EQ(result, "double: 3.140000");
}

TEST_F(VariantVisitorTest, VisitWithBool) {
    variant v(true); // bool
    auto    result = v.visit_with([](auto&& value) -> std::string {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, bool>) {
            return value ? "bool: true" : "bool: false";
        } else {
            return "not bool";
        }
    });
    EXPECT_EQ(result, "bool: true");
}

TEST_F(VariantVisitorTest, VisitWithString) {
    variant v("hello"); // string
    auto    result = v.visit_with([](auto&& value) -> std::string {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, std::string>) {
            return "string: " + value;
        } else {
            return "not string";
        }
    });
    EXPECT_EQ(result, "string: hello");
}

TEST_F(VariantVisitorTest, VisitWithDict) {
    mutable_dict md;
    md["key"] = "value";
    dict    d = md;
    variant v(d); // dict
    auto    result = v.visit_with([](auto&& value) -> std::string {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, dict>) {
            return "dict: " + std::to_string(value.size()) + " items";
        } else {
            return "not dict";
        }
    });
    EXPECT_EQ(result, "dict: 1 items");
}

TEST_F(VariantVisitorTest, VisitWithArray) {
    variants arr;
    arr.push_back(1);
    arr.push_back(2);
    variant v(arr); // array
    auto    result = v.visit_with([](auto&& value) -> std::string {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, variants>) {
            return "array: " + std::to_string(value.size()) + " items";
        } else {
            return "not array";
        }
    });
    EXPECT_EQ(result, "array: 2 items");
}

TEST_F(VariantVisitorTest, VisitWithBlob) {
    blob b;
    b.data = {'a', 'b', 'c'};
    variant v(b); // blob
    auto    result = v.visit_with([](auto&& value) -> std::string {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, blob>) {
            return "blob: " + std::to_string(value.data.size()) + " bytes";
        } else {
            return "not blob";
        }
    });
    EXPECT_EQ(result, "blob: 3 bytes");
}

/**
 * @brief 测试全局 visit 函数
 */
TEST_F(VariantVisitorTest, GlobalVisit) {
    // 测试不同类型的 variant
    {
        variant v; // null
        auto    result = v.visit_with([](auto&& value) -> std::string {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, std::nullptr_t>) {
                return "null";
            } else {
                return "not null";
            }
        });
        EXPECT_EQ(result, "null");
    }

    {
        variant v(42); // int32
        auto    result = v.visit_with([](auto&& value) -> std::string {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, int64_t>) {
                return "int64: " + std::to_string(value);
            } else {
                return "not int64";
            }
        });
        EXPECT_EQ(result, "int64: 42");
    }

    // 测试复杂的访问者，处理所有类型
    {
        variant v1(42);      // int32
        variant v2(3.14);    // double
        variant v3("hello"); // string
        variant v4(true);    // bool

        auto visitor = [](auto&& value) -> std::string {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, int64_t>) {
                return "int64: " + std::to_string(value);
            } else if constexpr (std::is_same_v<T, uint64_t>) {
                return "uint64: " + std::to_string(value);
            } else if constexpr (std::is_same_v<T, double>) {
                return "double: " + std::to_string(value);
            } else if constexpr (std::is_same_v<T, bool>) {
                return value ? "bool: true" : "bool: false";
            } else if constexpr (std::is_same_v<T, std::string>) {
                return "string: " + value;
            } else if constexpr (std::is_same_v<T, dict>) {
                return "dict: " + std::to_string(value.size()) + " items";
            } else if constexpr (std::is_same_v<T, variants>) {
                return "array: " + std::to_string(value.size()) + " items";
            } else if constexpr (std::is_same_v<T, blob>) {
                return "blob: " + std::to_string(value.data.size()) + " bytes";
            } else if constexpr (std::is_same_v<T, std::nullptr_t>) {
                return "null";
            } else {
                return "unknown";
            }
        };

        EXPECT_EQ(v1.visit_with(visitor), "int64: 42");
        EXPECT_EQ(v2.visit_with(visitor), "double: 3.140000");
        EXPECT_EQ(v3.visit_with(visitor), "string: hello");
        EXPECT_EQ(v4.visit_with(visitor), "bool: true");
    }
}

/**
 * @brief 测试 visit_with 函数的返回值类型推导
 */
TEST_F(VariantVisitorTest, VisitWithReturnType) {
    // 测试返回不同类型的值
    {
        variant v(42); // int32
        auto    result1 = v.visit_with([](auto&& value) -> int {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, int64_t>) {
                return static_cast<int>(value);
            } else {
                return 0;
            }
        });
        EXPECT_EQ(result1, 42);

        auto result2 = v.visit_with([](auto&& value) -> double {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, int64_t>) {
                return static_cast<double>(value);
            } else {
                return 0.0;
            }
        });
        EXPECT_DOUBLE_EQ(result2, 42.0);

        auto result3 = v.visit_with([](auto&& value) -> std::string {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, int64_t>) {
                return std::to_string(value);
            } else {
                return "";
            }
        });
        EXPECT_EQ(result3, "42");
    }

    // 测试 void 返回类型
    {
        variant v(42); // int32
        bool    called = false;
        v.visit_with([&called](auto&& value) -> void {
            called = true;
        });
        EXPECT_TRUE(called);
    }
}

} // namespace test
} // namespace mc