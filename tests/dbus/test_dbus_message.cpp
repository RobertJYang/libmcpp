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
#include <mc/variant.h>

using namespace mc::dbus;

// 测试 dbus::stream 与 libdbus 互操作
class dbus_interop_test : public ::testing::Test {
protected:
    void SetUp() override {
        // 确保 dbus 库已初始化
        dbus_error_init(&m_error);
    }

    void TearDown() override {
        if (m_message != nullptr) {
            dbus_message_unref(m_message);
            m_message = nullptr;
        }

        if (dbus_error_is_set(&m_error)) {
            dbus_error_free(&m_error);
        }
    }

    // 辅助函数：验证字节对齐
    void verify_alignment(stream& s, size_t expected_size) {
        // 获取流中的实际字节数
        size_t actual_size = s.readable_bytes();

        // 验证实际大小与期望大小的差异（对齐填充）
        // 实际大小应该大于或等于期望大小，因为可能有对齐字节
        EXPECT_GE(actual_size, expected_size);

        // 确保数据可以被正确读取
        s.seek_read(0);
        EXPECT_EQ(s.readable_bytes(), actual_size);
    }

    // 辅助函数：从 DBusMessage 获取数据
    void marshal_dbus_message(DBusMessage* msg, char** data, int* size) {
        dbus_message_marshal(msg, data, size);
        ASSERT_NE(*data, nullptr);
        ASSERT_GT(*size, 0);
    }

    DBusMessage* m_message{nullptr};
    DBusError    m_error;
};

