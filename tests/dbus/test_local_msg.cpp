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

// 测试 pack 方法
TEST_F(LocalMsgTest, Pack) {
    auto msg = new dbus::local_msg("bmc.kepler.test", "/bmc/kepler/test/obj",
                                   "bmc.kepler.test.intf", "TestMethod");
    msg->append("a{ss}s", dict({{"a", "str1"}, {"b", "str2"}}), "test");
    auto packed   = msg->pack();
    auto unpacked = dbus::serialize::unpack(packed);
    ASSERT_EQ(unpacked.size(), 12);
    ASSERT_TRUE(unpacked[7].is_array());
    auto args = unpacked[7].as_array();
    ASSERT_EQ(args.size(), 2);
    ASSERT_EQ(args[0].as_dict()["a"].as_string(), "str1");
    ASSERT_EQ(args[0].as_dict()["b"].as_string(), "str2");
    ASSERT_EQ(args[1].as_string(), "test");

    variant arg1 = "str1";
    variant arg2 = variants{variants{257, false, 3, "abc"}, variants{88, true, 4, "def"}};
    variant arg3 = 33;
    variant arg4 = dict({{"a", dict({{"b", "str2"}, {"c", 33}})}});
    variant arg5 = variants{97, 98, 99, 100}; // abcd
    variant arg6 = 3.14;
    msg->append("sa(ibys)ua{sa{sv}}ayd", arg1, arg2, arg3, arg4, arg5, arg6);
    packed   = msg->pack();
    unpacked = dbus::serialize::unpack(packed);
    ASSERT_EQ(unpacked.size(), 12);
    ASSERT_TRUE(unpacked[7].is_array());
    args = unpacked[7].as_array();
    ASSERT_EQ(args.size(), 6);
    ASSERT_EQ(args[0].as_string(), "str1");
    auto unpacked_arg2 = args[1].as_array();
    ASSERT_EQ(unpacked_arg2.size(), 2);
    auto unpacked_arg2_1 = unpacked_arg2[0].as_array();
    ASSERT_EQ(unpacked_arg2_1.size(), 4);
    ASSERT_EQ(unpacked_arg2_1[0].as_int32(), 257);
    ASSERT_EQ(unpacked_arg2_1[1].as_bool(), false);
    ASSERT_EQ(unpacked_arg2_1[2].as_int32(), 3);
    ASSERT_EQ(unpacked_arg2_1[3].as_string(), "abc");
    auto unpacked_arg2_2 = unpacked_arg2[1].as_array();
    ASSERT_EQ(unpacked_arg2_2.size(), 4);
    ASSERT_EQ(unpacked_arg2_2[0].as_int32(), 88);
    ASSERT_EQ(unpacked_arg2_2[1].as_bool(), true);
    ASSERT_EQ(unpacked_arg2_2[2].as_int32(), 4);
    ASSERT_EQ(unpacked_arg2_2[3].as_string(), "def");
    ASSERT_EQ(args[2].as_int32(), 33);
    auto unpacked_arg4 = args[3].as_dict();
    ASSERT_EQ(unpacked_arg4["a"].as_dict()["b"].as_string(), "str2");
    ASSERT_EQ(unpacked_arg4["a"].as_dict()["c"].as_int32(), 33);
    ASSERT_TRUE(args[4].is_string());
    auto unpacked_arg5 = args[4].as_string();
    ASSERT_EQ(unpacked_arg5, "abcd");
    ASSERT_TRUE(args[5].is_double());
    auto unpacked_arg6 = args[5].as_double();
    ASSERT_EQ(unpacked_arg6, 3.14);
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
    v.push_back("sisda{ss}");
    v.push_back(variants({"test error msg", 1, "str2", 3.3, variants()}));
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
    ASSERT_EQ(msg->signature(), "sisda{ss}");
    auto args = msg->read();
    ASSERT_EQ(args.size(), 5);
    ASSERT_EQ(args[0].as_string(), "test error msg");
    ASSERT_EQ(args[1].as_int32(), 1);
    ASSERT_EQ(args[2].as_string(), "str2");
    ASSERT_EQ(args[3].as_double(), 3.3);
    ASSERT_TRUE(args[4].is_dict());
    ASSERT_EQ(args[4].as_dict().size(), 0);
    ASSERT_EQ(msg->sender(), ":1.23");
    ASSERT_EQ(msg->get_serial(), 45);
    ASSERT_EQ(msg->is_local_call(), true);
    ASSERT_EQ(msg->get_reply_serial(), 67);
}