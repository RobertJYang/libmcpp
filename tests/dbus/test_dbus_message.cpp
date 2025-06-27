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
#include <mc/variant.h>
#include <test_utilities/test_base.h>

using namespace mc::dbus;

// 测试数据
struct test_data {
    uint8_t     m_u8;
    uint16_t    m_u16;
    uint32_t    m_u32;
    uint64_t    m_u64;
    double      m_d;
    std::string m_str;
    bool        m_b;

    // mc 的复杂数据结构
    mc::variant  m_v;
    mc::variants m_arr;
    mc::dict     m_dict;

    // C++ 标准库数据类型
    std::optional<int32_t>                   m_std_o;
    std::pair<int32_t, std::string>          m_std_p;
    std::tuple<int32_t, std::string, bool>   m_std_t;
    std::vector<int32_t>                     m_std_vec;
    std::list<std::string>                   m_std_list;
    std::map<int32_t, int32_t>               m_std_map;
    std::multimap<int32_t, int32_t>          m_std_mmap;
    std::unordered_map<std::string, int32_t> m_std_umap;
    std::set<int32_t>                        m_std_set;
    std::multiset<std::string>               m_std_multiset;
    std::unordered_set<int32_t>              m_std_uset;
    std::deque<int32_t>                      m_std_deque;
    std::array<int32_t, 5>                   m_std_array;
};

static void prepare_test_data(test_data& data) {
    // 一些基础数据类型
    data.m_u8  = 42;
    data.m_u16 = 4242;
    data.m_u32 = 424242;
    data.m_u64 = 42424242;
    data.m_d   = 42.42;
    data.m_str = "从stream写入的字符串";
    data.m_b   = true;

    // mc 的复杂数据结构
    data.m_v    = "hello";                                             // vs
    data.m_arr  = {1, 2, 3, 4, 5};                                     // a(i)
    data.m_dict = {{"key1", 100}, {"key2", "value2"}, {"key3", true}}; // a{sv}

    // C++ 标准库数据类型
    data.m_std_o        = 1;                                                            // ai
    data.m_std_p        = {1, "12"};                                                    // (is)
    data.m_std_t        = {1, "12", true};                                              // (isb)
    data.m_std_vec      = {1, 2, 3, 4, 5};                                              // a(i)
    data.m_std_list     = {"1", "12", "123", "1234", "12345"};                          // a(s)
    data.m_std_map      = {{1, 1}, {2, 2}, {3, 3}, {4, 4}, {5, 5}};                     // a{ii}
    data.m_std_mmap     = {{1, 1}, {2, 2}, {2, 22}, {3, 3}, {2, 33}, {4, 4}, {5, 5}};   // a{ii}
    data.m_std_umap     = {{"1", 1}, {"12", 2}, {"123", 3}, {"1234", 4}, {"12345", 5}}; // a{si}
    data.m_std_set      = {1, 2, 3, 4, 5};                                              // a(i)
    data.m_std_multiset = {"1", "12", "12", "123", "123", "1234", "12345"};             // a(s)
    data.m_std_uset     = {1, 2, 3, 4, 5};                                              // a(i)
    data.m_std_deque    = {1, 2, 3, 4, 5};                                              // a(i)
    data.m_std_array    = {1, 2, 3, 4, 5};                                              // a(i)
}

static void verify_data(test_data& data, test_data& other) {
    // 验证基本类型
    EXPECT_EQ(other.m_u8, data.m_u8);
    EXPECT_EQ(other.m_u16, data.m_u16);
    EXPECT_EQ(other.m_u32, data.m_u32);
    EXPECT_EQ(other.m_u64, data.m_u64);
    EXPECT_EQ(other.m_d, data.m_d);
    EXPECT_EQ(other.m_str, data.m_str);
    EXPECT_EQ(other.m_b, data.m_b);

    // 验证 mc 复杂数据结构
    EXPECT_EQ(other.m_v, data.m_v);
    EXPECT_EQ(other.m_arr, data.m_arr);
    EXPECT_EQ(other.m_dict, data.m_dict);

    // 验证标准库数据类型
    EXPECT_EQ(other.m_std_o, data.m_std_o);
    EXPECT_EQ(other.m_std_p, data.m_std_p);
    EXPECT_EQ(other.m_std_t, data.m_std_t);
    EXPECT_EQ(other.m_std_vec, data.m_std_vec);
    EXPECT_EQ(other.m_std_list, data.m_std_list);
    EXPECT_EQ(other.m_std_map, data.m_std_map);
    EXPECT_EQ(other.m_std_umap, data.m_std_umap);
    EXPECT_EQ(other.m_std_set, data.m_std_set);
    EXPECT_EQ(other.m_std_multiset, data.m_std_multiset);
    EXPECT_EQ(other.m_std_deque, data.m_std_deque);
    EXPECT_EQ(other.m_std_array, data.m_std_array);
}
class dbus_message_test : public mc::test::TestBase {
protected:
    void SetUp() override {
        prepare_test_data(data);
    }

    void TearDown() override {
    }

    test_data data;
};

