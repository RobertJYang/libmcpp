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
#include <gtest/gtest.h>
#include <mc/engine.h>
#include <mc/engine/micro_component.h>
#include <mc/exception.h>
#include <mc/string.h>
#include <test_utilities/test_base.h>

using namespace mc::engine;

struct test_service_1 : public mc::engine::service {
    test_service_1() : mc::engine::service("org.openubmc.test_service_1") {
    }
};

static mc::milliseconds                   call_timeout(1000);
static test_service_1*                    service_1;
static mc::dbus::connection               test_conn;
static std::map<std::string, std::string> empty_ctx;

class MicroComponentTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        service_1 = new test_service_1();
        service_1->init();
        service_1->start();
        test_conn = mc::dbus::connection::open_session_bus(mc::get_io_context());
        test_conn.start();
    }

    static void TearDownTestSuite() {
        service_1->stop();
        delete service_1;
        test_conn.disconnect();
    }
};

TEST_F(MicroComponentTest, TestMicroComponentInterface) {
    auto msg =
        mc::dbus::message::new_method_call("org.openubmc.test_service_1", "/bmc/kepler/test_service_1/MicroComponent",
                                           "bmc.kepler.MicroComponent", "HealthCheck");
    auto writer = msg.writer();
    writer << empty_ctx;
    auto reply = test_conn.send_with_reply(std::move(msg), call_timeout);
    ASSERT_TRUE(reply.is_valid() && reply.is_method_return());
    int32_t result;
    reply >> result;
    ASSERT_EQ(result, 0);

    msg    = mc::dbus::message::new_method_call("org.openubmc.test_service_1", "/bmc/kepler/test_service_1/MicroComponent",
                                                "bmc.kepler.Object.Properties", "GetWithContext");
    writer = msg.writer();
    writer << empty_ctx << "bmc.kepler.MicroComponent" << "Name";
    reply = test_conn.send_with_reply(std::move(msg), call_timeout);
    ASSERT_TRUE(reply.is_valid() && reply.is_method_return());
    mc::variant prop_value;
    reply >> prop_value;
    ASSERT_TRUE(prop_value.is_string());
    ASSERT_EQ(prop_value.as_string(), "test_service_1");

    msg    = mc::dbus::message::new_method_call("org.openubmc.test_service_1", "/bmc/kepler/test_service_1/MicroComponent",
                                                "bmc.kepler.Object.Properties", "GetWithContext");
    writer = msg.writer();
    writer << empty_ctx << "bmc.kepler.MicroComponent" << "Status";
    reply = test_conn.send_with_reply(std::move(msg), call_timeout);
    ASSERT_TRUE(reply.is_valid() && reply.is_method_return());
    reply >> prop_value;
    ASSERT_TRUE(prop_value.is_string());
    ASSERT_EQ(prop_value.as_string(), "InitCompleted");
}

