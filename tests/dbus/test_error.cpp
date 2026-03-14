/*
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2. You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY
 * KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
 * NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE. See the
 * Mulan PSL v2 for more details.
 */

#include <mc/dbus/error.h>
#include <mc/dict.h>

#include <gtest/gtest.h>

using namespace mc::dbus;

// 测试默认构造
TEST(ErrorTest, DefaultConstructor)
{
    error err;
    EXPECT_FALSE(err.is_set());
}

// 测试复制构造
TEST(ErrorTest, CopyConstructor)
{
    error err1;
    err1.set_error("org.freedesktop.DBus.Error.Failed", "Test error message");

    error err2(err1);
    EXPECT_TRUE(err2.is_set());
    EXPECT_STREQ(err2.name, err1.name);
    EXPECT_STREQ(err2.message, err1.message);
}

// 测试复制构造 - 源对象未设置错误
TEST(ErrorTest, CopyConstructorUnset)
{
    error err1; // 未设置错误
    error err2(err1);
    EXPECT_FALSE(err2.is_set());
}

// 测试复制赋值
TEST(ErrorTest, CopyAssignment)
{
    error err1;
    err1.set_error("org.freedesktop.DBus.Error.Failed", "Test error message");

    error err2;
    err2 = err1;
    EXPECT_TRUE(err2.is_set());
    EXPECT_STREQ(err2.name, err1.name);
    EXPECT_STREQ(err2.message, err1.message);
}

// 测试复制赋值 - 源对象未设置错误
TEST(ErrorTest, CopyAssignmentUnset)
{
    error err1; // 未设置错误
    error err2;
    err2.set_error("org.freedesktop.DBus.Error.Failed", "Test error message");
    err2 = err1; // 从未设置的对象赋值
    EXPECT_FALSE(err2.is_set());
}

// 测试移动赋值
TEST(ErrorTest, MoveAssignment)
{
    error err1;
    err1.set_error("org.freedesktop.DBus.Error.Failed", "Test error message");

    // 确保目标对象是未设置的（默认构造已初始化）
    error err2; // 默认构造，确保未设置错误
    err2 = std::move(err1);
    EXPECT_TRUE(err2.is_set());
    EXPECT_STREQ(err2.name, "org.freedesktop.DBus.Error.Failed");
    EXPECT_STREQ(err2.message, "Test error message");

    // 移动后原对象应该不再设置错误
    EXPECT_FALSE(err1.is_set());
}

// 测试移动构造
TEST(ErrorTest, MoveConstructor)
{
    error err1;
    err1.set_error("org.freedesktop.DBus.Error.Failed", "Test error message");

    error err2(std::move(err1));
    EXPECT_TRUE(err2.is_set());
    EXPECT_STREQ(err2.name, "org.freedesktop.DBus.Error.Failed");
    EXPECT_STREQ(err2.message, "Test error message");

    // 移动后原对象应该不再设置错误
    EXPECT_FALSE(err1.is_set());
}

// 测试 set_error (字符串视图版本)
TEST(ErrorTest, SetErrorStringView)
{
    error err;
    err.set_error("org.freedesktop.DBus.Error.Failed", "Test error message");

    EXPECT_TRUE(err.is_set());
    EXPECT_STREQ(err.name, "org.freedesktop.DBus.Error.Failed");
    EXPECT_STREQ(err.message, "Test error message");
}

// 测试 set_error (带格式化参数版本)
TEST(ErrorTest, SetErrorWithDict)
{
    error    err;
    mc::dict args;
    args["param"] = "value";
    args["count"] = 42;

    err.set_error("org.freedesktop.DBus.Error.Failed", "Error: ${param} = ${count}", args);

    EXPECT_TRUE(err.is_set());
    EXPECT_STREQ(err.name, "org.freedesktop.DBus.Error.Failed");
    EXPECT_NE(std::string(err.message).find("value"), std::string::npos);
    EXPECT_NE(std::string(err.message).find("42"), std::string::npos);
}

