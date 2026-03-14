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

/**
 * @file test_local_msg.cpp
 * @brief 测试 local_msg 类
 */
#include <dbus/dbus.h>
#include <mc/dbus/shm/local_msg.h>
#include <mc/dbus/shm/serialize.h>
#include <mc/dict.h>
#include <mc/exception.h>
#include <mc/future.h>

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

using namespace mc;
using namespace mc::dbus;

class LocalMsgTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // 在每个测试前执行
        sample_msg =
            new dbus::local_msg("bmc.kepler.test", "/bmc/kepler/test/obj", "bmc.kepler.test.intf", "TestMethod");
    }

    void TearDown() override
    {
        // 在每个测试后执行
    }

    dbus::local_msg* sample_msg;
};

// 测试 local_call 标记
TEST_F(LocalMsgTest, LocalCallFlag)
{
    EXPECT_FALSE(sample_msg->is_local_call());
    sample_msg->set_local_call(true);
    EXPECT_TRUE(sample_msg->is_local_call());
    sample_msg->set_local_call(false);
    EXPECT_FALSE(sample_msg->is_local_call());
}

// 测试错误信息
TEST_F(LocalMsgTest, ErrorInfo)
{
    sample_msg->error("InternalError", "this is an error message");
    auto [error_name, error_message] = sample_msg->get_error();
    ASSERT_EQ(error_name, "InternalError");
    ASSERT_EQ(error_message, "this is an error message");
}

