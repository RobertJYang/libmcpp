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

#include <mc/fmt/format.h>

#include <array>
#include <bitset>
#include <deque>
#include <gtest/gtest.h>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

using namespace mc::fmt;

TEST(format_std_test, vector_basic) {
    std::vector<int> v{1, 2, 3};
    EXPECT_EQ(sformat("{}", v), "[1,2,3]");
}

TEST(format_std_test, vector_empty) {
    std::vector<int> v;
    EXPECT_EQ(sformat("{}", v), "[]");
}

TEST(format_std_test, list_basic) {
    std::list<std::string> l{"a", "b", "c"};
    EXPECT_EQ(sformat("{}", l), "[\"a\",\"b\",\"c\"]");
}

TEST(format_std_test, set_basic) {
    std::set<int> s{3, 1, 2};
    EXPECT_EQ(sformat("{}", s), "{1,2,3}");
}

TEST(format_std_test, map_basic) {
    std::map<std::string, int> m{{"a", 1}, {"b", 2}};
    EXPECT_EQ(sformat("{}", m), "{\"a\":1,\"b\":2}");
}

TEST(format_std_test, nested_vector) {
    std::vector<std::vector<int>> v{{1, 2}, {3}};
    EXPECT_EQ(sformat("{}", v), "[[1,2],[3]]");
}

TEST(format_std_test, nested_map_vector) {
    std::map<std::string, std::vector<int>> m{{"x", {1, 2}}, {"y", {3}}};
    EXPECT_EQ(sformat("{}", m), "{\"x\":[1,2],\"y\":[3]}");
}

// vector
TEST(format_std_test, vector_int) {
    std::vector<int> v{4, 5, 6};
    EXPECT_EQ(sformat("{}", v), "[4,5,6]");
}

// list
TEST(format_std_test, list_string) {
    std::list<std::string> l{"x", "y"};
    EXPECT_EQ(sformat("{}", l), "[\"x\",\"y\"]");
}

// deque
TEST(format_std_test, deque_double) {
    std::deque<double> d{1.1, 2.2};
    EXPECT_EQ(sformat("{}", d), "[1.1,2.2]");
}

// set
TEST(format_std_test, set_int) {
    std::set<int> s{2, 1};
    EXPECT_EQ(sformat("{}", s), "{1,2}");
}

// unordered_set
TEST(format_std_test, unordered_set_int) {
    std::unordered_set<int> us{3, 2};
    auto                    str = sformat("{}", us);
    EXPECT_TRUE(str == "{2,3}" || str == "{3,2}"); // 顺序不保证
}

// map
TEST(format_std_test, map_nested_container) {
    std::map<int, std::vector<int>> m{{1, {2, 3}}, {4, {5}}};
    EXPECT_EQ(sformat("{}", m), "{1:[2,3],4:[5]}");
}

// unordered_map
TEST(format_std_test, unordered_map_str_int) {
    std::unordered_map<std::string, int> um{{"x", 7}, {"y", 8}};
    auto                                 str = sformat("{}", um);
    EXPECT_TRUE(str == "{\"x\":7,\"y\":8}" || str == "{\"y\":8,\"x\":7}"); // 顺序不保证
}

// array
TEST(format_std_test, array_int) {
    std::array<int, 3> arr{{7, 8, 9}};
    EXPECT_EQ(sformat("{}", arr), "[7,8,9]");
}

// pair
TEST(format_std_test, pair_int_str) {
    std::pair<int, std::string> p{1, "foo"};
    EXPECT_EQ(sformat("{}", p), "(1,\"foo\")");
}

// tuple
TEST(format_std_test, tuple_mixed) {
    std::tuple<int, std::string, double, char> t{1, "bar", 2.5, 'a'};
    EXPECT_EQ(sformat("{}", t), "(1,\"bar\",2.5,'a')");
}

// optional
TEST(format_std_test, optional_int) {
    std::optional<int> o1;
    std::optional<int> o2{42};
    EXPECT_EQ(sformat("{}", o1), "none");
    EXPECT_EQ(sformat("{}", o2), "optional(42)");
}

// variant
TEST(format_std_test, variant_basic) {
    std::variant<int, std::string> v1{123};
    std::variant<int, std::string> v2{"hello"};
    EXPECT_EQ(sformat("{}", v1), "variant(123)");
    EXPECT_EQ(sformat("{}", v2), "variant(\"hello\")");
}