// 测试使用 mc::dbus::message 的 reader 和 writer 流接口读写数据
TEST_F(dbus_message_test, test_message_reader_writer) {
    mc::dbus::message msg = message::new_method_call("org.example.Test", "/org/example/Test",
                                                     "org.example.Test", "TestVariantNesting");
    msg.set_sender("org.example.Test");
    msg.set_serial(1);

    auto writer = msg.writer();

    writer << data.m_u8 << data.m_u16 << data.m_u32 << data.m_u64 << data.m_d << data.m_str
           << data.m_b;
    writer << data.m_v << data.m_arr << data.m_dict;
    writer << data.m_std_o << data.m_std_p << data.m_std_t << data.m_std_vec << data.m_std_list
           << data.m_std_map << data.m_std_mmap << data.m_std_umap << data.m_std_set
           << data.m_std_multiset << data.m_std_uset << data.m_std_deque << data.m_std_array;

    // 将消息系列化成二进制数据
    auto [data_ptr, size] = msg.marshal();

    // 重新将二进制数据反系列化成 mc::dbus::message 对象
    mc::dbus::message msg2;
    error             err;
    msg2.demarshal(data_ptr.get(), size, err);

    // 验证消息头
    EXPECT_FALSE(err.is_set());
    EXPECT_EQ(msg2.get_sender(), "org.example.Test");
    EXPECT_EQ(msg2.get_serial(), 1);
    EXPECT_EQ(msg2.get_path(), "/org/example/Test");
    EXPECT_EQ(msg2.get_interface(), "org.example.Test");
    EXPECT_EQ(msg2.get_member(), "TestVariantNesting");
    EXPECT_EQ(msg2.get_signature(), msg.get_signature());

    test_data data2;
    auto      reader = msg2.reader();
    reader >> data2.m_u8 >> data2.m_u16 >> data2.m_u32 >> data2.m_u64 >> data2.m_d >> data2.m_str >>
        data2.m_b;
    reader >> data2.m_v >> data2.m_arr >> data2.m_dict;
    reader >> data2.m_std_o >> data2.m_std_p >> data2.m_std_t >> data2.m_std_vec >>
        data2.m_std_list >> data2.m_std_map >> data2.m_std_mmap >> data2.m_std_umap >>
        data2.m_std_set >> data2.m_std_multiset >> data2.m_std_uset >> data2.m_std_deque >>
        data2.m_std_array;

    verify_data(data, data2);
}