// 测试 set_error (空字典)
TEST(ErrorTest, SetErrorWithEmptyDict)
{
    error    err;
    mc::dict args;

    err.set_error("org.freedesktop.DBus.Error.Failed", "Test error message", args);

    EXPECT_TRUE(err.is_set());
    EXPECT_STREQ(err.name, "org.freedesktop.DBus.Error.Failed");
    EXPECT_STREQ(err.message, "Test error message");
}

// 测试 set_error_const
TEST(ErrorTest, SetErrorConst)
{
    error       err;
    const char* name    = "org.freedesktop.DBus.Error.Failed";
    const char* message = "Test error message";

    err.set_error_const(name, message);

    EXPECT_TRUE(err.is_set());
    EXPECT_STREQ(err.name, name);
    EXPECT_STREQ(err.message, message);
}

// 测试错误名称常量
TEST(ErrorTest, ErrorNames)
{
    EXPECT_EQ(std::string(error_names::failed), "org.freedesktop.DBus.Error.Failed");
    EXPECT_EQ(std::string(error_names::no_memory), "org.freedesktop.DBus.Error.NoMemory");
    EXPECT_EQ(std::string(error_names::service_unknown), "org.freedesktop.DBus.Error.ServiceUnknown");
    EXPECT_EQ(std::string(error_names::name_has_no_owner), "org.freedesktop.DBus.Error.NameHasNoOwner");
    EXPECT_EQ(std::string(error_names::no_reply), "org.freedesktop.DBus.Error.NoReply");
    EXPECT_EQ(std::string(error_names::io_error), "org.freedesktop.DBus.Error.IOError");
    EXPECT_EQ(std::string(error_names::bad_address), "org.freedesktop.DBus.Error.BadAddress");
    EXPECT_EQ(std::string(error_names::not_supported), "org.freedesktop.DBus.Error.NotSupported");
    EXPECT_EQ(std::string(error_names::limits_exceeded), "org.freedesktop.DBus.Error.LimitsExceeded");
    EXPECT_EQ(std::string(error_names::access_denied), "org.freedesktop.DBus.Error.AccessDenied");
    EXPECT_EQ(std::string(error_names::auth_failed), "org.freedesktop.DBus.Error.AuthFailed");
    EXPECT_EQ(std::string(error_names::no_server), "org.freedesktop.DBus.Error.NoServer");
    EXPECT_EQ(std::string(error_names::timeout), "org.freedesktop.DBus.Error.Timeout");
    EXPECT_EQ(std::string(error_names::no_network), "org.freedesktop.DBus.Error.NoNetwork");
    EXPECT_EQ(std::string(error_names::address_in_use), "org.freedesktop.DBus.Error.AddressInUse");
    EXPECT_EQ(std::string(error_names::disconnected), "org.freedesktop.DBus.Error.Disconnected");
    EXPECT_EQ(std::string(error_names::invalid_args), "org.freedesktop.DBus.Error.InvalidArgs");
    EXPECT_EQ(std::string(error_names::file_not_found), "org.freedesktop.DBus.Error.FileNotFound");
    EXPECT_EQ(std::string(error_names::file_exists), "org.freedesktop.DBus.Error.FileExists");
    EXPECT_EQ(std::string(error_names::unknown_method), "org.freedesktop.DBus.Error.UnknownMethod");
    EXPECT_EQ(std::string(error_names::unknown_object), "org.freedesktop.DBus.Error.UnknownObject");
    EXPECT_EQ(std::string(error_names::unknown_interface), "org.freedesktop.DBus.Error.UnknownInterface");
    EXPECT_EQ(std::string(error_names::unknown_property), "org.freedesktop.DBus.Error.UnknownProperty");
    EXPECT_EQ(std::string(error_names::property_read_only), "org.freedesktop.DBus.Error.PropertyReadOnly");
}