// 测试从 mc::dbus::stream 写入数据并由 libdbus 读取
TEST_F(dbus_interop_test, test_stream_to_dbus_message) {
    // 使用 stream 创建一个消息
    stream output_stream;

    variants_message msg;
    msg.type      = message_type::method_call;
    msg.interface = "org.example.Test";
    msg.member    = "TestVariantNesting";
    msg.path      = "/org/example/Test";
    // msg.sender      = "org.example.Test";
    msg.destination = "org.example.Test";
    msg.serial      = 1;

    // 写入各种类型的数据
    uint8_t     u8  = 42;
    uint16_t    u16 = 4242;
    uint32_t    u32 = 424242;
    uint64_t    u64 = 42424242;
    double      d   = 42.42;
    std::string str = "从stream写入的字符串";
    bool        b   = true;

    // 写入基本类型
    msg << u8 << u16 << u32 << u64 << d << str << b;

    // // 写入一个数组
    mc::variants arr = {1, 2, 3, 4, 5};
    msg << arr;

    // 写入字典
    mc::dict dict = {{"key1", 100}, {"key2", "value2"}, {"key3", true}};
    msg << dict;

    // 获取序列化数据
    output_stream << msg;
    std::string_view buffer = output_stream.get_data();

    // 使用 dbus 库解析数据
    m_message = dbus_message_demarshal(buffer.data(), buffer.size(), &m_error);
    ASSERT_NE(m_message, nullptr);

    // 验证消息内容
    EXPECT_TRUE(dbus_message_has_path(m_message, msg.path.str().c_str()));
    EXPECT_TRUE(dbus_message_has_interface(m_message, msg.interface.c_str()));
    EXPECT_TRUE(dbus_message_has_member(m_message, msg.member.c_str()));
    EXPECT_TRUE(dbus_message_has_signature(m_message, msg.signature.str().c_str()));
    const char* in_sig = dbus_message_get_signature(m_message);

    DBusMessageIter read_iter;
    dbus_message_iter_init(m_message, &read_iter);

    // 读取基本类型
    uint8_t     in_u8;
    uint16_t    in_u16;
    uint32_t    in_u32;
    uint64_t    in_u64;
    double      in_d;
    const char* in_str;
    bool        in_b;
    EXPECT_EQ(dbus_message_iter_get_arg_type(&read_iter), DBUS_TYPE_BYTE);
    dbus_message_iter_get_basic(&read_iter, &in_u8);
    EXPECT_EQ(in_u8, u8);
    dbus_message_iter_next(&read_iter);
    EXPECT_EQ(dbus_message_iter_get_arg_type(&read_iter), DBUS_TYPE_UINT16);
    dbus_message_iter_get_basic(&read_iter, &in_u16);
    EXPECT_EQ(in_u16, u16);
    dbus_message_iter_next(&read_iter);
    EXPECT_EQ(dbus_message_iter_get_arg_type(&read_iter), DBUS_TYPE_UINT32);
    dbus_message_iter_get_basic(&read_iter, &in_u32);
    EXPECT_EQ(in_u32, u32);
    dbus_message_iter_next(&read_iter);
    EXPECT_EQ(dbus_message_iter_get_arg_type(&read_iter), DBUS_TYPE_UINT64);
    dbus_message_iter_get_basic(&read_iter, &in_u64);
    EXPECT_EQ(in_u64, u64);
    dbus_message_iter_next(&read_iter);
    EXPECT_EQ(dbus_message_iter_get_arg_type(&read_iter), DBUS_TYPE_DOUBLE);
    dbus_message_iter_get_basic(&read_iter, &in_d);
    EXPECT_EQ(in_d, d);
    dbus_message_iter_next(&read_iter);
    EXPECT_EQ(dbus_message_iter_get_arg_type(&read_iter), DBUS_TYPE_STRING);
    dbus_message_iter_get_basic(&read_iter, &in_str);
    EXPECT_EQ(in_str, str);
    dbus_message_iter_next(&read_iter);
    EXPECT_EQ(dbus_message_iter_get_arg_type(&read_iter), DBUS_TYPE_BOOLEAN);
    dbus_message_iter_get_basic(&read_iter, &in_b);
    EXPECT_EQ(in_b, b);
    dbus_message_iter_next(&read_iter);

    // 读取数组
    mc::variants    arr_in;
    DBusMessageIter array_iter;
    EXPECT_EQ(dbus_message_iter_get_arg_type(&read_iter), DBUS_TYPE_ARRAY);
    dbus_message_iter_recurse(&read_iter, &array_iter);
    EXPECT_EQ(dbus_message_iter_get_arg_type(&array_iter), DBUS_TYPE_VARIANT);
    while (dbus_message_iter_get_arg_type(&array_iter) == DBUS_TYPE_VARIANT) {
        DBusMessageIter variant_iter;
        dbus_message_iter_recurse(&array_iter, &variant_iter);
        EXPECT_EQ(dbus_message_iter_get_arg_type(&variant_iter), DBUS_TYPE_INT32);
        int32_t value;
        dbus_message_iter_get_basic(&variant_iter, &value);
        arr_in.push_back(value);
        dbus_message_iter_next(&array_iter);
    }
    EXPECT_EQ(arr_in, arr);
    dbus_message_iter_next(&read_iter);

    // 读取字典
    mc::mutable_dict dict_in;
    DBusMessageIter  dict_iter;
    EXPECT_EQ(dbus_message_iter_get_arg_type(&read_iter), DBUS_TYPE_ARRAY);
    dbus_message_iter_recurse(&read_iter, &dict_iter);
    EXPECT_EQ(dbus_message_iter_get_arg_type(&dict_iter), DBUS_TYPE_DICT_ENTRY);
    while (dbus_message_iter_get_arg_type(&dict_iter) == DBUS_TYPE_DICT_ENTRY) {
        DBusMessageIter entry_iter;
        dbus_message_iter_recurse(&dict_iter, &entry_iter);

        // 读取键
        const char* key;
        dbus_message_iter_get_basic(&entry_iter, &key);
        dbus_message_iter_next(&entry_iter);

        // 读取值
        EXPECT_EQ(dbus_message_iter_get_arg_type(&entry_iter), DBUS_TYPE_VARIANT);
        DBusMessageIter value_iter;
        dbus_message_iter_recurse(&entry_iter, &value_iter);
        if (dbus_message_iter_get_arg_type(&value_iter) == DBUS_TYPE_STRING) {
            const char* value;
            dbus_message_iter_get_basic(&value_iter, &value);
            dict_in[key] = value;
        } else if (dbus_message_iter_get_arg_type(&value_iter) == DBUS_TYPE_INT32) {
            int32_t value;
            dbus_message_iter_get_basic(&value_iter, &value);
            dict_in[key] = value;
        } else if (dbus_message_iter_get_arg_type(&value_iter) == DBUS_TYPE_BOOLEAN) {
            int32_t value = 0;
            dbus_message_iter_get_basic(&value_iter, &value);
            dict_in[key] = value != 0;
        }
        dbus_message_iter_next(&dict_iter);
    }
    EXPECT_EQ(dict_in, dict);

    dbus_message_iter_next(&read_iter);
    EXPECT_EQ(dbus_message_iter_get_arg_type(&read_iter), DBUS_TYPE_INVALID);

    // 使用 stream 解析数据
    stream input_stream(stream::buffer::wrap(buffer.data(), buffer.size()), false);

    variants_message msg_in;
    input_stream >> msg_in;
    EXPECT_EQ(input_stream.readable_bytes(), 0);
    EXPECT_EQ(msg_in.body, msg.body);
}

