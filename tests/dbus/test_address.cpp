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
#include <mc/dbus/address.h>
#include <mc/exception.h>

using namespace mc::dbus;

TEST(AddressTest, EscapeValue) {
    // 测试普通字符不被转义
    EXPECT_EQ("abcABC123_/-.@", address::escape_value("abcABC123_/-.@"));

    // 测试特殊字符被转义
    EXPECT_EQ("hello%20world", address::escape_value("hello world"));
    EXPECT_EQ("%3A%2C%3B%3D", address::escape_value(":,;="));
    EXPECT_EQ("%25", address::escape_value("%"));

    // 测试控制字符转义
    EXPECT_EQ("test%0Aline", address::escape_value("test\nline"));
    EXPECT_EQ("tab%09char", address::escape_value("tab\tchar"));

    // 测试非ASCII字符转义
    EXPECT_EQ("%C3%A9%C3%A0%C3%BC", address::escape_value("éàü"));
}

TEST(AddressTest, UnescapeValue) {
    // 测试普通字符
    EXPECT_EQ("abcABC123_/-.@", address::unescape_value("abcABC123_/-.@"));

    // 测试转义字符
    EXPECT_EQ("hello world", address::unescape_value("hello%20world"));
    EXPECT_EQ(":,;=", address::unescape_value("%3A%2C%3B%3D"));
    EXPECT_EQ("%", address::unescape_value("%25"));

    // 测试控制字符
    EXPECT_EQ("test\nline", address::unescape_value("test%0Aline"));
    EXPECT_EQ("tab\tchar", address::unescape_value("tab%09char"));

    // 测试非ASCII字符
    EXPECT_EQ("éàü", address::unescape_value("%C3%A9%C3%A0%C3%BC"));

    // 测试不完整的转义序列
    EXPECT_FALSE(address::unescape_value("test%").has_value());
    EXPECT_FALSE(address::unescape_value("test%2").has_value());

    // 测试无效的转义序列
    EXPECT_FALSE(address::unescape_value("test%XX").has_value());
    EXPECT_FALSE(address::unescape_value("test%ZZ").has_value());
}

TEST(AddressTest, RoundTrip) {
    // 测试转义和解转义的往返
    auto original  = "Hello, World! 测试中文 éàü \t\n\r:;=@%";
    auto escaped   = address::escape_value(original);
    auto unescaped = address::unescape_value(escaped);
    ASSERT_TRUE(unescaped.has_value());
    EXPECT_EQ(original, *unescaped);
}

TEST(AddressTest, AddressEntry) {
    // 创建地址条目并设置值
    address_entry entry("unix");
    entry.set_value("path", "/tmp/dbus-test");
    entry.set_value("abstract", "test_socket");

    // 测试获取值
    EXPECT_EQ("unix", entry.get_method());
    EXPECT_EQ("/tmp/dbus-test", entry.get_value("path"));
    EXPECT_EQ("test_socket", entry.get_value("abstract"));
    EXPECT_TRUE(entry.has_key("path"));
    EXPECT_TRUE(entry.has_key("abstract"));
    EXPECT_FALSE(entry.has_key("nonexistent"));
    EXPECT_EQ("", entry.get_value("nonexistent"));

    // 测试to_string方法
    std::string str = entry.to_string();
    EXPECT_TRUE(str.find("unix:") != std::string::npos);
    EXPECT_TRUE(str.find("path=/tmp/dbus-test") != std::string::npos);
    EXPECT_TRUE(str.find("abstract=test_socket") != std::string::npos);

    // 测试键名验证
    EXPECT_THROW(entry.set_value("invalid-key", "value"), mc::invalid_arg_exception);
    EXPECT_THROW(entry.set_value("123invalid", "value"), mc::invalid_arg_exception);
    EXPECT_THROW(entry.set_value("key with space", "value"), mc::invalid_arg_exception);
}

TEST(AddressTest, ParseAddress) {
    // 测试解析单个地址
    std::string addr_str = "unix:path=/tmp/dbus-test";
    address     addr     = address::parse(addr_str);
    ASSERT_EQ(1, addr.get_entries().size());
    EXPECT_EQ("unix", addr.get_entries()[0]->get_method());
    EXPECT_EQ("/tmp/dbus-test", addr.get_entries()[0]->get_value("path"));

    // 测试解析多个地址
    addr_str = "unix:path=/tmp/dbus-test;tcp:host=localhost,port=1234";
    addr     = address::parse(addr_str);
    ASSERT_EQ(2, addr.get_entries().size());
    EXPECT_EQ("unix", addr.get_entries()[0]->get_method());
    EXPECT_EQ("/tmp/dbus-test", addr.get_entries()[0]->get_value("path"));
    EXPECT_EQ("tcp", addr.get_entries()[1]->get_method());
    EXPECT_EQ("localhost", addr.get_entries()[1]->get_value("host"));
    EXPECT_EQ("1234", addr.get_entries()[1]->get_value("port"));

    // 测试含转义字符的值
    addr_str = "unix:path=/tmp/dbus%20test";
    addr     = address::parse(addr_str);
    ASSERT_EQ(1, addr.get_entries().size());
    EXPECT_EQ("unix", addr.get_entries()[0]->get_method());
    EXPECT_EQ("/tmp/dbus test", addr.get_entries()[0]->get_value("path"));

    // 测试键名验证
    EXPECT_THROW(address::parse("unix:invalid-key=value"), mc::parse_error_exception);
    EXPECT_THROW(address::parse("unix:123invalid=value"), mc::parse_error_exception);
    EXPECT_THROW(address::parse("unix:key with space=value"), mc::parse_error_exception);

    // 测试其他异常情况
    EXPECT_THROW(address::parse(""), mc::parse_error_exception);                       // 空字符串
    EXPECT_THROW(address::parse("unix"), mc::parse_error_exception);                   // 缺少冒号
    EXPECT_THROW(address::parse("unix:"), mc::parse_error_exception);                  // 没有键值对
    EXPECT_THROW(address::parse("unix:path"), mc::parse_error_exception);              // 缺少等号
    EXPECT_THROW(address::parse("unix:=value"), mc::parse_error_exception);            // 键为空
    EXPECT_THROW(address::parse("unix:path=/tmp;invalid"), mc::parse_error_exception); // 无效分隔符
}

TEST(AddressTest, ToStringAndRoundTrip) {
    // 测试to_string方法
    std::string addr_str     = "unix:path=/tmp/dbus-test;tcp:host=localhost,port=1234";
    address     addr         = address::parse(addr_str);
    std::string new_addr_str = addr.to_string();

    // 解析新生成的地址字符串
    address new_addr = address::parse(new_addr_str);

    // 比较两个地址对象
    ASSERT_EQ(addr.get_entries().size(), new_addr.get_entries().size());
    for (size_t i = 0; i < addr.get_entries().size(); i++) {
        EXPECT_EQ(addr.get_entries()[i]->get_method(), new_addr.get_entries()[i]->get_method());
        EXPECT_EQ(addr.get_entries()[i]->get_values().size(),
                  new_addr.get_entries()[i]->get_values().size());

        for (const auto& [key, value] : addr.get_entries()[i]->get_values()) {
            EXPECT_TRUE(new_addr.get_entries()[i]->has_key(key));
            EXPECT_EQ(value, new_addr.get_entries()[i]->get_value(key));
        }
    }
}