// 使用原始 libdbus 接口创建消息，使用 mc::dbus::message 读取数据并验证结果
TEST_F(dbus_message_test, test_libdbus_write_mc_dbus_message_read) {
    DBusMessage* msg = dbus_message_new_method_call("org.example.Test", "/org/example/Test",
                                                    "org.example.Test", "TestVariantNesting");
    ASSERT_TRUE(msg != nullptr);

    dbus_message_set_sender(msg, "org.example.Test");
    dbus_message_set_serial(msg, 1);

    DBusMessageIter iter;
    dbus_message_iter_init_append(msg, &iter);

    // 写入基本类型
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_BYTE, &data.m_u8);
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_UINT16, &data.m_u16);
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_UINT32, &data.m_u32);
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_UINT64, &data.m_u64);
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_DOUBLE, &data.m_d);
    const char* str_val = data.m_str.c_str();
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &str_val);
    dbus_bool_t b = data.m_b;
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_BOOLEAN, &b);

    // 写入 variant
    DBusMessageIter var_iter;
    const char*     var_str = data.m_v.get_string().c_str();
    dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "s", &var_iter);
    dbus_message_iter_append_basic(&var_iter, DBUS_TYPE_STRING, &var_str);
    dbus_message_iter_close_container(&iter, &var_iter);

    // 写入数组 (av)
    DBusMessageIter array_iter;
    dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "v", &array_iter);
    for (const auto& val : data.m_arr) {
        DBusMessageIter v_iter;
        dbus_message_iter_open_container(&array_iter, DBUS_TYPE_VARIANT, "i", &v_iter);
        int i_val = val.as<int>();
        dbus_message_iter_append_basic(&v_iter, DBUS_TYPE_INT32, &i_val);
        dbus_message_iter_close_container(&array_iter, &v_iter);
    }
    dbus_message_iter_close_container(&iter, &array_iter);

    // 写入字典
    DBusMessageIter dict_iter, entry_iter;
    dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sv}", &dict_iter);
    for (const auto& entry : data.m_dict) {
        dbus_message_iter_open_container(&dict_iter, DBUS_TYPE_DICT_ENTRY, nullptr, &entry_iter);
        const char* k = entry.key.get_string().c_str();
        dbus_message_iter_append_basic(&entry_iter, DBUS_TYPE_STRING, &k);

        // 根据值类型写入variant
        if (entry.value.get_type() == mc::type_id::int32_type) {
            DBusMessageIter var_iter;
            int             i_val = entry.value.as<int>();
            dbus_message_iter_open_container(&entry_iter, DBUS_TYPE_VARIANT, "i", &var_iter);
            dbus_message_iter_append_basic(&var_iter, DBUS_TYPE_INT32, &i_val);
            dbus_message_iter_close_container(&entry_iter, &var_iter);
        } else if (entry.value.get_type() == mc::type_id::string_type) {
            DBusMessageIter var_iter;
            const char*     s_val = entry.value.get_string().c_str();
            dbus_message_iter_open_container(&entry_iter, DBUS_TYPE_VARIANT, "s", &var_iter);
            dbus_message_iter_append_basic(&var_iter, DBUS_TYPE_STRING, &s_val);
            dbus_message_iter_close_container(&entry_iter, &var_iter);
        } else if (entry.value.get_type() == mc::type_id::bool_type) {
            DBusMessageIter var_iter;
            dbus_bool_t     b_val = entry.value.as<bool>();
            dbus_message_iter_open_container(&entry_iter, DBUS_TYPE_VARIANT, "b", &var_iter);
            dbus_message_iter_append_basic(&var_iter, DBUS_TYPE_BOOLEAN, &b_val);
            dbus_message_iter_close_container(&entry_iter, &var_iter);
        }

        dbus_message_iter_close_container(&dict_iter, &entry_iter);
    }
    dbus_message_iter_close_container(&iter, &dict_iter);

    // 写入标准库数据类型
    DBusMessageIter std_o_iter;
    dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "i", &std_o_iter);
    dbus_message_iter_append_basic(&std_o_iter, DBUS_TYPE_INT32, &data.m_std_o.value());
    dbus_message_iter_close_container(&iter, &std_o_iter);

    DBusMessageIter std_p_iter;
    dbus_message_iter_open_container(&iter, DBUS_TYPE_STRUCT, nullptr, &std_p_iter);
    dbus_message_iter_append_basic(&std_p_iter, DBUS_TYPE_INT32, &data.m_std_p.first);
    const char* std_p_str = data.m_std_p.second.c_str();
    dbus_message_iter_append_basic(&std_p_iter, DBUS_TYPE_STRING, &std_p_str);
    dbus_message_iter_close_container(&iter, &std_p_iter);

    DBusMessageIter std_t_iter;
    dbus_message_iter_open_container(&iter, DBUS_TYPE_STRUCT, nullptr, &std_t_iter);
    dbus_message_iter_append_basic(&std_t_iter, DBUS_TYPE_INT32, &std::get<0>(data.m_std_t));
    const char* std_t_str = std::get<1>(data.m_std_t).c_str();
    dbus_message_iter_append_basic(&std_t_iter, DBUS_TYPE_STRING, &std_t_str);
    dbus_bool_t std_t_b = std::get<2>(data.m_std_t);
    dbus_message_iter_append_basic(&std_t_iter, DBUS_TYPE_BOOLEAN, &std_t_b);
    dbus_message_iter_close_container(&iter, &std_t_iter);

    DBusMessageIter std_vec_iter;
    dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "i", &std_vec_iter);
    const int* std_vec_data = data.m_std_vec.data();
    int        std_vec_size = data.m_std_vec.size();
    dbus_message_iter_append_fixed_array(&std_vec_iter, DBUS_TYPE_INT32, &std_vec_data,
                                         std_vec_size);
    dbus_message_iter_close_container(&iter, &std_vec_iter);

    DBusMessageIter std_list_iter;
    dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "s", &std_list_iter);
    for (const auto& val : data.m_std_list) {
        const char* s_val = val.c_str();
        dbus_message_iter_append_basic(&std_list_iter, DBUS_TYPE_STRING, &s_val);
    }
    dbus_message_iter_close_container(&iter, &std_list_iter);

    DBusMessageIter std_map_iter;
    dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{ii}", &std_map_iter);
    for (const auto& [key, val] : data.m_std_map) {
        DBusMessageIter entry_iter;
        dbus_message_iter_open_container(&std_map_iter, DBUS_TYPE_DICT_ENTRY, nullptr, &entry_iter);
        dbus_message_iter_append_basic(&entry_iter, DBUS_TYPE_INT32, &key);
        dbus_message_iter_append_basic(&entry_iter, DBUS_TYPE_INT32, &val);
        dbus_message_iter_close_container(&std_map_iter, &entry_iter);
    }
    dbus_message_iter_close_container(&iter, &std_map_iter);

    DBusMessageIter std_mmap_iter;
    dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{ii}", &std_mmap_iter);
    for (const auto& [key, val] : data.m_std_mmap) {
        DBusMessageIter entry_iter;
        dbus_message_iter_open_container(&std_mmap_iter, DBUS_TYPE_DICT_ENTRY, nullptr,
                                         &entry_iter);
        dbus_message_iter_append_basic(&entry_iter, DBUS_TYPE_INT32, &key);
        dbus_message_iter_append_basic(&entry_iter, DBUS_TYPE_INT32, &val);
        dbus_message_iter_close_container(&std_mmap_iter, &entry_iter);
    }
    dbus_message_iter_close_container(&iter, &std_mmap_iter);

    DBusMessageIter std_umap_iter;
    dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{si}", &std_umap_iter);
    for (const auto& [key, val] : data.m_std_umap) {
        DBusMessageIter entry_iter;
        dbus_message_iter_open_container(&std_umap_iter, DBUS_TYPE_DICT_ENTRY, nullptr,
                                         &entry_iter);
        const char* k = key.c_str();
        dbus_message_iter_append_basic(&entry_iter, DBUS_TYPE_STRING, &k);
        dbus_message_iter_append_basic(&entry_iter, DBUS_TYPE_INT32, &val);
        dbus_message_iter_close_container(&std_umap_iter, &entry_iter);
    }
    dbus_message_iter_close_container(&iter, &std_umap_iter);

    DBusMessageIter std_set_iter;
    dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "i", &std_set_iter);
    for (const auto& val : data.m_std_set) {
        dbus_message_iter_append_basic(&std_set_iter, DBUS_TYPE_INT32, &val);
    }
    dbus_message_iter_close_container(&iter, &std_set_iter);

    DBusMessageIter std_multiset_iter;
    dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "s", &std_multiset_iter);
    for (const auto& val : data.m_std_multiset) {
        const char* s_val = val.c_str();
        dbus_message_iter_append_basic(&std_multiset_iter, DBUS_TYPE_STRING, &s_val);
    }
    dbus_message_iter_close_container(&iter, &std_multiset_iter);

    DBusMessageIter std_uset_iter;
    dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "i", &std_uset_iter);
    for (const auto& val : data.m_std_uset) {
        dbus_message_iter_append_basic(&std_uset_iter, DBUS_TYPE_INT32, &val);
    }
    dbus_message_iter_close_container(&iter, &std_uset_iter);

    DBusMessageIter std_deque_iter;
    dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "i", &std_deque_iter);
    for (const auto& val : data.m_std_deque) {
        dbus_message_iter_append_basic(&std_deque_iter, DBUS_TYPE_INT32, &val);
    }
    dbus_message_iter_close_container(&iter, &std_deque_iter);

    DBusMessageIter std_array_iter;
    dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "i", &std_array_iter);
    const int* std_array_data = data.m_std_array.data();
    dbus_message_iter_append_fixed_array(&std_array_iter, DBUS_TYPE_INT32, &std_array_data,
                                         data.m_std_array.size());
    dbus_message_iter_close_container(&iter, &std_array_iter);

    // 获取消息数据
    int   size     = 0;
    char* data_ptr = nullptr;
    dbus_message_marshal(msg, &data_ptr, &size);
    ASSERT_TRUE(data_ptr != nullptr);

    // 使用 mc::dbus::message 读取数据
    mc::dbus::message mc_msg;
    error             err;
    mc_msg.demarshal(reinterpret_cast<const char*>(data_ptr), size, err);
    EXPECT_FALSE(err.is_set());

    // 验证消息头
    EXPECT_EQ(mc_msg.get_sender(), "org.example.Test");
    EXPECT_EQ(mc_msg.get_serial(), 1);
    EXPECT_EQ(mc_msg.get_path(), "/org/example/Test");
    EXPECT_EQ(mc_msg.get_interface(), "org.example.Test");
    EXPECT_EQ(mc_msg.get_member(), "TestVariantNesting");
    EXPECT_EQ(mc_msg.get_signature(), dbus_message_get_signature(msg));

    test_data data2;
    mc_msg.reader() >> data2.m_u8 >> data2.m_u16 >> data2.m_u32 >> data2.m_u64 >> data2.m_d >>
        data2.m_str >> data2.m_b >> data2.m_v >> data2.m_arr >> data2.m_dict >> data2.m_std_o >>
        data2.m_std_p >> data2.m_std_t >> data2.m_std_vec >> data2.m_std_list >> data2.m_std_map >>
        data2.m_std_mmap >> data2.m_std_umap >> data2.m_std_set >> data2.m_std_multiset >>
        data2.m_std_uset >> data2.m_std_deque >> data2.m_std_array;

    verify_data(data, data2);
    dbus_message_unref(msg);
}