// 测试 append 方法
TEST_F(LocalMsgTest, Append)
{
    auto     msg = new dbus::local_msg("bmc.kepler.test", "/bmc/kepler/test/obj", "bmc.kepler.test.intf", "TestMethod");
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
TEST_F(LocalMsgTest, Pack)
{
    auto msg = new dbus::local_msg("bmc.kepler.test", "/bmc/kepler/test/obj", "bmc.kepler.test.intf", "TestMethod");
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
TEST_F(LocalMsgTest, AppendArgs)
{
    auto     msg = new dbus::local_msg("bmc.kepler.test", "/bmc/kepler/test/obj", "bmc.kepler.test.intf", "TestMethod");
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
TEST_F(LocalMsgTest, NewDBusMsg)
{
    auto reply_msg =
        new dbus::local_msg("bmc.kepler.test", "/bmc/kepler/test/obj", "bmc.kepler.test.intf", "TestMethod");
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
TEST_F(LocalMsgTest, ConstructFromVariants)
{
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

TEST_F(LocalMsgTest, ConstructFromVariantsEdgeCases)
{
    variants        empty_v;
    dbus::local_msg msg1(empty_v);
    EXPECT_EQ(msg1.msg_type(), DBUS_MESSAGE_TYPE_METHOD_CALL);
    EXPECT_TRUE(msg1.destination().empty());
    EXPECT_TRUE(msg1.path().empty());
    EXPECT_TRUE(msg1.interface().empty());
    EXPECT_TRUE(msg1.member().empty());
    EXPECT_TRUE(msg1.signature().empty());
    EXPECT_TRUE(msg1.read().empty());
    EXPECT_EQ(msg1.get_serial(), 0U);
    EXPECT_EQ(msg1.get_reply_serial(), 0U);
    EXPECT_FALSE(msg1.is_local_call());
    auto [default_error_name, default_error_msg] = msg1.get_error();
    EXPECT_TRUE(default_error_name.empty());
    EXPECT_TRUE(default_error_msg.empty());

    variants partial_v;
    partial_v.push_back(static_cast<uint32_t>(DBUS_MESSAGE_TYPE_SIGNAL));
    dbus::local_msg msg2(partial_v);
    EXPECT_EQ(msg2.msg_type(), DBUS_MESSAGE_TYPE_SIGNAL);
    EXPECT_TRUE(msg2.destination().empty());
    EXPECT_TRUE(msg2.path().empty());
    EXPECT_TRUE(msg2.interface().empty());
    EXPECT_TRUE(msg2.member().empty());
    EXPECT_TRUE(msg2.signature().empty());
    EXPECT_TRUE(msg2.read().empty());
    EXPECT_EQ(msg2.get_serial(), 0U);
    EXPECT_EQ(msg2.get_reply_serial(), 0U);
    EXPECT_FALSE(msg2.is_local_call());
}

TEST_F(LocalMsgTest, AppendReturnArgsCases)
{
    auto msg1 = std::make_unique<dbus::local_msg>("org.test", "/org/test", "org.test", "Method");
    msg1->append_return_args("i", variant(42));
    EXPECT_EQ(msg1->signature(), "i");
    const auto& msg1_args = msg1->read();
    ASSERT_EQ(msg1_args.size(), 1U);
    EXPECT_EQ(msg1_args[0].as_int32(), 42);

    auto msg2 = std::make_unique<dbus::local_msg>("org.test", "/org/test", "org.test", "Method");
    msg2->append_return_args("isd", variant(variants{1, "test", 3.14}));
    EXPECT_EQ(msg2->signature(), "isd");
    const auto& msg2_args = msg2->read();
    ASSERT_EQ(msg2_args.size(), 3U);
    EXPECT_EQ(msg2_args[0].as_int32(), 1);
    EXPECT_EQ(msg2_args[1].as_string(), "test");
    EXPECT_DOUBLE_EQ(msg2_args[2].as_double(), 3.14);

    auto msg3 = std::make_unique<dbus::local_msg>("org.test", "/org/test", "org.test", "Method");
    msg3->append_return_args("", variant());
    EXPECT_TRUE(msg3->signature().empty());
    EXPECT_TRUE(msg3->read().empty());
}

TEST_F(LocalMsgTest, ParseVariantVariousTypes)
{
    // 验证基础整数类型
    const auto byte_result =
        dbus::local_msg::parse_variant(signature_iterator("y"), variant(static_cast<uint8_t>(65)), 0);
    EXPECT_TRUE(byte_result.is_uint8());
    EXPECT_EQ(byte_result.as_uint8(), 65);

    const auto bool_true_result = dbus::local_msg::parse_variant(signature_iterator("b"), variant(true), 0);
    EXPECT_TRUE(bool_true_result.is_bool());
    EXPECT_TRUE(bool_true_result.as_bool());

    // 验证数字解析为布尔值的行为（非零为 true，零为 false）
    const auto bool_numeric_true_result = dbus::local_msg::parse_variant(signature_iterator("b"), variant(1), 0);
    EXPECT_TRUE(bool_numeric_true_result.is_bool());
    EXPECT_TRUE(bool_numeric_true_result.as_bool());

    const auto bool_numeric_false_result = dbus::local_msg::parse_variant(signature_iterator("b"), variant(0), 0);
    EXPECT_TRUE(bool_numeric_false_result.is_bool());
    EXPECT_FALSE(bool_numeric_false_result.as_bool());

    const auto int16_result =
        dbus::local_msg::parse_variant(signature_iterator("n"), variant(static_cast<int16_t>(-123)), 0);
    EXPECT_TRUE(int16_result.is_int16());
    EXPECT_EQ(int16_result.as_int16(), -123);

    const auto uint16_result =
        dbus::local_msg::parse_variant(signature_iterator("q"), variant(static_cast<uint16_t>(12345)), 0);
    EXPECT_TRUE(uint16_result.is_uint16());
    EXPECT_EQ(uint16_result.as_uint16(), 12345);

    const auto int32_result =
        dbus::local_msg::parse_variant(signature_iterator("i"), variant(static_cast<int32_t>(-123456)), 0);
    EXPECT_TRUE(int32_result.is_int32());
    EXPECT_EQ(int32_result.as_int32(), -123456);

    const auto uint32_result =
        dbus::local_msg::parse_variant(signature_iterator("u"), variant(static_cast<uint32_t>(123456)), 0);
    EXPECT_TRUE(uint32_result.is_uint32());
    EXPECT_EQ(uint32_result.as_uint32(), 123456U);

    const auto int64_result =
        dbus::local_msg::parse_variant(signature_iterator("x"), variant(static_cast<int64_t>(-123456789)), 0);
    EXPECT_TRUE(int64_result.is_int64());
    EXPECT_EQ(int64_result.as_int64(), -123456789);

    const auto uint64_result =
        dbus::local_msg::parse_variant(signature_iterator("t"), variant(static_cast<uint64_t>(123456789)), 0);
    EXPECT_TRUE(uint64_result.is_uint64());
    EXPECT_EQ(uint64_result.as_uint64(), 123456789U);

    const auto double_result = dbus::local_msg::parse_variant(signature_iterator("d"), variant(3.14159), 0);
    EXPECT_TRUE(double_result.is_double());
    EXPECT_NEAR(double_result.as_double(), 3.14159, 1e-9);
}

TEST_F(LocalMsgTest, AppendArgsSignatureParseError)
{
    auto     msg = new dbus::local_msg("org.test", "/org/test", "org.test", "Method");
    variants args;
    args.push_back(1);
    args.push_back("test");
    EXPECT_THROW(msg->append_args("invalid", args), std::exception);
}

// ========== 安全性测试 ==========

TEST_F(LocalMsgTest, AppendArgsSizeMismatch)
{
    auto     msg = new dbus::local_msg("org.test", "/org/test", "org.test", "Method");
    variants args;
    args.push_back(1);
    EXPECT_THROW(msg->append_args("ii", args), std::exception);
    delete msg;
}

TEST_F(LocalMsgTest, NewDBusMsgInvalidType)
{
    auto     msg = new dbus::local_msg("org.test", "/org/test", "org.test", "Method");
    variants v;
    v.push_back(static_cast<uint32_t>(999));
    v.push_back("org.test");
    v.push_back("/org/test");
    v.push_back("org.test");
    v.push_back("Method");
    v.push_back("NA");
    v.push_back("");
    v.push_back(variants());
    v.push_back(":1.100");
    v.push_back(static_cast<uint32_t>(0));
    v.push_back(false);
    v.push_back(static_cast<uint32_t>(0));
    dbus::local_msg invalid_msg(v);
    EXPECT_THROW(invalid_msg.new_dbus_msg(), mc::invalid_arg_exception);
    delete msg;
}

TEST_F(LocalMsgTest, AppendReturnArgsMultipleNonArray)
{
    auto    msg      = new dbus::local_msg("org.test", "/org/test", "org.test", "Method");
    variant v_single = 42;
    EXPECT_THROW(msg->append_return_args("isd", v_single), std::exception);
    delete msg;
}

TEST_F(LocalMsgTest, SetterAndVariantParsingHelpers)
{
    sample_msg->set_member("NewMethod");
    EXPECT_EQ(sample_msg->member(), "NewMethod");

    sample_msg->set_sender(":1.200");
    EXPECT_EQ(sample_msg->sender(), ":1.200");

    auto path_variant = dbus::local_msg::parse_variant(signature_iterator("o"), variant("/org/test"), 0);
    EXPECT_EQ(path_variant.as_string(), "/org/test");

    auto signature_variant = dbus::local_msg::parse_variant(signature_iterator("g"), variant("a{ss}"), 0);
    EXPECT_EQ(signature_variant.as_string(), "a{ss}");
}

TEST_F(LocalMsgTest, ConstructFromVariantsParseFallback)
{
    variants v;
    v.push_back(static_cast<uint32_t>(DBUS_MESSAGE_TYPE_METHOD_CALL));
    v.push_back("org.test");
    v.push_back("/org/test");
    v.push_back("org.test.Interface");
    v.push_back("Method");
    v.push_back("NA");
    v.push_back("i");
    // 提供与签名不匹配的参数，触发解析异常并落回原始参数
    v.push_back(variants{"not-int"});
    v.push_back(":1.10");
    v.push_back(static_cast<uint32_t>(1));
    v.push_back(false);
    v.push_back(static_cast<uint32_t>(0));

    dbus::local_msg msg(v);
    auto            args = msg.read();
    ASSERT_EQ(args.size(), 1);
    ASSERT_TRUE(args[0].is_string());
    ASSERT_EQ(args[0].as_string(), "not-int");
}

TEST_F(LocalMsgTest, ScenarioConcurrentPackUnpack)
{
    const int                num_threads = 10;
    const int                iterations  = 100;
    std::atomic<int>         success_count{0};
    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([i, &success_count]() {
            for (int j = 0; j < iterations; ++j) {
                auto msg = new dbus::local_msg("org.test", "/org/test", "org.test", "Method");
                msg->set_serial(i * iterations + j);
                msg->set_sender(":1.100");

                variants args;
                args.push_back(i);
                args.push_back(j);
                msg->append_args("ii", args);

                auto            packed   = msg->pack();
                auto            unpacked = dbus::serialize::unpack(packed);
                dbus::local_msg restored_msg(unpacked);

                auto read_args = restored_msg.read();
                if (read_args.size() == 2 && read_args[0].as_int32() == i && read_args[1].as_int32() == j &&
                    restored_msg.get_serial() == i * iterations + j && restored_msg.sender() == ":1.100") {
                    success_count.fetch_add(1);
                }
                delete msg;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count.load(), num_threads * iterations);
}

TEST_F(LocalMsgTest, ScenarioConcurrentConstructFromVariants)
{
    const int                num_threads = 5;
    const int                iterations  = 50;
    std::atomic<int>         success_count{0};
    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([i, &success_count]() {
            for (int j = 0; j < iterations; ++j) {
                variants v;
                v.push_back(static_cast<uint32_t>(DBUS_MESSAGE_TYPE_METHOD_CALL));
                v.push_back("org.test");
                v.push_back("/org/test");
                v.push_back("org.test");
                v.push_back("Method");
                v.push_back("NA");
                v.push_back("ii");
                v.push_back(variants{i, j});
                v.push_back(":1.100");
                v.push_back(static_cast<uint32_t>(i * iterations + j));
                v.push_back(false);
                v.push_back(static_cast<uint32_t>(0));

                dbus::local_msg msg(v);
                auto            packed   = msg.pack();
                auto            unpacked = dbus::serialize::unpack(packed);
                dbus::local_msg restored_msg(unpacked);

                auto args = restored_msg.read();
                if (args.size() == 2 && args[0].as_int32() == i && args[1].as_int32() == j) {
                    success_count.fetch_add(1);
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count.load(), num_threads * iterations);
}

TEST_F(LocalMsgTest, ScenarioLargeMessagePackUnpack)
{
    auto msg = new dbus::local_msg("org.test", "/org/test", "org.test", "Method");

    variants large_args;
    for (int i = 0; i < 1000; ++i) {
        large_args.push_back(i);
    }
    msg->append("ai", large_args);

    auto packed = msg->pack();
    EXPECT_FALSE(packed.empty());

    auto            unpacked = dbus::serialize::unpack(packed);
    dbus::local_msg restored_msg(unpacked);

    auto read_args = restored_msg.read();
    EXPECT_EQ(read_args.size(), 1);
    EXPECT_TRUE(read_args[0].is_array());
    EXPECT_EQ(read_args[0].as_array().size(), 1000);
    delete msg;
}

// ========== shm_pending_msgs 测试 ==========

TEST_F(LocalMsgTest, ShmPendingMsgsSendAndReply)
{
    shm_pending_msgs pending;

    // 发送 promise：第一次成功，重复序列号失败
    auto p1 = mc::make_promise<local_msg>();
    EXPECT_TRUE(pending.send(":1.100", 1, std::move(p1)));

    auto p2 = mc::make_promise<local_msg>();
    EXPECT_FALSE(pending.send(":1.100", 1, std::move(p2)));

    // 未注册的目的地/序列号回复应返回 false
    local_msg msg("org.test", "/org/test", "org.test", "Method");
    EXPECT_FALSE(pending.reply(":1.200", 1, msg));

    // 正确的目的地 + 序列号回复应返回 true
    auto p3 = mc::make_promise<local_msg>();
    auto f3 = p3.get_future();
    EXPECT_TRUE(pending.send(":1.300", 9, std::move(p3)));
    EXPECT_TRUE(pending.reply(":1.300", 9, msg));
    auto result = f3.get();
    EXPECT_EQ(result.destination(), "org.test");
}

TEST_F(LocalMsgTest, ShmPendingMsgsClear)
{
    shm_pending_msgs pending;

    // 发送多个 promise
    auto p1 = mc::make_promise<local_msg>();
    EXPECT_TRUE(pending.send(":1.100", 1, std::move(p1)));

    auto p2 = mc::make_promise<local_msg>();
    EXPECT_TRUE(pending.send(":1.100", 2, std::move(p2)));

    auto p3 = mc::make_promise<local_msg>();
    EXPECT_TRUE(pending.send(":1.200", 1, std::move(p3)));

    // 清除 :1.100 的所有 pending 消息
    pending.clear(":1.100");

    // :1.100 的序列号应该无法回复
    local_msg msg("org.test", "/org/test", "org.test", "Method");
    EXPECT_FALSE(pending.reply(":1.100", 1, msg));
    EXPECT_FALSE(pending.reply(":1.100", 2, msg));

    // :1.200 的序列号应该仍然可以回复
    EXPECT_TRUE(pending.reply(":1.200", 1, msg));
}

TEST_F(LocalMsgTest, ShmPendingMsgsMultipleDestinations)
{
    shm_pending_msgs pending;

    // 为不同的目的地发送 promise
    auto p1 = mc::make_promise<local_msg>();
    EXPECT_TRUE(pending.send(":1.100", 1, std::move(p1)));

    auto p2 = mc::make_promise<local_msg>();
    EXPECT_TRUE(pending.send(":1.200", 1, std::move(p2)));

    auto p3 = mc::make_promise<local_msg>();
    EXPECT_TRUE(pending.send(":1.300", 1, std::move(p3)));

    // 每个目的地的相同序列号应该独立
    local_msg msg("org.test", "/org/test", "org.test", "Method");
    EXPECT_TRUE(pending.reply(":1.100", 1, msg));
    EXPECT_TRUE(pending.reply(":1.200", 1, msg));
    EXPECT_TRUE(pending.reply(":1.300", 1, msg));
}

TEST_F(LocalMsgTest, ShmPendingMsgsConcurrentSend)
{
    shm_pending_msgs         pending;
    const int                num_threads = 10;
    const int                iterations  = 20;
    std::atomic<int>         success_count{0};
    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([i, &pending, &success_count]() {
            for (int j = 0; j < iterations; ++j) {
                auto     p      = mc::make_promise<local_msg>();
                uint32_t serial = i * iterations + j + 1;
                if (pending.send(":1.100", serial, std::move(p))) {
                    success_count.fetch_add(1);
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count.load(), num_threads * iterations);
}

// 测试 is_unique_name() 空字符串情况
TEST_F(LocalMsgTest, IsUniqueNameEmptyString)
{
    EXPECT_FALSE(is_unique_name(""));
    EXPECT_FALSE(is_unique_name("not_unique"));
    EXPECT_TRUE(is_unique_name(":1.23"));
}

// 测试 local_msg::append() 所有类型
TEST_F(LocalMsgTest, LocalMsgAppendAllTypes)
{
    auto msg = new dbus::local_msg("bmc.kepler.test", "/bmc/kepler/test/obj", "bmc.kepler.test.intf", "TestMethod");

    // 测试所有支持的类型
    int8_t   i8  = -10;
    int16_t  i16 = -1000;
    int32_t  i32 = -100000;
    int64_t  i64 = -10000000000LL;
    uint8_t  u8  = 10;
    uint16_t u16 = 1000;
    uint32_t u32 = 100000;
    uint64_t u64 = 10000000000ULL;
    float    f   = 3.14f;
    double   d   = 3.14159;

    msg->append("ynqiuxtdgs", i8, u8, i16, u16, i32, u32, i64, u64, f, d);
    auto args = msg->read();
    ASSERT_EQ(args.size(), 10);
    EXPECT_EQ(args[0].as_int8(), i8);
    EXPECT_EQ(args[1].as_uint8(), u8);
    EXPECT_EQ(args[2].as_int16(), i16);
    EXPECT_EQ(args[3].as_uint16(), u16);
    EXPECT_EQ(args[4].as_int32(), i32);
    EXPECT_EQ(args[5].as_uint32(), u32);
    EXPECT_EQ(args[6].as_int64(), i64);
    EXPECT_EQ(args[7].as_uint64(), u64);
    EXPECT_FLOAT_EQ(static_cast<float>(args[8].as_double()), f);
    EXPECT_DOUBLE_EQ(args[9].as_double(), d);
}

// 测试 local_msg::append() 签名不匹配
TEST_F(LocalMsgTest, LocalMsgAppendInvalidSignature)
{
    auto msg = new dbus::local_msg("bmc.kepler.test", "/bmc/kepler/test/obj", "bmc.kepler.test.intf", "TestMethod");

    // 传入签名与参数数量不匹配的参数，应该抛出异常
    EXPECT_THROW(msg->append("ii", 1, 2, 3), mc::exception); // 签名只有2个参数，但传入了3个
    EXPECT_THROW(msg->append("iii", 1, 2), mc::exception);   // 签名有3个参数，但只传入了2个
}
