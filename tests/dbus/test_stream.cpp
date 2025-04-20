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
    }
};

// 测试构建和解析字符串类型
TEST_F(stream_test, test_string) {
    stream stream;
    stream << "hello";
    std::string str;
    stream >> str;
    EXPECT_EQ(str, "hello");
    EXPECT_EQ(stream.readable_bytes(), 0);
}

// 测试构建和解析字符串视图
TEST_F(stream_test, test_string_view) {
    stream           stream;
    std::string_view sv_out = "hello world";
    stream << sv_out;
    std::string_view sv_in;
    stream >> sv_in;
    EXPECT_EQ(sv_in, sv_out);
    EXPECT_EQ(stream.readable_bytes(), 0);
}

// 测试构建和解析基本类型
TEST_F(stream_test, test_basic_types) {
    stream stream;

    // 测试布尔值
    bool b_out = true;
    stream << b_out;
    bool b_in;
    stream >> b_in;
    EXPECT_EQ(b_in, b_out);
    EXPECT_EQ(stream.readable_bytes(), 0);

    // 测试数字类型
    int8_t   i8_out  = -42;
    uint8_t  u8_out  = 42;
    int16_t  i16_out = -1234;
    uint16_t u16_out = 1234;
    int32_t  i32_out = -123456;
    uint32_t u32_out = 123456;
    int64_t  i64_out = -1234567890;
    uint64_t u64_out = 1234567890;
    double   d_out   = 3.14159;

    stream << i8_out << u8_out << i16_out << u16_out << i32_out << u32_out << i64_out << u64_out
           << d_out;

    int8_t   i8_in;
    uint8_t  u8_in;
    int16_t  i16_in;
    uint16_t u16_in;
    int32_t  i32_in;
    uint32_t u32_in;
    int64_t  i64_in;
    uint64_t u64_in;
    double   d_in;

    stream >> i8_in >> u8_in >> i16_in >> u16_in >> i32_in >> u32_in >> i64_in >> u64_in >> d_in;

    EXPECT_EQ(i8_in, i8_out);
    EXPECT_EQ(u8_in, u8_out);
    EXPECT_EQ(i16_in, i16_out);
    EXPECT_EQ(u16_in, u16_out);
    EXPECT_EQ(i32_in, i32_out);
    EXPECT_EQ(u32_in, u32_out);
    EXPECT_EQ(i64_in, i64_out);
    EXPECT_EQ(u64_in, u64_out);
    EXPECT_DOUBLE_EQ(d_in, d_out);
    EXPECT_EQ(stream.readable_bytes(), 0);
}

// 测试构建和解析路径
TEST_F(stream_test, test_path) {
    stream stream;
    path   path_out("/org/freedesktop/DBus");
    stream << path_out;
    path path_in;
    stream >> path_in;
    EXPECT_EQ(path_in.str(), path_out.str());
    EXPECT_EQ(stream.readable_bytes(), 0);
}

// 测试构建和解析签名
TEST_F(stream_test, test_signature) {
    stream    stream;
    signature sig_out("a{sv}");
    stream << sig_out;
    signature sig_in;
    stream >> sig_in;
    EXPECT_EQ(sig_in.str(), sig_out.str());
    EXPECT_EQ(stream.readable_bytes(), 0);
}

// 测试构建和解析 variant
TEST_F(stream_test, test_variant) {
    stream stream;

    // 基本类型
    mc::variant v1_out = 42;
    mc::variant v2_out = "测试字符串";
    mc::variant v3_out = true;
    mc::variant v4_out = 3.14159;

    stream << v1_out << v2_out << v3_out << v4_out;

    mc::variant v1_in;
    mc::variant v2_in;
    mc::variant v3_in;
    mc::variant v4_in;

    stream >> v1_in >> v2_in >> v3_in >> v4_in;

    EXPECT_EQ(v1_in, v1_out);
    EXPECT_EQ(v2_in, v2_out);
    EXPECT_EQ(v3_in, v3_out);
    EXPECT_EQ(v4_in, v4_out);
    EXPECT_EQ(stream.readable_bytes(), 0);
}

// 测试构建和解析数组
TEST_F(stream_test, test_array) {
    stream stream;

    // 整型数组
    mc::variants int_array_out = {1, 2, 3, 4, 5};
    stream << int_array_out;
    mc::variants int_array_in;
    stream >> int_array_in;
    EXPECT_EQ(int_array_in, int_array_out);

    // 字符串数组
    mc::variants str_array_out = {"one", "two", "three"};
    stream << str_array_out;
    mc::variants str_array_in;
    stream >> str_array_in;
    EXPECT_EQ(str_array_in, str_array_out);

    // 空数组
    mc::variants empty_array_out;
    stream << empty_array_out;
    mc::variants empty_array_in;
    stream >> empty_array_in;
    EXPECT_TRUE(empty_array_in.empty());
    EXPECT_EQ(stream.readable_bytes(), 0);
}

// 测试构建和解析字典
TEST_F(stream_test, test_dict) {
    stream stream;

    // 字符串到整型的字典
    mc::dict dict_out = {
        {"one", 1},
        {"two", 2},
        {"three", 3},
    };

    stream << dict_out;
    mc::dict dict_in;
    stream >> dict_in;
    EXPECT_EQ(dict_in, dict_out);

    // 空字典
    mc::dict empty_dict_out;
    stream << empty_dict_out;
    mc::dict empty_dict_in;
    stream >> empty_dict_in;
    EXPECT_TRUE(empty_dict_in.empty());
    EXPECT_EQ(stream.readable_bytes(), 0);
}

// 测试构建和解析复杂嵌套结构
TEST_F(stream_test, test_complex_structure) {
    stream stream;

    // 创建复杂结构：包含数组和字典的字典
    mc::dict complex_out = {
        {"name", "复杂测试"},
        {"values", mc::variants{1, 2, 3, 4, 5}},
        {"properties",
         mc::dict{
             {"key1", "value1"},
             {"key2", 42},
             {"key3", true},
         }},
    };

    stream << complex_out;
    mc::dict complex_in;
    stream >> complex_in;

    EXPECT_EQ(complex_in, complex_out);
    EXPECT_EQ(stream.readable_bytes(), 0);
}

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

// 自定义可反射结构体
struct person {
    std::string              name;
    int                      age;
    bool                     active;
    std::vector<std::string> hobbies;
};

// 为自定义类型实现反射
MC_REFLECT(person, (name)(age)(active)(hobbies))

// 测试构建和解析自定义可反射类型
TEST_F(stream_test, test_custom_reflected_type) {
    stream stream;

    // 创建自定义类型实例
    person person_out;
    person_out.name    = "张三";
    person_out.age     = 30;
    person_out.active  = true;
    person_out.hobbies = {"读书", "游泳", "旅游"};

    // 将实例写入流
    stream << person_out;

    // 从流中读取实例
    person person_in;
    stream >> person_in;

    // 验证读写结果
    EXPECT_EQ(person_in.name, person_out.name);
    EXPECT_EQ(person_in.age, person_out.age);
    EXPECT_EQ(person_in.active, person_out.active);
    EXPECT_EQ(person_in.hobbies, person_out.hobbies);
    EXPECT_EQ(stream.readable_bytes(), 0);
}
