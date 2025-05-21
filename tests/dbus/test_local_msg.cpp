/*
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

/**
 * @file test_local_msg.cpp
 * @brief 测试 local_msg 类
 */
#include <dbus/dbus.h>
#include <gtest/gtest.h>
#include <mc/dbus/shm/local_msg.h>
#include <mc/dbus/shm/serialize.h>
#include <mc/dict.h>

using namespace mc;

class LocalMsgTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 在每个测试前执行
        sample_msg = new dbus::local_msg("bmc.kepler.test", "/bmc/kepler/test/obj",
                                         "bmc.kepler.test.intf", "TestMethod");
    }

    void TearDown() override {
        // 在每个测试后执行
    }

    dbus::local_msg* sample_msg;
};

// 测试 local_call 标记
TEST_F(LocalMsgTest, LocalCallFlag) {
    EXPECT_FALSE(sample_msg->is_local_call());
    sample_msg->set_local_call(true);
    EXPECT_TRUE(sample_msg->is_local_call());
    sample_msg->set_local_call(false);
    EXPECT_FALSE(sample_msg->is_local_call());
}

// 测试错误信息
TEST_F(LocalMsgTest, ErrorInfo) {
    sample_msg->error("InternalError", "this is an error message");
    auto [error_name, error_message] = sample_msg->get_error();
    ASSERT_EQ(error_name, "InternalError");
    ASSERT_EQ(error_message, "this is an error message");
}

// 测试转换为 variants
TEST_F(LocalMsgTest, ToVariants) {
    sample_msg->set_local_call(true);
    sample_msg->set_sender(":1.23");
    sample_msg->set_serial(123);
    sample_msg->append("ai", variants({123, 456}));
    auto v = sample_msg->to_variants();
    ASSERT_EQ(v.size(), 12);
    ASSERT_EQ(v[0].as_uint32(), DBUS_MESSAGE_TYPE_METHOD_CALL);
    ASSERT_EQ(v[1].as_string(), "bmc.kepler.test");
    ASSERT_EQ(v[2].as_string(), "/bmc/kepler/test/obj");
    ASSERT_EQ(v[3].as_string(), "bmc.kepler.test.intf");
    ASSERT_EQ(v[4].as_string(), "TestMethod");

    auto args = v[7].as_array();
    ASSERT_EQ(args.size(), 1);
    auto arg = args[0].as_array();
    ASSERT_EQ(arg.size(), 2);
    ASSERT_EQ(arg[0].as_int32(), 123);
    ASSERT_EQ(arg[1].as_int32(), 456);

    ASSERT_EQ(v[8].as_string(), ":1.23");
    ASSERT_EQ(v[9].as_uint32(), 123);
    ASSERT_EQ(v[10].as_bool(), true);
    ASSERT_EQ(v[11].as_uint32(), 0);
}

// 测试 append 方法
TEST_F(LocalMsgTest, Append) {
    auto     msg = new dbus::local_msg("bmc.kepler.test", "/bmc/kepler/test/obj",
                                       "bmc.kepler.test.intf", "TestMethod");
    variants arg1;
    arg1.push_back(1);
    arg1.push_back(-2);
    dict    arg2({{"a", 3}, {"b", 4}});
    variant arg3 = 5;
    msg->append("aia{si}u", arg1, arg2, arg3);
    auto args = msg->read();
    ASSERT_EQ(args.size(), 3);
    ASSERT_EQ(args[0].as_array()[0].as_int32(), 1);
    ASSERT_EQ(args[0].as_array()[1].as_int32(), -2);
    ASSERT_EQ(args[1].as_dict()["a"].as_int32(), 3);
    ASSERT_EQ(args[1].as_dict()["b"].as_int32(), 4);
    ASSERT_EQ(args[2].as_uint32(), 5);
}

