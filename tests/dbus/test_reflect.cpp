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

#include <gtest/gtest.h>
#include <mc/dbus/message.h>

using namespace mc::dbus;

struct test_struct {
    int8_t      i8;
    uint8_t     u8;
    int16_t     i16;
    uint16_t    u16;
    int32_t     i32;
    uint32_t    u32;
    int64_t     i64;
    uint64_t    u64;
    float       f;
    double      d;
    std::string str;
    bool        b;
    path        p;
    signature   sig;

    // mc 类型
    mc::variant  v; // v
    mc::variants a; // av
    mc::dict     m; // a{sv}

    // 标准库类型
    std::optional<int>                   std_o;        // ai
    std::pair<int, std::string>          std_p;        // (is)
    std::tuple<int, std::string, bool>   std_t;        // (isb)
    std::vector<int>                     std_vec;      // a(i)
    std::list<std::string>               std_list;     // a(s)
    std::map<int, int>                   std_map;      // a{ii}
    std::multimap<int, int>              std_mmap;     // a{ii}
    std::unordered_map<std::string, int> std_umap;     // a{si}
    std::array<int, 5>                   std_array;    // a(i)
    std::deque<int>                      std_deque;    // a(i)
    std::set<int>                        std_set;      // a(i)
    std::multiset<std::string>           std_multiset; // a(s)
    std::unordered_set<int>              std_uset;     // a(i)
};

MC_REFLECT(
    test_struct, (i8)(u8)(i16)(u16)(i32)(u32)(i64)(u64)(f)(d)(str)(b)(p)(sig)
    // mc 复杂类型
    (v)(a)(m)
    // 标准库类型
    (std_o)(std_p)(std_t)(std_vec)(std_list)(std_map)(std_mmap)(std_umap)(std_array)(std_deque)(std_set)(std_multiset)(std_uset))

// 测试C++标准库类型反射
class reflect_test : public ::testing::Test {
protected:
    void SetUp() override {
        ts_out.i8           = 1;
        ts_out.u8           = 2;
        ts_out.i16          = 3;
        ts_out.u16          = 4;
        ts_out.i32          = 5;
        ts_out.u32          = 6;
        ts_out.i64          = 7;
        ts_out.u64          = 8;
        ts_out.f            = 9.0f;
        ts_out.d            = 10.0;
        ts_out.str          = "Hello";
        ts_out.b            = true;
        ts_out.p            = "/org/freedesktop/DBus";
        ts_out.sig          = "s";
        ts_out.v            = "world";
        ts_out.a            = {"1", "12", "123", "1234", "12345"};
        ts_out.m            = {{"key1", "1"}, {"key12", "12"}, {"key123", "123"}};
        ts_out.std_o        = 1;
        ts_out.std_p        = {1, "12"};
        ts_out.std_t        = {1, "12", true};
        ts_out.std_vec      = {1, 2, 3, 4, 5};
        ts_out.std_list     = {"1", "12", "123", "1234", "12345"};
        ts_out.std_map      = {{1, 1}, {2, 2}, {3, 3}, {4, 4}, {5, 5}};
        ts_out.std_mmap     = {{1, 1}, {2, 2}, {2, 22}, {3, 3}, {3, 33}, {4, 4}, {5, 5}};
        ts_out.std_umap     = {{"1", 1}, {"12", 2}, {"123", 3}, {"1234", 4}, {"12345", 5}};
        ts_out.std_array    = {1, 2, 3, 4, 5};
        ts_out.std_deque    = {1, 2, 3, 4, 5};
        ts_out.std_set      = {1, 2, 3, 4, 5};
        ts_out.std_multiset = {"1", "12", "123", "1234", "12345"};
        ts_out.std_uset     = {1, 2, 3, 4, 5};
    }

    void TearDown() override {
    }

    test_struct ts_out;
};

TEST_F(reflect_test, test_reflect_struct) {
    auto msg_out = mc::dbus::message::new_method_call(
        "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "Hello");
    msg_out.set_serial(123456);
    EXPECT_TRUE(msg_out.is_valid());

    // 序列化消息
    msg_out.writer() << ts_out;
    auto [data, len] = msg_out.marshal();
    EXPECT_NE(data, nullptr);
    EXPECT_GT(len, 0);

    // 反序列化消息
    test_struct       ts_in;
    mc::dbus::message msg_in;
    mc::dbus::error   err;
    EXPECT_TRUE(msg_in.demarshal(data.get(), len, err));

    EXPECT_EQ(msg_in.get_type(), message_type::method_call);
    EXPECT_EQ(msg_in.get_path(), "/org/freedesktop/DBus");
    EXPECT_EQ(msg_in.get_interface(), "org.freedesktop.DBus");
    EXPECT_EQ(msg_in.get_member(), "Hello");
    EXPECT_EQ(msg_in.get_serial(), 123456);
    EXPECT_EQ(msg_in.get_signature(), get_signature<test_struct>());
    ASSERT_EQ(msg_in.has_signature(get_signature<test_struct>()), true);

    msg_in.reader() >> ts_in;

    EXPECT_EQ(ts_in.i8, 1);
    EXPECT_EQ(ts_in.u8, 2);
    EXPECT_EQ(ts_in.i16, 3);
    EXPECT_EQ(ts_in.u16, 4);
    EXPECT_EQ(ts_in.i32, 5);
    EXPECT_EQ(ts_in.u32, 6);
    EXPECT_EQ(ts_in.i64, 7);
    EXPECT_EQ(ts_in.u64, 8);
    EXPECT_EQ(ts_in.f, 9.0f);
    EXPECT_EQ(ts_in.d, 10.0);
    EXPECT_EQ(ts_in.str, "Hello");
    EXPECT_EQ(ts_in.b, true);
    EXPECT_EQ(ts_in.p, "/org/freedesktop/DBus");
    EXPECT_EQ(ts_in.sig, "s");
    EXPECT_EQ(ts_in.v, "world");
    EXPECT_EQ(ts_in.a, ts_out.a);
    EXPECT_EQ(ts_in.m, ts_out.m);
    EXPECT_EQ(ts_in.std_o, 1);
    EXPECT_EQ(ts_in.std_p, ts_out.std_p);
    EXPECT_EQ(ts_in.std_t, ts_out.std_t);
    EXPECT_EQ(ts_in.std_vec, ts_out.std_vec);
    EXPECT_EQ(ts_in.std_list, ts_out.std_list);
    EXPECT_EQ(ts_in.std_map, ts_out.std_map);
    EXPECT_EQ(ts_in.std_mmap, ts_out.std_mmap);
    EXPECT_EQ(ts_in.std_umap, ts_out.std_umap);
    EXPECT_EQ(ts_in.std_array, ts_out.std_array);
    EXPECT_EQ(ts_in.std_deque, ts_out.std_deque);
    EXPECT_EQ(ts_in.std_set, ts_out.std_set);
    EXPECT_EQ(ts_in.std_multiset, ts_out.std_multiset);
    EXPECT_EQ(ts_in.std_uset, ts_out.std_uset);
}
