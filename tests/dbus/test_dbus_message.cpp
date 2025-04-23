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

#include <dbus/dbus.h>
#include <gtest/gtest.h>
#include <mc/dbus/error.h>
#include <mc/dbus/message.h>
#include <mc/dbus/path.h>
#include <mc/dbus/signature.h>
#include <mc/variant.h>

using namespace mc::dbus;

class dbus_message_test : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }
};

TEST_F(dbus_message_test, test_writer) {
    mc::dbus::message msg = message::new_method_call("org.example.Test", "/org/example/Test",
                                                     "org.example.Test", "TestVariantNesting");
    msg.set_sender("org.example.Test");
    msg.set_serial(1);

    // 写入各种类型的数据
    uint8_t     u8  = 42;
    uint16_t    u16 = 4242;
    uint32_t    u32 = 424242;
    uint64_t    u64 = 42424242;
    double      d   = 42.42;
    std::string str = "从stream写入的字符串";
    bool        b   = true;

    auto writer = msg.writer();

    // 写入基本类型
    writer << u8 << u16 << u32 << u64 << d << str << b;

    // 写入一个数组
    mc::variants arr = {1, 2, 3, 4, 5};
    writer << arr;

    // 写入字典
    mc::dict dict = {{"key1", 100}, {"key2", "value2"}, {"key3", true}};
    writer << dict;

    // 写入标准库类型
    std::optional<int>                 std_o    = 1;                                        // ai
    std::pair<int, std::string>        std_p    = {1, "12"};                                // (is)
    std::tuple<int, std::string, bool> std_t    = {1, "12", true};                          // (isb)
    std::vector<int>                   std_vec  = {1, 2, 3, 4, 5};                          // a(i)
    std::list<std::string>             std_list = {"1", "12", "123", "1234", "12345"};      // a(s)
    std::map<int, int>                 std_map  = {{1, 1}, {2, 2}, {3, 3}, {4, 4}, {5, 5}}; // a{ii}
    std::unordered_map<std::string, int> std_umap = {
        {"1", 1}, {"12", 2}, {"123", 3}, {"1234", 4}, {"12345", 5}}; // a{si}
    writer << std_o << std_p << std_t << std_vec << std_list << std_map << std_umap;

    auto [data, size] = msg.marshal();

    message msg2;
    error   err;
    msg2.demarshal(data.get(), size, err);
    EXPECT_FALSE(err.is_set());
    EXPECT_EQ(msg2.get_sender(), "org.example.Test");
    EXPECT_EQ(msg2.get_serial(), 1);
    EXPECT_EQ(msg2.get_path(), "/org/example/Test");
    EXPECT_EQ(msg2.get_interface(), "org.example.Test");
    EXPECT_EQ(msg2.get_member(), "TestVariantNesting");

    auto        reader = msg2.reader();
    uint8_t     u8_2;
    uint16_t    u16_2;
    uint32_t    u32_2;
    uint64_t    u64_2;
    double      d_2;
    std::string str_2;
    bool        b_2;

    reader >> u8_2 >> u16_2 >> u32_2 >> u64_2 >> d_2 >> str_2 >> b_2;
    EXPECT_EQ(u8_2, u8);
    EXPECT_EQ(u16_2, u16);
    EXPECT_EQ(u32_2, u32);
    EXPECT_EQ(u64_2, u64);
    EXPECT_EQ(d_2, d);
    EXPECT_EQ(str_2, str);
    EXPECT_EQ(b_2, b);

    mc::variants arr_2;
    reader >> arr_2;
    EXPECT_EQ(arr_2, arr);

    mc::dict dict_2;
    reader >> dict_2;
    EXPECT_EQ(dict_2, dict);

    std::optional<int>                   std_o_2;
    std::pair<int, std::string>          std_p_2;
    std::tuple<int, std::string, bool>   std_t_2;
    std::vector<int>                     std_vec_2;
    std::list<std::string>               std_list_2;
    std::map<int, int>                   std_map_2;
    std::unordered_map<std::string, int> std_umap_2;

    reader >> std_o_2 >> std_p_2 >> std_t_2;
    reader >> std_vec_2 >> std_list_2 >> std_map_2 >> std_umap_2;
    EXPECT_EQ(std_o_2, std_o);
    EXPECT_EQ(std_p_2, std_p);
    EXPECT_EQ(std_t_2, std_t);
    EXPECT_EQ(std_vec_2, std_vec);
    EXPECT_EQ(std_list_2, std_list);
    EXPECT_EQ(std_map_2, std_map);
    EXPECT_EQ(std_umap_2, std_umap);
}
