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
#include <mc/dbus/message_header.h>

using namespace mc::dbus;

// 测试消息生成器和解析器
class marshalled_test : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
        if (message != nullptr) {
            dbus_message_unref(message);
        }
    }

    DBusMessage* message{nullptr};
};

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
    std::optional<int>                   std_o;    // v
    std::pair<int, std::string>          std_p;    // (is)
    std::tuple<int, std::string, bool>   std_t;    // (isb)
    std::vector<int>                     std_vec;  // a(i)
    std::list<std::string>               std_list; // a(s)
    std::map<int, int>                   std_map;  // a{ii}
    std::unordered_map<std::string, int> std_umap; // a{si}
};

MC_REFLECT(test_struct, (i8)(u8)(i16)(u16)(i32)(u32)(i64)(u64)(f)(d)(str)(b)(p)(sig)(v)(a)(m)
           // 标准库类型
           (std_o)(std_p)(std_t)(std_vec)(std_list)(std_map)(std_umap))

TEST_F(marshalled_test, test_dbus_message) {
    test_struct ts_out;
    ts_out.i8       = 1;
    ts_out.u8       = 2;
    ts_out.i16      = 3;
    ts_out.u16      = 4;
    ts_out.i32      = 5;
    ts_out.u32      = 6;
    ts_out.i64      = 7;
    ts_out.u64      = 8;
    ts_out.f        = 9.0f;
    ts_out.d        = 10.0;
    ts_out.str      = "Hello";
    ts_out.b        = true;
    ts_out.p        = "/org/freedesktop/DBus";
    ts_out.sig      = "s";
    ts_out.v        = "world";
    ts_out.a        = {"1", "12", "123", "1234", "12345"};
    ts_out.m        = {{"key1", "1"}, {"key12", "12"}, {"key123", "123"}};
    ts_out.std_o    = 1;
    ts_out.std_p    = {1, "12"};
    ts_out.std_t    = {1, "12", true};
    ts_out.std_vec  = {1, 2, 3, 4, 5};
    ts_out.std_list = {"1", "12", "123", "1234", "12345"};
    ts_out.std_map  = {{1, 1}, {2, 2}, {3, 3}, {4, 4}, {5, 5}};
    ts_out.std_umap = {{"1", 1}, {"12", 2}, {"123", 3}, {"1234", 4}, {"12345", 5}};

    message = dbus_message_new_method_call("org.freedesktop.DBus", "/org/freedesktop/DBus",
                                           "org.freedesktop.DBus", "Hello");
    EXPECT_NE(message, nullptr);

    DBusMessageIter iter;
    dbus_message_iter_init_append(message, &iter);

    DBusMessageIter struct_iter;
    dbus_message_iter_open_container(&iter, DBUS_TYPE_STRUCT, nullptr, &struct_iter);
    dbus_message_iter_append_basic(&struct_iter, DBUS_TYPE_BYTE, &ts_out.i8);
    dbus_message_iter_append_basic(&struct_iter, DBUS_TYPE_BYTE, &ts_out.u8);
    dbus_message_iter_append_basic(&struct_iter, DBUS_TYPE_INT16, &ts_out.i16);
    dbus_message_iter_append_basic(&struct_iter, DBUS_TYPE_UINT16, &ts_out.u16);
    dbus_message_iter_append_basic(&struct_iter, DBUS_TYPE_INT32, &ts_out.i32);
    dbus_message_iter_append_basic(&struct_iter, DBUS_TYPE_UINT32, &ts_out.u32);
    dbus_message_iter_append_basic(&struct_iter, DBUS_TYPE_INT64, &ts_out.i64);
    dbus_message_iter_append_basic(&struct_iter, DBUS_TYPE_UINT64, &ts_out.u64);

    // dbus 不支持 float 类型，需要转换为 double
    double tmp_f = ts_out.f;
    dbus_message_iter_append_basic(&struct_iter, DBUS_TYPE_DOUBLE, &tmp_f);
    dbus_message_iter_append_basic(&struct_iter, DBUS_TYPE_DOUBLE, &ts_out.d);

    const char* str = ts_out.str.c_str();
    dbus_message_iter_append_basic(&struct_iter, DBUS_TYPE_STRING, &str);

    // dbus 的 bool 类型是 uint32_t 类型
    uint32_t b = ts_out.b ? 1 : 0;
    dbus_message_iter_append_basic(&struct_iter, DBUS_TYPE_BOOLEAN, &b);

    const char* p = ts_out.p.str().c_str();
    dbus_message_iter_append_basic(&struct_iter, DBUS_TYPE_OBJECT_PATH, &p);
    const char* sig = ts_out.sig.str().c_str();
    dbus_message_iter_append_basic(&struct_iter, DBUS_TYPE_SIGNATURE, &sig);

    // 写入 variant
    DBusMessageIter viter;
    dbus_message_iter_open_container(&struct_iter, DBUS_TYPE_VARIANT, "s", &viter);
    const char* v = ts_out.v.get_string().c_str();
    dbus_message_iter_append_basic(&viter, DBUS_TYPE_STRING, &v);
    dbus_message_iter_close_container(&struct_iter, &viter);

    // 写入 array
    DBusMessageIter aiter;
    dbus_message_iter_open_container(&struct_iter, DBUS_TYPE_ARRAY, "v", &aiter);
    for (auto& v : ts_out.a) {
        DBusMessageIter viter;
        dbus_message_iter_open_container(&aiter, DBUS_TYPE_VARIANT, "s", &viter);
        const char* v_val = v.get_string().c_str();
        dbus_message_iter_append_basic(&viter, DBUS_TYPE_STRING, &v_val);
        dbus_message_iter_close_container(&aiter, &viter);
    }
    dbus_message_iter_close_container(&struct_iter, &aiter);

    // 写入 dict
    DBusMessageIter diter;
    dbus_message_iter_open_container(&struct_iter, DBUS_TYPE_ARRAY, "{sv}", &diter);
    for (auto& entry : ts_out.m) {
        DBusMessageIter entry_iter;
        dbus_message_iter_open_container(&diter, DBUS_TYPE_DICT_ENTRY, nullptr, &entry_iter);

        // 写入 key
        const char* v_key = entry.key.get_string().c_str();
        dbus_message_iter_append_basic(&entry_iter, DBUS_TYPE_STRING, &v_key);

        // 写入 value
        DBusMessageIter viter;
        dbus_message_iter_open_container(&entry_iter, DBUS_TYPE_VARIANT, "s", &viter);
        const char* v_val = entry.value.get_string().c_str();
        dbus_message_iter_append_basic(&viter, DBUS_TYPE_STRING, &v_val);
        dbus_message_iter_close_container(&entry_iter, &viter);

        dbus_message_iter_close_container(&diter, &entry_iter);
    }
    dbus_message_iter_close_container(&struct_iter, &diter);

    // 写入 optional
    DBusMessageIter std_oiter;
    dbus_message_iter_open_container(&struct_iter, DBUS_TYPE_ARRAY, "i", &std_oiter);
    if (ts_out.std_o.has_value()) {
        int* o_val = &ts_out.std_o.value();
        dbus_message_iter_append_basic(&std_oiter, DBUS_TYPE_INT32, o_val);
    }
    dbus_message_iter_close_container(&struct_iter, &std_oiter);

    // 写入 pair
    DBusMessageIter std_piter;
    dbus_message_iter_open_container(&struct_iter, DBUS_TYPE_STRUCT, nullptr, &std_piter);
    dbus_message_iter_append_basic(&std_piter, DBUS_TYPE_INT32, &ts_out.std_p.first);
    const char* p_val = ts_out.std_p.second.c_str();
    dbus_message_iter_append_basic(&std_piter, DBUS_TYPE_STRING, &p_val);
    dbus_message_iter_close_container(&struct_iter, &std_piter);

    // 写入 tuple
    DBusMessageIter std_titer;
    dbus_message_iter_open_container(&struct_iter, DBUS_TYPE_STRUCT, nullptr, &std_titer);
    dbus_message_iter_append_basic(&std_titer, DBUS_TYPE_INT32, &std::get<0>(ts_out.std_t));
    const char* t_val = std::get<1>(ts_out.std_t).c_str();
    dbus_message_iter_append_basic(&std_titer, DBUS_TYPE_STRING, &t_val);
    uint32_t t_b = std::get<2>(ts_out.std_t) ? 1 : 0;
    dbus_message_iter_append_basic(&std_titer, DBUS_TYPE_BOOLEAN, &t_b);
    dbus_message_iter_close_container(&struct_iter, &std_titer);

    // 写入 vector
    DBusMessageIter std_vec_iter;
    dbus_message_iter_open_container(&struct_iter, DBUS_TYPE_ARRAY, "i", &std_vec_iter);
    for (auto& v : ts_out.std_vec) {
        dbus_message_iter_append_basic(&std_vec_iter, DBUS_TYPE_INT32, &v);
    }
    dbus_message_iter_close_container(&struct_iter, &std_vec_iter);

    // 写入 list
    DBusMessageIter std_list_iter;
    dbus_message_iter_open_container(&struct_iter, DBUS_TYPE_ARRAY, "s", &std_list_iter);
    for (auto& v : ts_out.std_list) {
        const char* l_val = v.c_str();
        dbus_message_iter_append_basic(&std_list_iter, DBUS_TYPE_STRING, &l_val);
    }
    dbus_message_iter_close_container(&struct_iter, &std_list_iter);

    // 写入 map
    DBusMessageIter std_map_iter;
    dbus_message_iter_open_container(&struct_iter, DBUS_TYPE_ARRAY, "{ii}", &std_map_iter);
    for (auto& v : ts_out.std_map) {
        DBusMessageIter entry_iter;
        dbus_message_iter_open_container(&std_map_iter, DBUS_TYPE_DICT_ENTRY, nullptr, &entry_iter);
        dbus_message_iter_append_basic(&entry_iter, DBUS_TYPE_INT32, &v.first);
        dbus_message_iter_append_basic(&entry_iter, DBUS_TYPE_INT32, &v.second);
        dbus_message_iter_close_container(&std_map_iter, &entry_iter);
    }
    dbus_message_iter_close_container(&struct_iter, &std_map_iter);

    // 写入 unordered_map
    DBusMessageIter std_umap_iter;
    dbus_message_iter_open_container(&struct_iter, DBUS_TYPE_ARRAY, "{si}", &std_umap_iter);
    for (auto& v : ts_out.std_umap) {
        DBusMessageIter entry_iter;
        dbus_message_iter_open_container(&std_umap_iter, DBUS_TYPE_DICT_ENTRY, nullptr,
                                         &entry_iter);
        const char* um_val = v.first.c_str();
        dbus_message_iter_append_basic(&entry_iter, DBUS_TYPE_STRING, &um_val);
        dbus_message_iter_append_basic(&entry_iter, DBUS_TYPE_INT32, &v.second);
        dbus_message_iter_close_container(&std_umap_iter, &entry_iter);
    }
    dbus_message_iter_close_container(&struct_iter, &std_umap_iter);

    // 关闭 struct 容器
    dbus_message_iter_close_container(&iter, &struct_iter);

    // 设置序列号
    dbus_message_set_serial(message, 123456);

    // 序列化消息
    char* marshalled_data{nullptr};
    int   len{0};
    dbus_message_marshal(message, &marshalled_data, &len);
    EXPECT_NE(marshalled_data, nullptr);
    EXPECT_GT(len, 0);
    const char* expected_sig = dbus_message_get_signature(message);

    message_header header;

    stream stream(mc::io::io_buffer::wrap_buffer(marshalled_data, len), false);
    stream >> header;
    EXPECT_EQ(header.m_type, message_type::method_call);
    EXPECT_EQ(header.m_path, "/org/freedesktop/DBus");
    EXPECT_EQ(header.m_interface, "org.freedesktop.DBus");
    EXPECT_EQ(header.m_member, "Hello");
    EXPECT_EQ(header.m_serial, 123456);
    EXPECT_EQ(header.m_signature, expected_sig);
    EXPECT_EQ(header.m_signature, get_signature<test_struct>());

    test_struct body;
    stream >> body;

    EXPECT_EQ(body.i8, 1);
    EXPECT_EQ(body.u8, 2);
    EXPECT_EQ(body.i16, 3);
    EXPECT_EQ(body.u16, 4);
    EXPECT_EQ(body.i32, 5);
    EXPECT_EQ(body.u32, 6);
    EXPECT_EQ(body.i64, 7);
    EXPECT_EQ(body.u64, 8);
    EXPECT_EQ(body.f, 9.0f);
    EXPECT_EQ(body.d, 10.0);
    EXPECT_EQ(body.str, "Hello");
    EXPECT_EQ(body.b, true);
    EXPECT_EQ(body.p, "/org/freedesktop/DBus");
    EXPECT_EQ(body.sig, "s");
    EXPECT_EQ(body.v, "world");
    EXPECT_EQ(body.a, ts_out.a);
    EXPECT_EQ(body.m, ts_out.m);
    EXPECT_EQ(body.std_o, 1);
    EXPECT_EQ(body.std_p, ts_out.std_p);
    EXPECT_EQ(body.std_t, ts_out.std_t);
    EXPECT_EQ(body.std_vec, ts_out.std_vec);
    EXPECT_EQ(body.std_list, ts_out.std_list);
    EXPECT_EQ(body.std_map, ts_out.std_map);
    EXPECT_EQ(body.std_umap, ts_out.std_umap);

    EXPECT_EQ(stream.readable_bytes(), 0);
}
