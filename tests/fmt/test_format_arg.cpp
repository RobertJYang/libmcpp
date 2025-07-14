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
#include <mc/fmt/format.h>

using namespace mc::fmt;

TEST(format_arg, basicTypes) {
    // 测试基本类型构造
    detail::format_arg int_arg(static_cast<int>(42));
    detail::format_arg float_arg(3.14f);
    std::string        str = "hello";
    detail::format_arg string_arg(str);              // std::string
    detail::format_arg sv_arg(string_view("world")); // string_view
    detail::format_arg cstr_arg("hello");            // 字符串字面量，推断为 const char*
    detail::format_arg bool_arg(true);
    detail::format_arg char_arg('A');

    EXPECT_TRUE(int_arg.is_signed_integer());
    EXPECT_TRUE(float_arg.is_float());
    EXPECT_TRUE(string_arg.is_string());
    EXPECT_TRUE(sv_arg.is_string());
    EXPECT_TRUE(cstr_arg.is_cstring());
    EXPECT_TRUE(bool_arg.is_bool());
    EXPECT_TRUE(char_arg.is_char());

    EXPECT_EQ(int_arg.as_signed(), 42);
    EXPECT_FLOAT_EQ(float_arg.as_float(), 3.14f);
    EXPECT_EQ(string_arg.as_string(), "hello");
    EXPECT_EQ(sv_arg.as_string(), "world");
    EXPECT_STREQ(cstr_arg.as_cstring(), "hello");
    EXPECT_EQ(bool_arg.as_bool(), true);
    EXPECT_EQ(char_arg.as_char(), 'A');
}

TEST(format_arg, typeChecking) {
    detail::format_arg arg;

    // 默认构造应该是 null
    EXPECT_TRUE(arg.is_null());
    EXPECT_FALSE(static_cast<bool>(arg));

    // 设置不同类型的值
    arg = detail::format_arg(static_cast<int>(123));
    EXPECT_TRUE(arg.is_signed_integer());
    EXPECT_FALSE(arg.is_string());
    EXPECT_TRUE(static_cast<bool>(arg));

    arg = detail::format_arg("test"); // 字符串字面量，推断为 const char*
    EXPECT_TRUE(arg.is_cstring());
    EXPECT_FALSE(arg.is_string());
    EXPECT_TRUE(static_cast<bool>(arg));

    std::string str = "test2";
    arg             = detail::format_arg(str); // std::string
    EXPECT_TRUE(arg.is_string());
    EXPECT_FALSE(arg.is_cstring());
    EXPECT_TRUE(static_cast<bool>(arg));
}

TEST(format_arg, visitPattern) {
    detail::format_arg int_arg(42);
    std::string        str = "hello";
    detail::format_arg string_arg(str);
    detail::format_arg cstr_arg("world");

    // 测试访问者模式
    int result = 0;
    int_arg.visit([&](auto&& val) {
        using T = std::decay_t<decltype(val)>;
        if constexpr (mc::fmt::detail::is_integer_v<T>) {
            result = val;
        }
    });
    EXPECT_EQ(result, 42);

    std::string str_result;
    string_arg.visit([&](auto&& val) {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, string_view>) {
            str_result = std::string(val);
        }
    });
    EXPECT_EQ(str_result, "hello");

    // 访问 const char*
    std::string cstr_result;
    cstr_arg.visit([&](auto&& val) {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, const char*>) {
            cstr_result = std::string(val);
        }
    });
    EXPECT_EQ(cstr_result, "world");
}

TEST(format_arg, argStore) {
    // 测试参数存储
    std::string       str = "hello";
    detail::arg_store store(static_cast<int>(42),
                            str,
                            static_cast<double>(3.14),
                            "cstr");

    EXPECT_EQ(store.size(), 4);

    auto arg0 = store.get_positional(0);
    auto arg1 = store.get_positional(1);
    auto arg2 = store.get_positional(2);
    auto arg3 = store.get_positional(3);

    ASSERT_NE(arg0, nullptr);
    ASSERT_NE(arg1, nullptr);
    ASSERT_NE(arg2, nullptr);
    ASSERT_NE(arg3, nullptr);

    EXPECT_TRUE(arg0->is_signed_integer());
    EXPECT_TRUE(arg1->is_string());
    EXPECT_TRUE(arg2->is_double());
    EXPECT_TRUE(arg3->is_cstring());

    EXPECT_EQ(arg0->as_signed(), 42);
    EXPECT_EQ(arg1->as_string(), "hello");
    EXPECT_DOUBLE_EQ(arg2->as_double(), 3.14);
    EXPECT_STREQ(arg3->as_cstring(), "cstr");
}

TEST(format_arg, formatWithVariant) {
    // 测试使用新的 variant 实现进行格式化
    auto result = sformat("整数: {}, 字符串: {}, 浮点数: {:.2f}", 42, "hello", 3.14159);
    EXPECT_EQ(result, "整数: 42, 字符串: hello, 浮点数: 3.14");
}

TEST(format_arg, edgeCases) {
    // 测试边界情况
    detail::format_arg null_arg;
    detail::format_arg zero_int(static_cast<int>(0));
    std::string        str = "";
    detail::format_arg empty_string(str);
    detail::format_arg empty_cstr("");

    EXPECT_TRUE(null_arg.is_null());
    EXPECT_TRUE(zero_int.is_signed_integer());
    EXPECT_TRUE(empty_string.is_string());
    EXPECT_TRUE(empty_cstr.is_cstring());

    EXPECT_EQ(zero_int.as_signed(), 0);
    EXPECT_EQ(empty_string.as_string(), "");
    EXPECT_STREQ(empty_cstr.as_cstring(), "");
}

TEST(format_arg, pointerTypes) {
    int         value = 42;
    const void* ptr   = &value;

    detail::format_arg pointer_arg(ptr);
    EXPECT_TRUE(pointer_arg.is_pointer());
    EXPECT_EQ(pointer_arg.as_pointer(), ptr);
}

TEST(format_arg, longTypes) {
    long long          big_int  = 9223372036854775807LL;
    unsigned long long big_uint = 18446744073709551615ULL;

    detail::format_arg long_arg(big_int);
    detail::format_arg ulong_arg(big_uint);

    EXPECT_TRUE(long_arg.is_signed_integer());
    EXPECT_TRUE(ulong_arg.is_unsigned_integer());

    EXPECT_EQ(long_arg.as_signed(), big_int);
    EXPECT_EQ(ulong_arg.as_unsigned(), big_uint);
}