// bitset
TEST(format_std_test, bitset_basic) {
    std::bitset<4> b(std::string("1010"));
    EXPECT_EQ(sformat("{}", b), "1010");
}

// unique_ptr
TEST(format_std_test, unique_ptr_basic) {
    std::unique_ptr<int> up1;
    std::unique_ptr<int> up2(new int(7));
    EXPECT_EQ(sformat("{}", up1), "nullptr");
    EXPECT_EQ(sformat("{}", up2), "ptr(7)");
}

// shared_ptr
TEST(format_std_test, shared_ptr_basic) {
    std::shared_ptr<int> sp1;
    std::shared_ptr<int> sp2 = std::make_shared<int>(8);
    EXPECT_EQ(sformat("{}", sp1), "nullptr");
    EXPECT_EQ(sformat("{}", sp2), "ptr(8)");
}

// weak_ptr
TEST(format_std_test, weak_ptr_basic) {
    std::weak_ptr<int>   wp1;
    std::shared_ptr<int> sp  = std::make_shared<int>(9);
    std::weak_ptr<int>   wp2 = sp;
    EXPECT_EQ(sformat("{}", wp1), "nullptr");
    EXPECT_EQ(sformat("{}", wp2), "ptr(9)");
}

// string_view
TEST(format_std_test, string_view_basic) {
    std::string_view sv = "abc";
    EXPECT_EQ(sformat("{}", sv), "abc");
}

TEST(format_std_test, pair_char_int) {
    std::pair<char, int> p{'x', 42};
    EXPECT_EQ(sformat("{}", p), "('x',42)");
}

TEST(format_std_test, tuple_char_string) {
    std::tuple<char, std::string, int> t{'a', "hello", 123};
    EXPECT_EQ(sformat("{}", t), "('a',\"hello\",123)");
}

TEST(format_std_test, vector_char) {
    std::vector<char> v{'a', 'b', 'c'};
    EXPECT_EQ(sformat("{}", v), "[\'a\',\'b\',\'c\']"); // 容器中的 char 不加引号
}

TEST(format_std_test, vector_string) {
    std::vector<std::string> v{"hello", "world"};
    EXPECT_EQ(sformat("{}", v), "[\"hello\",\"world\"]"); // 容器中的字符串不加引号
}

TEST(format_std_test, set_string) {
    std::set<std::string> s{"foo", "bar"};
    EXPECT_EQ(sformat("{}", s), "{\"bar\",\"foo\"}"); // 容器中的字符串不加引号
}

TEST(format_std_test, optional_string) {
    std::optional<std::string> o1;
    std::optional<std::string> o2{"test"};
    EXPECT_EQ(sformat("{}", o1), "none");
    EXPECT_EQ(sformat("{}", o2), "optional(\"test\")"); // optional 中的字符串加引号
}

TEST(format_std_test, optional_char) {
    std::optional<char> o1;
    std::optional<char> o2{'z'};
    EXPECT_EQ(sformat("{}", o1), "none");
    EXPECT_EQ(sformat("{}", o2), "optional('z')"); // optional 中的 char 加单引号
}

TEST(format_std_test, variant_char_string) {
    std::variant<char, std::string> v1{'x'};
    std::variant<char, std::string> v2{"world"};
    EXPECT_EQ(sformat("{}", v1), "variant('x')");       // variant 中的 char 加单引号
    EXPECT_EQ(sformat("{}", v2), "variant(\"world\")"); // variant 中的字符串加双引号
}

TEST(format_std_test, pair_string_view_int) {
    std::string_view                 sv = "test";
    std::pair<std::string_view, int> p{sv, 100};
    EXPECT_EQ(sformat("{}", p), "(\"test\",100)"); // string_view 在 pair 中加双引号
}

TEST(format_std_test, tuple_unsigned_char) {
    unsigned char                           uc = 'A';
    uint8_t                                 ub = 'B';
    std::tuple<unsigned char, uint8_t, int> t{uc, ub, 200};
    EXPECT_EQ(sformat("{}", t), "(65,66,200)"); // unsigned char 和 uint8_t 不加引号，当作整数
}

TEST(format_std_test, signed_char_test) {
    signed char                                  sc = 'A';
    int8_t                                       sb = 'B';
    std::tuple<signed char, int8_t, std::string> t{sc, sb, "signed"};
    EXPECT_EQ(sformat("{}", t), "('A','B',\"signed\")"); // signed char 和 int8_t 不加引号，当作整数
}