TEST_F(MicroComponentTest, TestMicroComponentConfigManageInterface) {
    auto msg =
        mc::dbus::message::new_method_call("org.openubmc.test_service_1", "/bmc/kepler/test_service_1/MicroComponent",
                                           "bmc.kepler.MicroComponent.ConfigManage", "Backup");
    auto writer = msg.writer();
    writer << empty_ctx << "";
    auto reply = test_conn.send_with_reply(std::move(msg), call_timeout);
    ASSERT_TRUE(reply.is_valid() && reply.is_method_return());
    auto output = reply.read_args();
    EXPECT_EQ(output.size(), 1);
    EXPECT_TRUE(output[0].is_array());

    msg    = mc::dbus::message::new_method_call("org.openubmc.test_service_1", "/bmc/kepler/test_service_1/MicroComponent",
                                                "bmc.kepler.MicroComponent.ConfigManage", "Export");
    writer = msg.writer();
    writer << empty_ctx << "";
    reply = test_conn.send_with_reply(std::move(msg), call_timeout);
    ASSERT_TRUE(reply.is_valid() && reply.is_method_return());
    output = reply.read_args();
    EXPECT_EQ(output.size(), 1);
    EXPECT_TRUE(output[0].is_string());

    msg    = mc::dbus::message::new_method_call("org.openubmc.test_service_1", "/bmc/kepler/test_service_1/MicroComponent",
                                                "bmc.kepler.MicroComponent.ConfigManage", "Import");
    writer = msg.writer();
    writer << empty_ctx << "" << "";
    reply = test_conn.send_with_reply(std::move(msg), call_timeout);
    ASSERT_TRUE(reply.is_valid() && reply.is_method_return());
    output = reply.read_args();
    EXPECT_EQ(output.size(), 0);

    msg    = mc::dbus::message::new_method_call("org.openubmc.test_service_1", "/bmc/kepler/test_service_1/MicroComponent",
                                                "bmc.kepler.MicroComponent.ConfigManage", "Recover");
    writer = msg.writer();
    writer << empty_ctx << empty_ctx;
    reply = test_conn.send_with_reply(std::move(msg), call_timeout);
    ASSERT_TRUE(reply.is_valid() && reply.is_method_return());
    output = reply.read_args();
    EXPECT_EQ(output.size(), 0);

    msg    = mc::dbus::message::new_method_call("org.openubmc.test_service_1", "/bmc/kepler/test_service_1/MicroComponent",
                                                "bmc.kepler.MicroComponent.ConfigManage", "Verify");
    writer = msg.writer();
    writer << empty_ctx << "";
    reply = test_conn.send_with_reply(std::move(msg), call_timeout);
    ASSERT_TRUE(reply.is_valid() && reply.is_method_return());
    output = reply.read_args();
    EXPECT_EQ(output.size(), 1);
    EXPECT_TRUE(output[0].is_string());

    msg    = mc::dbus::message::new_method_call("org.openubmc.test_service_1", "/bmc/kepler/test_service_1/MicroComponent",
                                                "bmc.kepler.MicroComponent.ConfigManage", "GetPreservedConfig");
    writer = msg.writer();
    writer << empty_ctx << empty_ctx;
    reply = test_conn.send_with_reply(std::move(msg), call_timeout);
    ASSERT_TRUE(reply.is_valid() && reply.is_method_return());
    output = reply.read_args();
    EXPECT_EQ(output.size(), 1);
    EXPECT_TRUE(output[0].is_string());

    msg    = mc::dbus::message::new_method_call("org.openubmc.test_service_1", "/bmc/kepler/test_service_1/MicroComponent",
                                                "bmc.kepler.MicroComponent.ConfigManage", "GetTrustedConfig");
    writer = msg.writer();
    writer << empty_ctx << empty_ctx;
    reply = test_conn.send_with_reply(std::move(msg), call_timeout);
    ASSERT_TRUE(reply.is_valid() && reply.is_method_return());
    output = reply.read_args();
    EXPECT_EQ(output.size(), 1);
    EXPECT_TRUE(output[0].is_string());
}

TEST_F(MicroComponentTest, TestMicroComponentDebugInterface) {
    auto msg =
        mc::dbus::message::new_method_call("org.openubmc.test_service_1", "/bmc/kepler/test_service_1/MicroComponent",
                                           "bmc.kepler.MicroComponent.Debug", "AttachDebugConsole");
    auto writer = msg.writer();
    writer << empty_ctx << 40010;
    auto reply = test_conn.send_with_reply(std::move(msg), call_timeout);
    ASSERT_TRUE(reply.is_valid() && reply.is_method_return());
    auto output = reply.read_args();
    EXPECT_EQ(output.size(), 0);

    msg    = mc::dbus::message::new_method_call("org.openubmc.test_service_1", "/bmc/kepler/test_service_1/MicroComponent",
                                                "bmc.kepler.MicroComponent.Debug", "DetachDebugConsole");
    writer = msg.writer();
    writer << empty_ctx;
    reply = test_conn.send_with_reply(std::move(msg), call_timeout);
    ASSERT_TRUE(reply.is_valid() && reply.is_method_return());
    output = reply.read_args();
    EXPECT_EQ(output.size(), 0);

    msg    = mc::dbus::message::new_method_call("org.openubmc.test_service_1", "/bmc/kepler/test_service_1/MicroComponent",
                                                "bmc.kepler.MicroComponent.Debug", "Dump");
    writer = msg.writer();
    writer << empty_ctx << "";
    reply = test_conn.send_with_reply(std::move(msg), call_timeout);
    ASSERT_TRUE(reply.is_valid() && reply.is_method_return());
    output = reply.read_args();
    EXPECT_EQ(output.size(), 0);

    msg    = mc::dbus::message::new_method_call("org.openubmc.test_service_1", "/bmc/kepler/test_service_1/MicroComponent",
                                                "bmc.kepler.MicroComponent.Debug", "SetDlogLevel");
    writer = msg.writer();
    writer << empty_ctx << "debug" << 2;
    reply = test_conn.send_with_reply(std::move(msg), call_timeout);
    ASSERT_TRUE(reply.is_valid() && reply.is_method_return());
    output = reply.read_args();
    EXPECT_EQ(output.size(), 0);
}