// 使用 mc::dbus::message 写入数据，用原生 libdbus 接口读取数据并验证结果
TEST_F(dbus_message_test, test_mc_dbus_message_write_libdbus_read) {
    // 使用 mc::dbus::message 创建消息并写入数据
    mc::dbus::message msg = message::new_method_call("org.example.Test", "/org/example/Test",
                                                     "org.example.Test", "TestVariantNesting");
    msg.set_sender("org.example.Test");
    msg.set_serial(1);

    msg.writer() << data.m_u8 << data.m_u16 << data.m_u32 << data.m_u64 << data.m_d << data.m_str
                 << data.m_b << data.m_v << data.m_arr << data.m_dict << data.m_std_o
                 << data.m_std_p << data.m_std_t << data.m_std_vec << data.m_std_list
                 << data.m_std_map << data.m_std_mmap << data.m_std_umap << data.m_std_set
                 << data.m_std_multiset << data.m_std_uset << data.m_std_deque << data.m_std_array;

    auto [data_ptr, size] = msg.marshal();

    // 使用原生 libdbus 接口读取数据
    DBusError dbus_err;
    dbus_error_init(&dbus_err);

    DBusMessage* dbus_msg = dbus_message_demarshal(data_ptr.get(), size, &dbus_err);
    ASSERT_FALSE(dbus_error_is_set(&dbus_err));
    ASSERT_TRUE(dbus_msg != nullptr);

    // 验证消息头
    EXPECT_STREQ(dbus_message_get_sender(dbus_msg), "org.example.Test");
    EXPECT_EQ(dbus_message_get_serial(dbus_msg), 1);
    EXPECT_STREQ(dbus_message_get_path(dbus_msg), "/org/example/Test");
    EXPECT_STREQ(dbus_message_get_interface(dbus_msg), "org.example.Test");
    EXPECT_STREQ(dbus_message_get_member(dbus_msg), "TestVariantNesting");
    EXPECT_STREQ(dbus_message_get_signature(dbus_msg), msg.get_signature().data());

    // 从原生 libdbus 消息读取数据
    DBusMessageIter iter;
    dbus_bool_t     init_result = dbus_message_iter_init(dbus_msg, &iter);
    ASSERT_TRUE(init_result);

    // 准备一个新的 test_data 结构来存储读取的结果
    test_data data2 = {};

    // 读取基本类型
    ASSERT_EQ(dbus_message_iter_get_arg_type(&iter), DBUS_TYPE_BYTE);
    dbus_message_iter_get_basic(&iter, &data2.m_u8);
    dbus_message_iter_next(&iter);

    ASSERT_EQ(dbus_message_iter_get_arg_type(&iter), DBUS_TYPE_UINT16);
    dbus_message_iter_get_basic(&iter, &data2.m_u16);
    dbus_message_iter_next(&iter);

    ASSERT_EQ(dbus_message_iter_get_arg_type(&iter), DBUS_TYPE_UINT32);
    dbus_message_iter_get_basic(&iter, &data2.m_u32);
    dbus_message_iter_next(&iter);

    ASSERT_EQ(dbus_message_iter_get_arg_type(&iter), DBUS_TYPE_UINT64);
    dbus_message_iter_get_basic(&iter, &data2.m_u64);
    dbus_message_iter_next(&iter);

    ASSERT_EQ(dbus_message_iter_get_arg_type(&iter), DBUS_TYPE_DOUBLE);
    dbus_message_iter_get_basic(&iter, &data2.m_d);
    dbus_message_iter_next(&iter);

    const char* str_val = nullptr;
    ASSERT_EQ(dbus_message_iter_get_arg_type(&iter), DBUS_TYPE_STRING);
    dbus_message_iter_get_basic(&iter, &str_val);
    ASSERT_NE(str_val, nullptr);
    data2.m_str = str_val;
    dbus_message_iter_next(&iter);

    dbus_bool_t b_val = FALSE;
    ASSERT_EQ(dbus_message_iter_get_arg_type(&iter), DBUS_TYPE_BOOLEAN);
    dbus_message_iter_get_basic(&iter, &b_val);
    data2.m_b = b_val;
    dbus_message_iter_next(&iter);

    // 读取 variant
    ASSERT_EQ(dbus_message_iter_get_arg_type(&iter), DBUS_TYPE_VARIANT);
    DBusMessageIter var_iter;
    dbus_message_iter_recurse(&iter, &var_iter);
    const char* v_str = nullptr;
    ASSERT_EQ(dbus_message_iter_get_arg_type(&var_iter), DBUS_TYPE_STRING);
    dbus_message_iter_get_basic(&var_iter, &v_str);
    ASSERT_NE(v_str, nullptr);
    data2.m_v = v_str;
    dbus_message_iter_next(&iter);

    // 读取数组
    ASSERT_EQ(dbus_message_iter_get_arg_type(&iter), DBUS_TYPE_ARRAY);
    DBusMessageIter array_iter;
    dbus_message_iter_recurse(&iter, &array_iter);
    while (dbus_message_iter_get_arg_type(&array_iter) != DBUS_TYPE_INVALID) {
        mc::variant val;
        int         arg_type = dbus_message_iter_get_arg_type(&array_iter);
        ASSERT_EQ(arg_type, DBUS_TYPE_VARIANT);
        DBusMessageIter v_iter;
        dbus_message_iter_recurse(&array_iter, &v_iter);
        ASSERT_EQ(dbus_message_iter_get_arg_type(&v_iter), DBUS_TYPE_INT32);
        int32_t i_val = 0;
        dbus_message_iter_get_basic(&v_iter, &i_val);
        val = i_val;
        data2.m_arr.push_back(val);
        dbus_message_iter_next(&array_iter);
    }
    dbus_message_iter_next(&iter);

    // 读取字典
    mc::mutable_dict dict;
    ASSERT_EQ(dbus_message_iter_get_arg_type(&iter), DBUS_TYPE_ARRAY);
    DBusMessageIter dict_iter;
    dbus_message_iter_recurse(&iter, &dict_iter);
    while (dbus_message_iter_get_arg_type(&dict_iter) != DBUS_TYPE_INVALID) {
        DBusMessageIter entry_iter;
        dbus_message_iter_recurse(&dict_iter, &entry_iter);

        // 读取键
        const char* key = nullptr;
        ASSERT_EQ(dbus_message_iter_get_arg_type(&entry_iter), DBUS_TYPE_STRING);
        dbus_message_iter_get_basic(&entry_iter, &key);
        ASSERT_NE(key, nullptr);
        dbus_message_iter_next(&entry_iter);

        // 读取值
        ASSERT_EQ(dbus_message_iter_get_arg_type(&entry_iter), DBUS_TYPE_VARIANT);
        DBusMessageIter value_iter;
        dbus_message_iter_recurse(&entry_iter, &value_iter);

        mc::variant value;
        int         value_type = dbus_message_iter_get_arg_type(&value_iter);
        if (value_type == DBUS_TYPE_INT32) {
            int32_t i_val = 0;
            dbus_message_iter_get_basic(&value_iter, &i_val);
            value = i_val;
        } else if (value_type == DBUS_TYPE_STRING) {
            const char* s_val = nullptr;
            dbus_message_iter_get_basic(&value_iter, &s_val);
            ASSERT_NE(s_val, nullptr);
            value = s_val;
        } else if (value_type == DBUS_TYPE_BOOLEAN) {
            dbus_bool_t b_val = FALSE;
            dbus_message_iter_get_basic(&value_iter, &b_val);
            value = (bool)b_val;
        }

        dict[key] = value;
        dbus_message_iter_next(&dict_iter);
    }
    data2.m_dict = std::move(dict);
    dbus_message_iter_next(&iter);

    // 读取 std::optional<int32_t>
    ASSERT_EQ(dbus_message_iter_get_arg_type(&iter), DBUS_TYPE_ARRAY);
    DBusMessageIter opt_iter;
    dbus_message_iter_recurse(&iter, &opt_iter);
    ASSERT_EQ(dbus_message_iter_get_arg_type(&opt_iter), DBUS_TYPE_INT32);
    int32_t opt_val = 0;
    dbus_message_iter_get_basic(&opt_iter, &opt_val);
    data2.m_std_o = opt_val;
    dbus_message_iter_next(&iter);

    // 读取 std::pair<int32_t, std::string>
    ASSERT_EQ(dbus_message_iter_get_arg_type(&iter), DBUS_TYPE_STRUCT);
    DBusMessageIter pair_iter;
    dbus_message_iter_recurse(&iter, &pair_iter);
    int32_t pair_first = 0;
    ASSERT_EQ(dbus_message_iter_get_arg_type(&pair_iter), DBUS_TYPE_INT32);
    dbus_message_iter_get_basic(&pair_iter, &pair_first);
    dbus_message_iter_next(&pair_iter);
    const char* pair_second = nullptr;
    ASSERT_EQ(dbus_message_iter_get_arg_type(&pair_iter), DBUS_TYPE_STRING);
    dbus_message_iter_get_basic(&pair_iter, &pair_second);
    ASSERT_NE(pair_second, nullptr);
    data2.m_std_p = std::make_pair(pair_first, pair_second);
    dbus_message_iter_next(&iter);

    // 读取 std::tuple<int32_t, std::string, bool>
    ASSERT_EQ(dbus_message_iter_get_arg_type(&iter), DBUS_TYPE_STRUCT);
    DBusMessageIter tuple_iter;
    dbus_message_iter_recurse(&iter, &tuple_iter);
    int32_t tuple_first = 0;
    ASSERT_EQ(dbus_message_iter_get_arg_type(&tuple_iter), DBUS_TYPE_INT32);
    dbus_message_iter_get_basic(&tuple_iter, &tuple_first);
    dbus_message_iter_next(&tuple_iter);
    const char* tuple_second = nullptr;
    ASSERT_EQ(dbus_message_iter_get_arg_type(&tuple_iter), DBUS_TYPE_STRING);
    dbus_message_iter_get_basic(&tuple_iter, &tuple_second);
    dbus_message_iter_next(&tuple_iter);
    dbus_bool_t tuple_third = FALSE;
    ASSERT_EQ(dbus_message_iter_get_arg_type(&tuple_iter), DBUS_TYPE_BOOLEAN);
    dbus_message_iter_get_basic(&tuple_iter, &tuple_third);
    data2.m_std_t =
        std::make_tuple(tuple_first, tuple_second ? tuple_second : "", (bool)tuple_third);
    dbus_message_iter_next(&iter);

    // 读取 std::vector<int32_t>
    ASSERT_EQ(dbus_message_iter_get_arg_type(&iter), DBUS_TYPE_ARRAY);
    DBusMessageIter vec_iter;
    dbus_message_iter_recurse(&iter, &vec_iter);
    ASSERT_EQ(dbus_message_iter_get_arg_type(&vec_iter), DBUS_TYPE_INT32);
    int         vec_count = dbus_message_iter_get_element_count(&iter);
    const char* vec_data  = nullptr;
    int         vec_len   = 0;
    dbus_message_iter_get_fixed_array(&vec_iter, &vec_data, &vec_len);
    ASSERT_NE(vec_data, nullptr);
    ASSERT_EQ(vec_len, vec_count);
    data2.m_std_vec.resize(vec_count);
    std::memcpy(data2.m_std_vec.data(), vec_data, vec_len * sizeof(int32_t));
    dbus_message_iter_next(&iter);

    // 读取 std::list<std::string>
    ASSERT_EQ(dbus_message_iter_get_arg_type(&iter), DBUS_TYPE_ARRAY);
    DBusMessageIter list_iter;
    dbus_message_iter_recurse(&iter, &list_iter);
    while (dbus_message_iter_get_arg_type(&list_iter) != DBUS_TYPE_INVALID) {
        const char* s_val = nullptr;
        dbus_message_iter_get_basic(&list_iter, &s_val);
        ASSERT_NE(s_val, nullptr);
        data2.m_std_list.push_back(s_val);
        dbus_message_iter_next(&list_iter);
    }
    dbus_message_iter_next(&iter);

    // 读取 std::map<int32_t, int32_t>
    ASSERT_EQ(dbus_message_iter_get_arg_type(&iter), DBUS_TYPE_ARRAY);
    DBusMessageIter map_iter;
    dbus_message_iter_recurse(&iter, &map_iter);
    while (dbus_message_iter_get_arg_type(&map_iter) != DBUS_TYPE_INVALID) {
        DBusMessageIter map_entry_iter;
        dbus_message_iter_recurse(&map_iter, &map_entry_iter);

        int32_t key = 0;
        ASSERT_EQ(dbus_message_iter_get_arg_type(&map_entry_iter), DBUS_TYPE_INT32);
        dbus_message_iter_get_basic(&map_entry_iter, &key);
        dbus_message_iter_next(&map_entry_iter);

        int32_t val = 0;
        ASSERT_EQ(dbus_message_iter_get_arg_type(&map_entry_iter), DBUS_TYPE_INT32);
        dbus_message_iter_get_basic(&map_entry_iter, &val);

        data2.m_std_map[key] = val;
        dbus_message_iter_next(&map_iter);
    }
    dbus_message_iter_next(&iter);

    // 读取 std::multimap<int32_t, int32_t>
    ASSERT_EQ(dbus_message_iter_get_arg_type(&iter), DBUS_TYPE_ARRAY);
    DBusMessageIter mmap_iter;
    dbus_message_iter_recurse(&iter, &mmap_iter);
    while (dbus_message_iter_get_arg_type(&mmap_iter) != DBUS_TYPE_INVALID) {
        DBusMessageIter mmap_entry_iter;
        dbus_message_iter_recurse(&mmap_iter, &mmap_entry_iter);

        int32_t key = 0;
        ASSERT_EQ(dbus_message_iter_get_arg_type(&mmap_entry_iter), DBUS_TYPE_INT32);
        dbus_message_iter_get_basic(&mmap_entry_iter, &key);
        dbus_message_iter_next(&mmap_entry_iter);

        int32_t val = 0;
        ASSERT_EQ(dbus_message_iter_get_arg_type(&mmap_entry_iter), DBUS_TYPE_INT32);
        dbus_message_iter_get_basic(&mmap_entry_iter, &val);

        data2.m_std_mmap.insert(std::make_pair(key, val));
        dbus_message_iter_next(&mmap_iter);
    }
    dbus_message_iter_next(&iter);

    // 读取 std::unordered_map<std::string, int32_t>
    ASSERT_EQ(dbus_message_iter_get_arg_type(&iter), DBUS_TYPE_ARRAY);
    DBusMessageIter umap_iter;
    dbus_message_iter_recurse(&iter, &umap_iter);
    while (dbus_message_iter_get_arg_type(&umap_iter) != DBUS_TYPE_INVALID) {
        DBusMessageIter umap_entry_iter;
        dbus_message_iter_recurse(&umap_iter, &umap_entry_iter);

        const char* key = nullptr;
        ASSERT_EQ(dbus_message_iter_get_arg_type(&umap_entry_iter), DBUS_TYPE_STRING);
        dbus_message_iter_get_basic(&umap_entry_iter, &key);
        ASSERT_NE(key, nullptr);
        dbus_message_iter_next(&umap_entry_iter);

        int32_t val = 0;
        ASSERT_EQ(dbus_message_iter_get_arg_type(&umap_entry_iter), DBUS_TYPE_INT32);
        dbus_message_iter_get_basic(&umap_entry_iter, &val);

        data2.m_std_umap[key] = val;
        dbus_message_iter_next(&umap_iter);
    }
    dbus_message_iter_next(&iter);

    // 读取 std::set<int32_t>
    ASSERT_EQ(dbus_message_iter_get_arg_type(&iter), DBUS_TYPE_ARRAY);
    DBusMessageIter set_iter;
    dbus_message_iter_recurse(&iter, &set_iter);
    while (dbus_message_iter_get_arg_type(&set_iter) != DBUS_TYPE_INVALID) {
        int32_t val = 0;
        dbus_message_iter_get_basic(&set_iter, &val);
        data2.m_std_set.insert(val);
        dbus_message_iter_next(&set_iter);
    }
    dbus_message_iter_next(&iter);

    // 读取 std::multiset<std::string>
    ASSERT_EQ(dbus_message_iter_get_arg_type(&iter), DBUS_TYPE_ARRAY);
    DBusMessageIter multiset_iter;
    dbus_message_iter_recurse(&iter, &multiset_iter);
    while (dbus_message_iter_get_arg_type(&multiset_iter) != DBUS_TYPE_INVALID) {
        const char* s_val = nullptr;
        dbus_message_iter_get_basic(&multiset_iter, &s_val);
        ASSERT_NE(s_val, nullptr);
        data2.m_std_multiset.insert(s_val);
        dbus_message_iter_next(&multiset_iter);
    }
    dbus_message_iter_next(&iter);

    // 读取 std::unordered_set<int32_t>
    ASSERT_EQ(dbus_message_iter_get_arg_type(&iter), DBUS_TYPE_ARRAY);
    DBusMessageIter uset_iter;
    dbus_message_iter_recurse(&iter, &uset_iter);
    while (dbus_message_iter_get_arg_type(&uset_iter) != DBUS_TYPE_INVALID) {
        int32_t val = 0;
        dbus_message_iter_get_basic(&uset_iter, &val);
        data2.m_std_uset.insert(val);
        dbus_message_iter_next(&uset_iter);
    }
    dbus_message_iter_next(&iter);

    // 读取 std::deque<int32_t>
    ASSERT_EQ(dbus_message_iter_get_arg_type(&iter), DBUS_TYPE_ARRAY);
    DBusMessageIter deque_iter;
    dbus_message_iter_recurse(&iter, &deque_iter);
    while (dbus_message_iter_get_arg_type(&deque_iter) != DBUS_TYPE_INVALID) {
        int32_t val = 0;
        dbus_message_iter_get_basic(&deque_iter, &val);
        data2.m_std_deque.push_back(val);
        dbus_message_iter_next(&deque_iter);
    }
    dbus_message_iter_next(&iter);

    // 读取 std::array<int32_t, 5>
    ASSERT_EQ(dbus_message_iter_get_arg_type(&iter), DBUS_TYPE_ARRAY);
    DBusMessageIter array_iter2;
    dbus_message_iter_recurse(&iter, &array_iter2);
    int array_count = dbus_message_iter_get_element_count(&iter);
    ASSERT_EQ(array_count, data.m_std_array.size());
    ASSERT_EQ(dbus_message_iter_get_arg_type(&array_iter2), DBUS_TYPE_INT32);
    const char* array_data = nullptr;
    int         array_len  = 0;
    dbus_message_iter_get_fixed_array(&array_iter2, &array_data, &array_len);
    ASSERT_NE(array_data, nullptr);
    ASSERT_EQ(array_len, array_count);
    std::memcpy(data2.m_std_array.data(), array_data, array_len * sizeof(int32_t));

    // 验证读取的数据与原始数据一致
    verify_data(data, data2);

    // 释放资源
    dbus_message_unref(dbus_msg);
    dbus_error_free(&dbus_err);
}