// 测试变体嵌套和对齐特殊情况
TEST_F(dbus_interop_test, test_dbus_message_to_stream) {
    m_message = dbus_message_new_method_call("org.example.Test", "/org/example/Test",
                                             "org.example.Test", "TestVariantNesting");
    ASSERT_NE(m_message, nullptr);

    uint8_t      u8   = 42;
    uint16_t     u16  = 4242;
    uint32_t     u32  = 424242;
    uint64_t     u64  = 42424242;
    double       d    = 42.42;
    std::string  str  = "从stream写入的字符串";
    int          b    = true;
    mc::variants arr  = {1, 2, 3, 4, 5};
    mc::dict     dict = {{"key1", 100}, {"key2", "value2"}, {"key3", true}};

    dbus_message_set_serial(m_message, 1);

    DBusMessageIter iter;
    dbus_message_iter_init_append(m_message, &iter);

    // 写入基本类型
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_BYTE, &u8);
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_UINT16, &u16);
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_UINT32, &u32);
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_UINT64, &u64);
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_DOUBLE, &d);
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &str);
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_BOOLEAN, &b);

    // 写入数组
    DBusMessageIter array_iter;
    dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "v", &array_iter);
    for (auto& value : arr) {
        DBusMessageIter variant_iter;
        if (value.get_type() == mc::type_id::int32_type) {
            int32_t v = value.as<int32_t>();
            dbus_message_iter_open_container(&array_iter, DBUS_TYPE_VARIANT, "i", &variant_iter);
            dbus_message_iter_append_basic(&variant_iter, DBUS_TYPE_INT32, &v);
        } else if (value.get_type() == mc::type_id::string_type) {
            const char* v = value.get_string().c_str();
            dbus_message_iter_open_container(&array_iter, DBUS_TYPE_VARIANT, "s", &variant_iter);
            dbus_message_iter_append_basic(&variant_iter, DBUS_TYPE_STRING, &v);
        } else if (value.get_type() == mc::type_id::bool_type) {
            uint32_t v = value.as<bool>();
            dbus_message_iter_open_container(&array_iter, DBUS_TYPE_VARIANT, "b", &variant_iter);
            dbus_message_iter_append_basic(&variant_iter, DBUS_TYPE_BOOLEAN, &v);
        }
        dbus_message_iter_close_container(&array_iter, &variant_iter);
    }
    dbus_message_iter_close_container(&iter, &array_iter);

    // 写入字典
    DBusMessageIter std_map_iter;
    dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sv}", &std_map_iter);
    for (auto& entry : dict) {
        DBusMessageIter entry_iter;
        dbus_message_iter_open_container(&std_map_iter, DBUS_TYPE_DICT_ENTRY, nullptr, &entry_iter);
        const char* key = entry.key.get_string().c_str();
        dbus_message_iter_append_basic(&entry_iter, DBUS_TYPE_STRING, &key);

        DBusMessageIter viter;
        if (entry.value.is_string()) {
            dbus_message_iter_open_container(&entry_iter, DBUS_TYPE_VARIANT, "s", &viter);
            const char* v = entry.value.get_string().c_str();
            dbus_message_iter_append_basic(&viter, DBUS_TYPE_STRING, &v);
        } else if (entry.value.is_int32()) {
            dbus_message_iter_open_container(&entry_iter, DBUS_TYPE_VARIANT, "i", &viter);
            int32_t v = entry.value.as<int32_t>();
            dbus_message_iter_append_basic(&viter, DBUS_TYPE_INT32, &v);
        } else if (entry.value.is_bool()) {
            dbus_message_iter_open_container(&entry_iter, DBUS_TYPE_VARIANT, "b", &viter);
            uint32_t v = entry.value.as<bool>();
            dbus_message_iter_append_basic(&viter, DBUS_TYPE_BOOLEAN, &v);
        }
        dbus_message_iter_close_container(&entry_iter, &viter);
        dbus_message_iter_close_container(&std_map_iter, &entry_iter);
    }
    dbus_message_iter_close_container(&iter, &std_map_iter);

    // 序列化消息
    char* marshalled_data = nullptr;
    int   len             = 0;
    marshal_dbus_message(m_message, &marshalled_data, &len);

    // 使用 dbus::stream 解析数据
    variants_message msg;
    stream           stream(stream::buffer::wrap(marshalled_data, len), false);
    stream >> msg;

    EXPECT_EQ(msg.body.size(), 9);
    EXPECT_EQ(msg.body, (mc::variants{u8, u16, u32, u64, d, str, b, arr, dict}));
    EXPECT_EQ(stream.readable_bytes(), 0);

    dbus_free(marshalled_data);
}