// 测试 append_args 方法
TEST_F(LocalMsgTest, AppendArgs) {
    auto     msg = new dbus::local_msg("bmc.kepler.test", "/bmc/kepler/test/obj",
                                       "bmc.kepler.test.intf", "TestMethod");
    dict     arg1({{"a", "str1"}, {"b", "str2"}});
    variants arg2;
    arg2.push_back(12);
    arg2.push_back(34);
    variants input_args;
    input_args.push_back(arg1);
    input_args.push_back(arg2);
    msg->append_args("a{ss}ay", input_args);
    auto args = msg->read();
    ASSERT_EQ(args.size(), 2);
    ASSERT_EQ(args[0].as_dict()["a"].as_string(), "str1");
    ASSERT_EQ(args[0].as_dict()["b"].as_string(), "str2");
    ASSERT_EQ(args[1].as_array()[0].as_uint8(), 12);
    ASSERT_EQ(args[1].as_array()[1].as_uint8(), 34);
}

// 测试 new_dbus_msg 方法
TEST_F(LocalMsgTest, NewDBusMsg) {
    auto reply_msg = new dbus::local_msg("bmc.kepler.test", "/bmc/kepler/test/obj",
                                         "bmc.kepler.test.intf", "TestMethod");
    reply_msg->set_sender(":1.23");
    reply_msg->set_serial(123);
    reply_msg->method_return();
    dict out_arg({{"a", "str1"}, {"b", "str2"}});
    reply_msg->append("a{ss}", out_arg);
    auto rsp = reply_msg->new_dbus_msg();
    ASSERT_EQ(rsp.get_type(), dbus::message_type::method_return);
    auto args = rsp.read_args();
    ASSERT_EQ(args.size(), 1);
    ASSERT_EQ(args[0].as_dict()["a"].as_string(), "str1");
    ASSERT_EQ(args[0].as_dict()["b"].as_string(), "str2");
}

// 测试从 variants 构造 local_msg
TEST_F(LocalMsgTest, ConstructFromVariants) {
    variants v;
    v.push_back(static_cast<uint32_t>(DBUS_MESSAGE_TYPE_METHOD_RETURN));
    v.push_back("bmc.kepler.test1");
    v.push_back("/bmc/kepler/test/obj1");
    v.push_back("bmc.kepler.test.intf1");
    v.push_back("TestMethod1");
    v.push_back("TestErrorName");
    v.push_back("a{ss}");
    v.push_back(variants({"test error msg", 1, "str2", 3.3}));
    v.push_back(":1.23");
    v.push_back(static_cast<uint32_t>(45));
    v.push_back(true);
    v.push_back(static_cast<uint32_t>(67));

    auto msg = new dbus::local_msg(v);
    ASSERT_EQ(msg->msg_type(), DBUS_MESSAGE_TYPE_METHOD_RETURN);
    ASSERT_EQ(msg->destination(), "bmc.kepler.test1");
    ASSERT_EQ(msg->path(), "/bmc/kepler/test/obj1");
    ASSERT_EQ(msg->interface(), "bmc.kepler.test.intf1");
    ASSERT_EQ(msg->member(), "TestMethod1");
    auto [error_name, error_message] = msg->get_error();
    ASSERT_EQ(error_name, "TestErrorName");
    ASSERT_EQ(error_message, "test error msg");
    ASSERT_EQ(msg->signature(), "a{ss}");
    auto args = msg->read();
    ASSERT_EQ(args.size(), 4);
    ASSERT_EQ(args[0].as_string(), "test error msg");
    ASSERT_EQ(args[1].as_int32(), 1);
    ASSERT_EQ(args[2].as_string(), "str2");
    ASSERT_EQ(args[3].as_double(), 3.3);
    ASSERT_EQ(msg->sender(), ":1.23");
    ASSERT_EQ(msg->get_serial(), 45);
    ASSERT_EQ(msg->is_local_call(), true);
    ASSERT_EQ(msg->get_reply_serial(), 67);
}