TEST_F(dbus_message_test, test_empty_dict) {
    mc::dbus::message msg = message::new_method_call("org.example.Test", "/org/example/Test",
                                                     "org.example.Test", "TestVariantNesting");
    msg.set_sender("org.example.Test");
    msg.set_serial(1);

    auto writer = msg.writer();
    writer << mc::dict();

    auto [data_ptr, size] = msg.marshal();

    // 使用原生 libdbus 接口读取数据
    DBusError dbus_err;
    dbus_error_init(&dbus_err);

    DBusMessage* dbus_msg = dbus_message_demarshal(data_ptr.get(), size, &dbus_err);
    ASSERT_FALSE(dbus_error_is_set(&dbus_err));
    ASSERT_TRUE(dbus_msg != nullptr);

    EXPECT_STREQ(dbus_message_get_signature(dbus_msg), "a{sv}");

    dbus_message_unref(dbus_msg);
    dbus_error_free(&dbus_err);

    // 使用 mc::dbus::message 读取数据
    mc::dbus::message mc_msg;
    error             err;
    mc_msg.demarshal(data_ptr.get(), size, err);
    EXPECT_FALSE(err.is_set());

    auto args = mc_msg.read_args();
    EXPECT_EQ(args.size(), 1);
    EXPECT_EQ(args[0].get_type(), mc::type_id::object_type);
}