TEST_F(MicroComponentTest, TestMicroComponentRebootInterface) {
    auto msg =
        mc::dbus::message::new_method_call("org.openubmc.test_service_1", "/bmc/kepler/test_service_1/MicroComponent",
                                           "bmc.kepler.MicroComponent.Reboot", "Prepare");
    auto writer = msg.writer();
    writer << empty_ctx;
    auto reply = test_conn.send_with_reply(std::move(msg), call_timeout);
    ASSERT_TRUE(reply.is_valid() && reply.is_method_return());
    int32_t ret_code;
    reply >> ret_code;
    ASSERT_EQ(ret_code, 0);

    msg    = mc::dbus::message::new_method_call("org.openubmc.test_service_1", "/bmc/kepler/test_service_1/MicroComponent",
                                                "bmc.kepler.MicroComponent.Reboot", "Action");
    writer = msg.writer();
    writer << empty_ctx;
    reply = test_conn.send_with_reply(std::move(msg), call_timeout);
    ASSERT_TRUE(reply.is_valid() && reply.is_method_return());
    reply >> ret_code;
    ASSERT_EQ(ret_code, 0);

    msg    = mc::dbus::message::new_method_call("org.openubmc.test_service_1", "/bmc/kepler/test_service_1/MicroComponent",
                                                "bmc.kepler.MicroComponent.Reboot", "Cancel");
    writer = msg.writer();
    writer << empty_ctx;
    reply = test_conn.send_with_reply(std::move(msg), call_timeout);
    ASSERT_TRUE(reply.is_valid() && reply.is_method_return());
    auto output = reply.read_args();
    EXPECT_EQ(output.size(), 0);
}

TEST_F(MicroComponentTest, TestMicroComponentResetInterface) {
    auto msg =
        mc::dbus::message::new_method_call("org.openubmc.test_service_1", "/bmc/kepler/test_service_1/MicroComponent",
                                           "bmc.kepler.MicroComponent.Reset", "Prepare");
    auto writer = msg.writer();
    writer << empty_ctx << "";
    auto reply = test_conn.send_with_reply(std::move(msg), call_timeout);
    ASSERT_TRUE(reply.is_valid() && reply.is_method_return());
    int32_t ret_code;
    reply >> ret_code;
    ASSERT_EQ(ret_code, 0);

    msg    = mc::dbus::message::new_method_call("org.openubmc.test_service_1", "/bmc/kepler/test_service_1/MicroComponent",
                                                "bmc.kepler.MicroComponent.Reset", "Action");
    writer = msg.writer();
    writer << empty_ctx << "";
    reply = test_conn.send_with_reply(std::move(msg), call_timeout);
    ASSERT_TRUE(reply.is_valid() && reply.is_method_return());
    reply >> ret_code;
    ASSERT_EQ(ret_code, 0);

    msg    = mc::dbus::message::new_method_call("org.openubmc.test_service_1", "/bmc/kepler/test_service_1/MicroComponent",
                                                "bmc.kepler.MicroComponent.Reset", "Cancel");
    writer = msg.writer();
    writer << empty_ctx << "";
    reply = test_conn.send_with_reply(std::move(msg), call_timeout);
    ASSERT_TRUE(reply.is_valid() && reply.is_method_return());
    auto output = reply.read_args();
    EXPECT_EQ(output.size(), 0);
}

TEST_F(MicroComponentTest, TestMicroComponentMaintenanceInterface) {
    auto msg =
        mc::dbus::message::new_method_call("org.openubmc.test_service_1", "/bmc/kepler/test_service_1/MicroComponent",
                                           "bmc.kepler.Release.Maintenance", "DlogLimit");
    auto writer = msg.writer();
    writer << empty_ctx << true << 60;
    auto reply = test_conn.send_with_reply(std::move(msg), call_timeout);
    ASSERT_TRUE(reply.is_valid() && reply.is_method_return());
    auto output = reply.read_args();
    EXPECT_EQ(output.size(), 0);
}