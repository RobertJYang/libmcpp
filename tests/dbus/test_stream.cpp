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
#include <mc/dbus/stream.h>

using namespace mc::dbus;

// 测试消息生成器和解析器
class stream_test : public ::testing::Test {
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

// // 测试构建和解析字符串类型
// TEST_F(stream_test, test_string) {
//     stream stream;
//     stream << "hello";
//     std::string str;
//     stream >> str;
//     EXPECT_EQ(str, "hello");
//     EXPECT_EQ(stream.readable_bytes(), 0);
// }

// // 测试构建和解析字符串视图
// TEST_F(stream_test, test_string_view) {
//     stream           stream;
//     std::string_view sv_out = "hello world";
//     stream << sv_out;
//     std::string_view sv_in;
//     stream >> sv_in;
//     EXPECT_EQ(sv_in, sv_out);
//     EXPECT_EQ(stream.readable_bytes(), 0);
// }

// // 测试构建和解析基本类型
// TEST_F(stream_test, test_basic_types) {
//     stream stream;

//     // 测试布尔值
//     bool b_out = true;
//     stream << b_out;
//     bool b_in;
//     stream >> b_in;
//     EXPECT_EQ(b_in, b_out);
//     EXPECT_EQ(stream.readable_bytes(), 0);

//     // 测试数字类型
//     int8_t   i8_out  = -42;
//     uint8_t  u8_out  = 42;
//     int16_t  i16_out = -1234;
//     uint16_t u16_out = 1234;
//     int32_t  i32_out = -123456;
//     uint32_t u32_out = 123456;
//     int64_t  i64_out = -1234567890;
//     uint64_t u64_out = 1234567890;
//     double   d_out   = 3.14159;

//     stream << i8_out << u8_out << i16_out << u16_out << i32_out << u32_out << i64_out << u64_out
//            << d_out;

//     int8_t   i8_in;
//     uint8_t  u8_in;
//     int16_t  i16_in;
//     uint16_t u16_in;
//     int32_t  i32_in;
//     uint32_t u32_in;
//     int64_t  i64_in;
//     uint64_t u64_in;
//     double   d_in;

//     stream >> i8_in >> u8_in >> i16_in >> u16_in >> i32_in >> u32_in >> i64_in >> u64_in >> d_in;

//     EXPECT_EQ(i8_in, i8_out);
//     EXPECT_EQ(u8_in, u8_out);
//     EXPECT_EQ(i16_in, i16_out);
//     EXPECT_EQ(u16_in, u16_out);
//     EXPECT_EQ(i32_in, i32_out);
//     EXPECT_EQ(u32_in, u32_out);
//     EXPECT_EQ(i64_in, i64_out);
//     EXPECT_EQ(u64_in, u64_out);
//     EXPECT_DOUBLE_EQ(d_in, d_out);
//     EXPECT_EQ(stream.readable_bytes(), 0);
// }

// // 测试构建和解析路径
// TEST_F(stream_test, test_path) {
//     stream stream;
//     path   path_out("/org/freedesktop/DBus");
//     stream << path_out;
//     path path_in;
//     stream >> path_in;
//     EXPECT_EQ(path_in.str(), path_out.str());
//     EXPECT_EQ(stream.readable_bytes(), 0);
// }

// // 测试构建和解析签名
// TEST_F(stream_test, test_signature) {
//     stream    stream;
//     signature sig_out("a{sv}");
//     stream << sig_out;
//     signature sig_in;
//     stream >> sig_in;
//     EXPECT_EQ(sig_in.str(), sig_out.str());
//     EXPECT_EQ(stream.readable_bytes(), 0);
// }

// // 测试构建和解析 variant
// TEST_F(stream_test, test_variant) {
//     stream stream;

//     // 基本类型
//     mc::variant v1_out = 42;
//     mc::variant v2_out = "测试字符串";
//     mc::variant v3_out = true;
//     mc::variant v4_out = 3.14159;

//     stream << v1_out << v2_out << v3_out << v4_out;

//     mc::variant v1_in;
//     mc::variant v2_in;
//     mc::variant v3_in;
//     mc::variant v4_in;

//     stream >> v1_in >> v2_in >> v3_in >> v4_in;

//     EXPECT_EQ(v1_in, v1_out);
//     EXPECT_EQ(v2_in, v2_out);
//     EXPECT_EQ(v3_in, v3_out);
//     EXPECT_EQ(v4_in, v4_out);
//     EXPECT_EQ(stream.readable_bytes(), 0);
// }

// // 测试构建和解析数组
// TEST_F(stream_test, test_array) {
//     stream stream;

//     // 整型数组
//     mc::variants int_array_out = {1, 2, 3, 4, 5};
//     stream << int_array_out;
//     mc::variants int_array_in;
//     stream >> int_array_in;

//     EXPECT_EQ(int_array_in.size(), int_array_out.size());
//     for (size_t i = 0; i < int_array_out.size(); ++i) {
//         EXPECT_EQ(int_array_in[i], int_array_out[i]);
//     }

//     // 字符串数组
//     mc::variants str_array_out = {"one", "two", "three"};
//     stream << str_array_out;
//     mc::variants str_array_in;
//     stream >> str_array_in;

//     EXPECT_EQ(str_array_in.size(), str_array_out.size());
//     for (size_t i = 0; i < str_array_out.size(); ++i) {
//         EXPECT_EQ(str_array_in[i], str_array_out[i]);
//     }

//     // 空数组
//     mc::variants empty_array_out;
//     stream << empty_array_out;
//     mc::variants empty_array_in;
//     stream >> empty_array_in;
//     EXPECT_TRUE(empty_array_in.empty());

//     EXPECT_EQ(stream.readable_bytes(), 0);
// }

// // 测试构建和解析字典
// TEST_F(stream_test, test_dict) {
//     stream stream;

//     // 字符串到整型的字典
//     mc::mutable_dict dict_out = {
//         {"one", 1},
//         {"two", 2},
//         {"three", 3},
//     };

//     stream << dict_out;
//     mc::dict dict_in;
//     stream >> dict_in;

//     EXPECT_EQ(dict_in.size(), dict_out.size());
//     EXPECT_TRUE(dict_in["one"] == dict_out["one"]);
//     EXPECT_TRUE(dict_in["two"] == dict_out["two"]);
//     EXPECT_TRUE(dict_in["three"] == dict_out["three"]);

//     // 空字典
//     mc::dict empty_dict_out;
//     stream << empty_dict_out;
//     mc::dict empty_dict_in;
//     stream >> empty_dict_in;
//     EXPECT_TRUE(empty_dict_in.empty());

//     EXPECT_EQ(stream.readable_bytes(), 0);
// }

// // 测试构建和解析复杂嵌套结构
// TEST_F(stream_test, test_complex_structure) {
//     stream stream;

//     // 创建复杂结构：包含数组和字典的字典
//     mc::mutable_dict complex_out = {
//         {"name", "复杂测试"},
//         {"values", mc::variants{1, 2, 3, 4, 5}},
//         {"properties",
//          mc::mutable_dict{
//              {"key1", "value1"},
//              {"key2", 42},
//              {"key3", true},
//          }},
//     };

//     stream << complex_out;
//     mc::dict complex_in;
//     stream >> complex_in;

//     // 验证顶层字典
//     EXPECT_EQ(complex_in.size(), complex_out.size());
//     EXPECT_EQ(complex_in["name"], complex_out["name"]);

//     // 验证数组
//     EXPECT_EQ(complex_in["values"].get_array().size(), complex_out["values"].get_array().size());
//     for (size_t i = 0; i < complex_out["values"].get_array().size(); ++i) {
//         EXPECT_EQ(complex_in["values"].get_array()[i], complex_out["values"].get_array()[i]);
//     }

//     // 验证嵌套字典
//     EXPECT_EQ(complex_in["properties"].size(), complex_out["properties"].size());
//     EXPECT_TRUE(complex_in["properties"] == complex_out["properties"]);
//     EXPECT_EQ(stream.readable_bytes(), 0);
// }

// 测试流对齐
TEST_F(stream_test, test_alignment) {
    stream stream;

    // 向流中写入一个 uint8_t，然后写入 uint32_t
    // uint32_t 需要 4 字节对齐，所以应该会有对齐字节
    uint8_t  u8_out  = 42;
    uint32_t u32_out = 0x12345678;

    stream << u8_out << u32_out;

    // 记录流的大小，应该比原始数据大（因为有对齐填充）
    std::size_t stream_size = stream.readable_bytes();

    uint8_t  u8_in;
    uint32_t u32_in;

    stream >> u8_in >> u32_in;

    EXPECT_EQ(u8_in, u8_out);
    EXPECT_EQ(u32_in, u32_out);
    EXPECT_GT(stream_size, sizeof(uint8_t) + sizeof(uint32_t));
    EXPECT_EQ(stream.readable_bytes(), 0);
}

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

TEST_F(stream_test, test_dbus_message) {
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