TEST_F(dbus_message_test, test_empty_array) {
    mc::dbus::message msg = message::new_method_call("org.example.Test", "/org/example/Test",
                                                     "org.example.Test", "TestVariantNesting");
    msg.set_sender("org.example.Test");
    msg.set_serial(1);

    auto writer = msg.writer();
    writer << std::vector<int32_t>();

    auto [data_ptr, size] = msg.marshal();

    // 使用原生 libdbus 接口读取数据
    DBusError dbus_err;
    dbus_error_init(&dbus_err);

    DBusMessage* dbus_msg = dbus_message_demarshal(data_ptr.get(), size, &dbus_err);
    ASSERT_FALSE(dbus_error_is_set(&dbus_err));
    ASSERT_TRUE(dbus_msg != nullptr);

    EXPECT_STREQ(dbus_message_get_signature(dbus_msg), "ai");

    dbus_message_unref(dbus_msg);
    dbus_error_free(&dbus_err);

    // 使用 mc::dbus::message 读取数据
    mc::dbus::message mc_msg;
    error             err;
    mc_msg.demarshal(data_ptr.get(), size, err);
    EXPECT_FALSE(err.is_set());

    auto args = mc_msg.read_args();
    EXPECT_EQ(args.size(), 1);
    EXPECT_EQ(args[0].get_type(), mc::type_id::array_type);
}

TEST_F(dbus_message_test, test_read_args) {
    mc::dbus::message msg = message::new_method_call("org.example.Test", "/org/example/Test",
                                                     "org.example.Test", "TestVariantNesting");
    msg.set_sender("org.example.Test");
    msg.set_serial(1);

    auto writer = msg.writer();
    writer << mc::dict{{"key", "value"}, {"key2", "value2"}} << "123" << 123 << true;

    auto [data_ptr, size] = msg.marshal();

    // 使用原生 libdbus 接口读取数据
    DBusError dbus_err;
    dbus_error_init(&dbus_err);

    DBusMessage* dbus_msg = dbus_message_demarshal(data_ptr.get(), size, &dbus_err);
    ASSERT_FALSE(dbus_error_is_set(&dbus_err));
    ASSERT_TRUE(dbus_msg != nullptr);

    EXPECT_STREQ(dbus_message_get_signature(dbus_msg), "a{sv}sib");

    dbus_message_unref(dbus_msg);
    dbus_error_free(&dbus_err);

    // 使用 mc::dbus::message 读取数据
    mc::dbus::message mc_msg;
    error             err;
    mc_msg.demarshal(data_ptr.get(), size, err);
    EXPECT_FALSE(err.is_set());

    auto args = mc_msg.read_args();
    EXPECT_EQ(args.size(), 4);
    EXPECT_EQ(args[0].get_type(), mc::type_id::object_type);
    EXPECT_EQ(args[1].get_type(), mc::type_id::string_type);
    EXPECT_EQ(args[2].get_type(), mc::type_id::int32_type);
    EXPECT_EQ(args[3].get_type(), mc::type_id::bool_type);
}
