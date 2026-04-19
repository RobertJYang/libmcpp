/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
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
#include <atomic>
#include <mc/dbus/service_protocol.h>
#include <mc/engine.h>
#include <mc/exception.h>
#include <mc/string.h>

#include <chrono>
#include <mc/dbus/sd_bus.h>
#include <test_utilities/test_base.h>
#include <thread>

static int g_test_cnt = 0;

namespace tests::dbus::sd_bus {
class TestInterface1 : public mc::engine::interface<TestInterface1> {
public:
    MC_INTERFACE("org.test.sd_bus.TestInterface1")

    void set_value(int32_t value)
    {
        m_value = value;
    }

    int32_t get_value() const
    {
        return m_value;
    }

    void sleep(int32_t seconds)
    {
        std::this_thread::sleep_for(std::chrono::seconds(seconds));
    }

    std::string parse_requestor(mc::dict context)
    {
        return context["Requestor"].as_string();
    }

    mc::engine::property<int32_t> m_value{0};
};

class TestObject1 : public mc::engine::object<TestObject1> {
public:
    MC_OBJECT(TestObject1, "TestObject1", "/org/test/sd_bus/TestObject1", (TestInterface1))

    void init()
    {
        set_object_name("TestObject1");
    }

    TestInterface1 m_iface1;
};

class TestInterfaceA : public mc::engine::interface<TestInterfaceA> {
public:
    MC_INTERFACE("org.test.sd_bus.TestInterfaceA")

    void test_increment(std::string_view input)
    {
        g_test_cnt++;
    }

    std::string test_increment_and_return_string()
    {
        g_test_cnt++;
        return std::string(UINT16_MAX + 1, 'a');
    }

    void test_throw_unknown_property(std::string_view property)
    {
        MC_REPLY_ERROR_AND_THROW(mc::engine::errors::unknown_property, ("property", property));
    }
};

class TestObjectA : public mc::engine::object<TestObjectA> {
public:
    MC_OBJECT(TestObjectA, "TestObjectA", "/org/test/sd_bus/TestObjectA", (TestInterfaceA))

    void init()
    {
        set_object_name("TestObjectA");
    }

    TestInterfaceA m_iface;
};

class TestChipInterface : public mc::engine::interface<TestChipInterface> {
public:
    MC_INTERFACE("bmc.dev.Chip")

    std::vector<uint32_t> BitIORead(uint32_t offset, uint8_t length, uint32_t mask)
    {
        return {0x12, 0x34, 0x56, 0x78};
    }

    void BitIOWrite(uint32_t offset, uint8_t length, uint32_t mask, const std::vector<uint8_t>& buffer)
    {
        return;
    }

    std::vector<uint32_t> BlockIORead(uint32_t offset, uint32_t length)
    {
        return {0x12, 0x34, 0x56, 0x78};
    }

    void BlockIOWrite(uint32_t offset, const std::vector<uint8_t>& buffer)
    {
        return;
    }

    std::vector<uint32_t> BlockIOWriteRead(const std::vector<uint8_t>& indata, uint32_t read_length)
    {
        return {0x12, 0x34, 0x56, 0x78};
    }

    std::vector<uint32_t> BlockIOComboWriteRead(uint32_t write_offset, const std::vector<uint8_t>& write_buffer,
                                                uint32_t read_offset, uint32_t read_length)
    {
        return {0x12, 0x34, 0x56, 0x78};
    }
};

class TestChipObject : public mc::engine::object<TestChipObject> {
public:
    MC_OBJECT(TestChipObject, "TestChipObject", "/bmc/dev/TestChip", (TestChipInterface))

    void init()
    {
        set_object_name("TestChipObject");
    }

    TestChipInterface m_iface;
};

struct test_service_1 : public mc::engine::service {
    test_service_1() : mc::engine::service("org.test.test_service_1")
    {}

    bool init(mc::dict args = {})
    {
        mc::dict args_mut(args);
        args_mut["service_path"] = "/org/test/test_service_1";
        args_mut["service_name"] = "org.test.test_service_1";
        return mc::engine::service::init(args_mut);
    }

    bool start()
    {
        if (!mc::engine::service::start()) {
            return false;
        }
        m_obj_1 = mc::make_shared<TestObject1>();
        m_obj_1->init();
        register_object(*m_obj_1);
        m_obj_a = mc::make_shared<TestObjectA>();
        m_obj_a->init();
        register_object(*m_obj_a);
        return true;
    }

    mc::shared_ptr<TestObject1> m_obj_1;
    mc::shared_ptr<TestObjectA> m_obj_a;
};

struct test_devmon_service : public mc::engine::service {
    test_devmon_service() : mc::engine::service("bmc.kepler.devmon")
    {}

    bool init(mc::dict args = {})
    {
        return mc::engine::service::init(args);
    }

    bool start()
    {
        if (!mc::engine::service::start()) {
            return false;
        }
        m_obj_chip = mc::make_shared<TestChipObject>();
        m_obj_chip->init();
        register_object(*m_obj_chip);
        return true;
    }

    mc::shared_ptr<TestChipObject> m_obj_chip;
};

} // namespace tests::dbus::sd_bus

MC_REFLECT(tests::dbus::sd_bus::TestInterface1,
           ((m_value, "Value"))((set_value, "SetValue"))((get_value, "GetValue"))((sleep, "Sleep"))((parse_requestor,
                                                                                                     "ParseRequestor")))
MC_REFLECT(tests::dbus::sd_bus::TestObject1, ((m_iface1, "Interface1")))
MC_REFLECT(tests::dbus::sd_bus::TestInterfaceA,
           ((test_increment, "TestIncrement"))((test_increment_and_return_string, "TestIncrementAndReturnString"))(
               (test_throw_unknown_property, "TestThrowUnknownProperty")))
MC_REFLECT(tests::dbus::sd_bus::TestObjectA, ((m_iface, "InterfaceA")))
MC_REFLECT(tests::dbus::sd_bus::TestChipInterface,
           ((BitIORead, "BitIORead"))((BitIOWrite, "BitIOWrite"))((BlockIORead, "BlockIORead"))(
               (BlockIOWrite, "BlockIOWrite"))((BlockIOWriteRead, "BlockIOWriteRead"))((BlockIOComboWriteRead,
                                                                                        "BlockIOComboWriteRead")))
MC_REFLECT(tests::dbus::sd_bus::TestChipObject, ((m_iface, "Chip")))

using namespace mc::dbus;

static tests::dbus::sd_bus::test_service_1*      service_1;
static tests::dbus::sd_bus::test_devmon_service* devmon_service;
static connection*                               service_1_conn;
static connection*                               devmon_conn;
static DBusConnection*                           service_1_raw_conn;
static DBusConnection*                           devmon_raw_conn;
static std::atomic_bool                          server_dispatch_running{false};
static std::thread*                              service_1_dispatch_thread;
static std::thread*                              devmon_dispatch_thread;
static sd_bus*                                   test_bus;

namespace {

message make_method_return(message& request)
{
    return message::new_method_return(request);
}

void send_empty_reply(DBusConnection* raw_conn, message& request)
{
    auto reply = make_method_return(request);
    ASSERT_TRUE(raw_conn != nullptr);
    ASSERT_TRUE(dbus_connection_send(raw_conn, reply.get_dbus_message(), nullptr));
    dbus_connection_flush(raw_conn);
}

template <typename ValueType>
void send_value_reply(DBusConnection* raw_conn, message& request, ValueType&& value)
{
    auto reply = make_method_return(request);
    auto writer = reply.writer();
    writer << std::forward<ValueType>(value);
    ASSERT_TRUE(raw_conn != nullptr);
    ASSERT_TRUE(dbus_connection_send(raw_conn, reply.get_dbus_message(), nullptr));
    dbus_connection_flush(raw_conn);
}

DBusHandlerResult handle_test_service_1_call(DBusConnection* raw_conn, message& msg)
{
    if (!msg.is_method_call()) {
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    const auto args = msg.read_args();
    if (msg.get_path() == "/org/test/sd_bus/TestObject1") {
        const auto member = std::string(msg.get_member());
        auto&      iface  = service_1->m_obj_1->m_iface1;
        if (member == "SetValue") {
            MC_ASSERT(args.size() == 1, "SetValue expects 1 argument");
            iface.set_value(args[0].as_int32());
            send_empty_reply(raw_conn, msg);
            return DBUS_HANDLER_RESULT_HANDLED;
        }
        if (member == "GetValue") {
            send_value_reply(raw_conn, msg, iface.get_value());
            return DBUS_HANDLER_RESULT_HANDLED;
        }
        if (member == "Sleep") {
            MC_ASSERT(args.size() == 1, "Sleep expects 1 argument");
            iface.sleep(args[0].as_int32());
            send_empty_reply(raw_conn, msg);
            return DBUS_HANDLER_RESULT_HANDLED;
        }
        if (member == "ParseRequestor") {
            MC_ASSERT(args.size() == 1 && args[0].is_dict(), "ParseRequestor expects a dict context");
            send_value_reply(raw_conn, msg, iface.parse_requestor(args[0].as_dict()));
            return DBUS_HANDLER_RESULT_HANDLED;
        }
    }

    MC_THROW(mc::exception, "unsupported test service call: ${path}.${member}",
             ("path", msg.get_path())("member", msg.get_member()));
}

DBusHandlerResult handle_devmon_call(DBusConnection* raw_conn, message& msg)
{
    if (!msg.is_method_call()) {
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    const auto args   = msg.read_args();
    const auto member = std::string(msg.get_member());
    auto&      iface  = devmon_service->m_obj_chip->m_iface;

    if (member == "BitIORead") {
        MC_ASSERT(args.size() == 3, "BitIORead expects 3 arguments");
        send_value_reply(raw_conn, msg, iface.BitIORead(args[0].as_uint32(), args[1].as_uint8(), args[2].as_uint32()));
        return DBUS_HANDLER_RESULT_HANDLED;
    }
    if (member == "BitIOWrite") {
        MC_ASSERT(args.size() == 4, "BitIOWrite expects 4 arguments");
        iface.BitIOWrite(args[0].as_uint32(), args[1].as_uint8(), args[2].as_uint32(),
                         args[3].as<std::vector<uint8_t>>());
        send_empty_reply(raw_conn, msg);
        return DBUS_HANDLER_RESULT_HANDLED;
    }
    if (member == "BlockIORead") {
        MC_ASSERT(args.size() == 2, "BlockIORead expects 2 arguments");
        send_value_reply(raw_conn, msg, iface.BlockIORead(args[0].as_uint32(), args[1].as_uint32()));
        return DBUS_HANDLER_RESULT_HANDLED;
    }
    if (member == "BlockIOWrite") {
        MC_ASSERT(args.size() == 2, "BlockIOWrite expects 2 arguments");
        iface.BlockIOWrite(args[0].as_uint32(), args[1].as<std::vector<uint8_t>>());
        send_empty_reply(raw_conn, msg);
        return DBUS_HANDLER_RESULT_HANDLED;
    }
    if (member == "BlockIOWriteRead") {
        MC_ASSERT(args.size() == 2, "BlockIOWriteRead expects 2 arguments");
        send_value_reply(raw_conn, msg,
                         iface.BlockIOWriteRead(args[0].as<std::vector<uint8_t>>(), args[1].as_uint32()));
        return DBUS_HANDLER_RESULT_HANDLED;
    }
    if (member == "BlockIOComboWriteRead") {
        MC_ASSERT(args.size() == 4, "BlockIOComboWriteRead expects 4 arguments");
        send_value_reply(raw_conn, msg,
                         iface.BlockIOComboWriteRead(args[0].as_uint32(), args[1].as<std::vector<uint8_t>>(),
                                                     args[2].as_uint32(), args[3].as_uint32()));
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    MC_THROW(mc::exception, "unsupported devmon call: ${member}", ("member", member));
}

void dispatch_connection_loop(connection& conn)
{
    while (server_dispatch_running.load(std::memory_order_acquire)) {
        auto* raw_conn = conn.get_connection();
        if (raw_conn != nullptr) {
            dbus_connection_read_write(raw_conn, 10);
            conn.dispatch();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

} // namespace

class SdBusTest : public mc::test::TestWithDbusDaemon {
protected:
    static void SetUpTestSuite()
    {
        mc::test::TestWithDbusDaemon::SetUpTestSuite();
        service_1 = new tests::dbus::sd_bus::test_service_1();
        service_1->init();
        service_1->start();
        devmon_service = new tests::dbus::sd_bus::test_devmon_service();
        devmon_service->init();
        devmon_service->start();

        service_1_conn = new connection(connection::open_session_bus(mc::get_io_context()));
        ASSERT_TRUE(service_1_conn->start());
        ASSERT_TRUE(std::get<0>(service_1_conn->request_name("org.test.test_service_1")));
        service_1_raw_conn = service_1_conn->get_connection();
        service_1_conn->register_path("/org/test/sd_bus/TestObject1",
                                      [](message& msg) -> DBusHandlerResult {
                                          return handle_test_service_1_call(service_1_raw_conn, msg);
                                      });
        service_1_conn->register_path("/org/test/sd_bus/TestObjectA",
                                      [](message& msg) -> DBusHandlerResult {
                                          return handle_test_service_1_call(service_1_raw_conn, msg);
                                      });

        devmon_conn = new connection(connection::open_session_bus(mc::get_io_context()));
        ASSERT_TRUE(devmon_conn->start());
        ASSERT_TRUE(std::get<0>(devmon_conn->request_name("bmc.kepler.devmon")));
        devmon_raw_conn = devmon_conn->get_connection();
        devmon_conn->register_path("/bmc/dev/TestChip",
                                   [](message& msg) -> DBusHandlerResult {
                                       return handle_devmon_call(devmon_raw_conn, msg);
                                   });

        server_dispatch_running.store(true, std::memory_order_release);
        service_1_dispatch_thread = new std::thread([]() {
            dispatch_connection_loop(*service_1_conn);
        });
        devmon_dispatch_thread = new std::thread([]() {
            dispatch_connection_loop(*devmon_conn);
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        test_bus = new sd_bus(true, false);
        test_bus->request_name("org.openubmc.test_bus");
#if defined(ENABLE_CONAN_COMPILE) && ENABLE_CONAN_COMPILE == 1
#else
        test_bus->set_enable_local_request(false);
#endif
    }

    static void TearDownTestSuite()
    {
        server_dispatch_running.store(false, std::memory_order_release);
        service_1_dispatch_thread->join();
        devmon_dispatch_thread->join();
        delete service_1_dispatch_thread;
        delete devmon_dispatch_thread;
        service_1_conn->disconnect();
        devmon_conn->disconnect();
        delete service_1_conn;
        delete devmon_conn;
        service_1->stop();
        devmon_service->stop();
        delete service_1;
        delete devmon_service;
        delete test_bus;
        mc::test::TestWithDbusDaemon::TearDownTestSuite();
    }
};

// 测试有效参数的调用
TEST_F(SdBusTest, test_valid_args_call)
{
    auto result = test_bus->call({"org.test.test_service_1",
                                  "/org/test/sd_bus/TestObject1",
                                  "org.test.sd_bus.TestInterface1",
                                  "SetValue",
                                  "i",
                                  {12}});
    ASSERT_TRUE(result.empty());
    result = test_bus->call({"org.test.test_service_1",
                             "/org/test/sd_bus/TestObject1",
                             "org.test.sd_bus.TestInterface1",
                             "GetValue",
                             "",
                             {}});
    ASSERT_EQ(result.size(), 1u);
    ASSERT_TRUE(result[0].is_int32());
    EXPECT_EQ(result[0].as_int32(), 12);
}

// 测试无效参数的调用
TEST_F(SdBusTest, test_invalid_args_call)
{
    EXPECT_THROW(test_bus->call({"org.test.test_service_1",
                                 "/org/test/sd_bus/TestObjectA",
                                 "org.test.sd_bus.TestInterfaceA",
                                 "NonExistentMethod",
                                 "",
                                 {}}),
                 mc::exception);
    auto [error, result] = test_bus->pcall({"org.test.test_service_1",
                                            "/org/test/sd_bus/TestObjectA",
                                            "org.test.sd_bus.TestInterfaceA",
                                            "NonExistentMethod",
                                            "",
                                            {}});
    ASSERT_TRUE(error.has_value());
    ASSERT_TRUE(result.empty());
}

// 测试阻塞式DBus调用
TEST_F(SdBusTest, test_blocking_bus_call)
{
    sd_bus blocking_bus(true, true);
    auto   result = blocking_bus.call({"org.test.test_service_1",
                                       "/org/test/sd_bus/TestObject1",
                                       "org.test.sd_bus.TestInterface1",
                                       "SetValue",
                                       "i",
                                       {-33}});
    ASSERT_TRUE(result.empty());
    result = blocking_bus.call({"org.test.test_service_1",
                                "/org/test/sd_bus/TestObject1",
                                "org.test.sd_bus.TestInterface1",
                                "GetValue",
                                "",
                                {}});
    ASSERT_EQ(result.size(), 1u);
    ASSERT_TRUE(result[0].is_int32());
    EXPECT_EQ(result[0].as_int32(), -33);
}

// 测试禁用本地请求后回退到普通 DBus 调用
TEST_F(SdBusTest, test_disable_local_request_fallback_to_dbus)
{
    sd_bus fallback_bus(true, false);
    fallback_bus.request_name("org.openubmc.test_bus_no_local_request");
    fallback_bus.set_enable_local_request(false);

    auto result = fallback_bus.timeout_call(mc::seconds(1), {"org.test.test_service_1",
                                                             "/org/test/sd_bus/TestObject1",
                                                             "org.test.sd_bus.TestInterface1",
                                                             "SetValue",
                                                             "i",
                                                             {21}});
    ASSERT_TRUE(result.empty());

    result = fallback_bus.timeout_call(mc::seconds(1), {"org.test.test_service_1",
                                                        "/org/test/sd_bus/TestObject1",
                                                        "org.test.sd_bus.TestInterface1",
                                                        "GetValue",
                                                        "",
                                                        {}});
    ASSERT_EQ(result.size(), 1u);
    ASSERT_TRUE(result[0].is_int32());
    EXPECT_EQ(result[0].as_int32(), 21);
}

// 测试指定超时时间调用
TEST_F(SdBusTest, test_call_timeout)
{
    // 使用阻塞式 sd_bus 校验超时语义，避免非阻塞路径在高并发测试场景下出现时序抖动
    sd_bus blocking_bus(true, true);
    auto   result =
        blocking_bus.timeout_call(mc::seconds(3), {"org.test.test_service_1", "/org/test/sd_bus/TestObject1",
                                                   "org.test.sd_bus.TestInterface1", "Sleep", "i", mc::variants{2}});
    ASSERT_TRUE(result.empty());
    EXPECT_THROW(
        blocking_bus.timeout_call(mc::seconds(1), {"org.test.test_service_1", "/org/test/sd_bus/TestObject1",
                                                   "org.test.sd_bus.TestInterface1", "Sleep", "i", mc::variants{2}}),
        mc::exception);
}

// 测试devmon chip接口和方法映射调用
TEST_F(SdBusTest, test_devmon_chip_methods)
{
    auto result = test_bus->call({"bmc.kepler.devmon", "/bmc/dev/TestChip", "bmc.kepler.Chip.BitIO", "Read", "a{ss}uyu",
                                  mc::variants{mc::dict(), 0x1234, 0x5678, 0x9ABC}});
    ASSERT_EQ(result.size(), 1u);
    auto arr1 = result[0];
    ASSERT_TRUE(arr1.is_array());
    EXPECT_EQ(arr1.as_array().size(), 4);
    EXPECT_EQ(arr1.as_array()[0].as_uint32(), 0x12);
    EXPECT_EQ(arr1.as_array()[1].as_uint32(), 0x34);
    EXPECT_EQ(arr1.as_array()[2].as_uint32(), 0x56);
    EXPECT_EQ(arr1.as_array()[3].as_uint32(), 0x78);

    result = test_bus->call({"bmc.kepler.devmon", "/bmc/dev/TestChip", "bmc.kepler.Chip.BitIO", "Write", "a{ss}uyuay",
                             mc::variants{mc::dict(), 0x1234, 0x5678, 0x9ABC, mc::variants{0x12, 0x34, 0x56, 0x78}}});
    ASSERT_TRUE(result.empty());

    result = test_bus->call({"bmc.kepler.devmon", "/bmc/dev/TestChip", "bmc.kepler.Chip.BlockIO", "Read", "a{ss}uu",
                             mc::variants{mc::dict(), 0x1234, 0x5678}});
    ASSERT_EQ(result.size(), 1u);
    auto arr2 = result[0];
    ASSERT_TRUE(arr2.is_array());
    EXPECT_EQ(arr2.as_array().size(), 4);
    EXPECT_EQ(arr2.as_array()[0].as_uint32(), 0x12);
    EXPECT_EQ(arr2.as_array()[1].as_uint32(), 0x34);
    EXPECT_EQ(arr2.as_array()[2].as_uint32(), 0x56);
    EXPECT_EQ(arr2.as_array()[3].as_uint32(), 0x78);

    result = test_bus->call({"bmc.kepler.devmon", "/bmc/dev/TestChip", "bmc.kepler.Chip.BlockIO", "Write", "a{ss}uay",
                             mc::variants{mc::dict(), 0x1234, mc::variants{0x12, 0x34, 0x56, 0x78}}});
    ASSERT_TRUE(result.empty());

    result = test_bus->call({"bmc.kepler.devmon", "/bmc/dev/TestChip", "bmc.kepler.Chip.BlockIO", "WriteRead",
                             "a{ss}ayu", mc::variants{mc::dict(), mc::variants{0x12, 0x34, 0x56, 0x78}, 4}});
    ASSERT_EQ(result.size(), 1u);
    auto arr3 = result[0];
    ASSERT_TRUE(arr3.is_array());
    EXPECT_EQ(arr3.as_array().size(), 4);
    EXPECT_EQ(arr3.as_array()[0].as_uint32(), 0x12);
    EXPECT_EQ(arr3.as_array()[1].as_uint32(), 0x34);
    EXPECT_EQ(arr3.as_array()[2].as_uint32(), 0x56);
    EXPECT_EQ(arr3.as_array()[3].as_uint32(), 0x78);

    result =
        test_bus->call({"bmc.kepler.devmon", "/bmc/dev/TestChip", "bmc.kepler.Chip.BlockIO", "ComboWriteRead",
                        "a{ss}uayuu", mc::variants{mc::dict(), 0x1234, mc::variants{0x12, 0x34, 0x56, 0x78}, 4, 4}});
    ASSERT_EQ(result.size(), 1u);
    auto arr4 = result[0];
    ASSERT_TRUE(arr4.is_array());
    EXPECT_EQ(arr4.as_array().size(), 4);
    EXPECT_EQ(arr4.as_array()[0].as_uint32(), 0x12);
    EXPECT_EQ(arr4.as_array()[1].as_uint32(), 0x34);
    EXPECT_EQ(arr4.as_array()[2].as_uint32(), 0x56);
    EXPECT_EQ(arr4.as_array()[3].as_uint32(), 0x78);
}

// 测试上下文Requestor字段
TEST_F(SdBusTest, test_context_requestor)
{
    // 测试入参显式指定Requestor字段
    mc::dict context;
    context["Requestor"] = "org.openubmc.test_client";
    auto result          = test_bus->call({"org.test.test_service_1", "/org/test/sd_bus/TestObject1",
                                           "org.test.sd_bus.TestInterface1", "ParseRequestor", "a{ss}", mc::variants{context}});
    ASSERT_EQ(result.size(), 1u);
    ASSERT_TRUE(result[0].is_string());
    EXPECT_EQ(result[0].as_string(), "org.openubmc.test_client");

    // 测试自动填充Requestor字段
    result = test_bus->call({"org.test.test_service_1", "/org/test/sd_bus/TestObject1",
                             "org.test.sd_bus.TestInterface1", "ParseRequestor", "a{ss}", mc::variants{mc::dict()}});
    ASSERT_EQ(result.size(), 1u);
    ASSERT_TRUE(result[0].is_string());
    EXPECT_EQ(result[0].as_string(), "org.openubmc.test_bus");
}

TEST_F(SdBusTest, test_dbus_service_protocol_invoke_method)
{
    mc::dbus::dbus_service_protocol protocol(*test_bus);
    mc::engine::service_operation   operation{
        mc::engine::service_operation_kind::invoke_method,
        mc::engine::invoke_method_operation{
            {"org.test.test_service_1", "/org/test/sd_bus/TestObject1", "org.test.sd_bus.TestInterface1"},
            "GetValue",
            "",
            {},
            {},
        },
    };

    auto result = protocol.request(operation);
    ASSERT_TRUE(result.is_array());
    ASSERT_EQ(result.as_array().size(), 1u);
    ASSERT_TRUE(result.as_array()[0].is_int32());
    EXPECT_EQ(result.as_array()[0].as_int32(), service_1->m_obj_1->m_iface1.get_value());
}

TEST_F(SdBusTest, test_dbus_service_protocol_exports_registered_object)
{
    class protocol_export_service : public mc::engine::service {
    public:
        protocol_export_service() : mc::engine::service("org.test.protocol_export_service")
        {}

        void init()
        {
            m_obj = mc::make_shared<tests::dbus::sd_bus::TestObject1>();
            m_obj->set_object_name("ProtocolExportTestObject1");
            m_obj->set_object_path("/org/test/protocol_export/TestObject1");
            register_object(*m_obj);
        }

        mc::shared_ptr<tests::dbus::sd_bus::TestObject1> m_obj;
    };

    protocol_export_service exported_service;
    exported_service.init();
    exported_service.start();

    mc::dbus::sd_bus exported_bus(true, false);
    exported_bus.request_name("org.test.protocol_export_service");
    auto protocol = mc::make_shared<mc::dbus::dbus_service_protocol>(exported_bus);
    exported_service.set_protocol(protocol);

    std::atomic_bool export_dispatch_running{true};
    std::thread export_dispatch_thread([&]() {
        while (export_dispatch_running.load(std::memory_order_acquire)) {
            auto& conn     = exported_bus.get_connection();
            auto* raw_conn = conn.get_connection();
            if (raw_conn != nullptr) {
                dbus_connection_read_write(raw_conn, 10);
                conn.dispatch();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    try {
        auto result = test_bus->timeout_call(
            mc::seconds(1), {"org.test.protocol_export_service", "/org/test/protocol_export/TestObject1",
                             "org.test.sd_bus.TestInterface1", "SetValue", "i", mc::variants{42}});
        ASSERT_TRUE(result.empty());

        result = test_bus->timeout_call(
            mc::seconds(1), {"org.test.protocol_export_service", "/org/test/protocol_export/TestObject1",
                             "org.test.sd_bus.TestInterface1", "GetValue", "", mc::variants{}});
        ASSERT_EQ(result.size(), 1u);
        ASSERT_TRUE(result[0].is_int32());
        EXPECT_EQ(result[0].as_int32(), 42);
    } catch (...) {
        export_dispatch_running.store(false, std::memory_order_release);
        export_dispatch_thread.join();
        exported_service.stop();
        throw;
    }

    export_dispatch_running.store(false, std::memory_order_release);
    export_dispatch_thread.join();
    exported_service.stop();
}

TEST_F(SdBusTest, test_dbus_service_protocol_emits_properties_changed)
{
    GTEST_SKIP() << "暂时跳过：该用例依赖 shm/外部依赖能力，当前受 Meson/Conan/编译器组合差异影响，"
                    "待统一处理相关构建与依赖矩阵后再恢复";

    class protocol_export_service : public mc::engine::service {
    public:
        protocol_export_service() : mc::engine::service("org.test.protocol_export_signal_service")
        {}

        void init()
        {
            m_obj = mc::make_shared<tests::dbus::sd_bus::TestObject1>();
            m_obj->set_object_name("ProtocolExportSignalTestObject1");
            m_obj->set_object_path("/org/test/protocol_export_signal/TestObject1");
            register_object(*m_obj);
        }

        mc::shared_ptr<tests::dbus::sd_bus::TestObject1> m_obj;
    };

    protocol_export_service exported_service;
    exported_service.init();
    exported_service.start();

    mc::dbus::sd_bus exported_bus(true, false);
    exported_bus.request_name("org.test.protocol_export_signal_service");
    auto protocol = mc::make_shared<mc::dbus::dbus_service_protocol>(exported_bus);
    exported_service.set_protocol(protocol);

    std::atomic_bool export_dispatch_running{true};
    std::thread export_dispatch_thread([&]() {
        while (export_dispatch_running.load(std::memory_order_acquire)) {
            auto& conn     = exported_bus.get_connection();
            auto* raw_conn = conn.get_connection();
            if (raw_conn != nullptr) {
                dbus_connection_read_write(raw_conn, 10);
                conn.dispatch();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });

    mc::dbus::match_rule rule = mc::dbus::match_rule::new_signal("PropertiesChanged", "org.freedesktop.DBus.Properties");
    rule.with_path("/org/test/protocol_export_signal/TestObject1");

    mc::dbus::message signal_msg;
    auto match_id = test_bus->add_match(rule, [&signal_msg](mc::dbus::message& current) {
        signal_msg = mc::dbus::message(current);
    });

    std::atomic_bool receiver_dispatch_running{true};
    std::thread receiver_dispatch_thread([&]() {
        while (receiver_dispatch_running.load(std::memory_order_acquire)) {
            auto& conn     = test_bus->get_connection();
            auto* raw_conn = conn.get_connection();
            if (raw_conn != nullptr) {
                dbus_connection_read_write(raw_conn, 10);
                conn.dispatch();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });

    struct cleanup_guard {
        mc::dbus::sd_bus*   bus;
        uint64_t            match_id;
        std::atomic_bool*   export_running;
        std::thread*        export_thread;
        std::atomic_bool*   receiver_running;
        std::thread*        receiver_thread;
        mc::engine::service* service;

        ~cleanup_guard()
        {
            if (bus != nullptr) {
                bus->remove_match(match_id);
            }
            if (export_running != nullptr) {
                export_running->store(false, std::memory_order_release);
            }
            if (receiver_running != nullptr) {
                receiver_running->store(false, std::memory_order_release);
            }
            if (export_thread != nullptr && export_thread->joinable()) {
                export_thread->join();
            }
            if (receiver_thread != nullptr && receiver_thread->joinable()) {
                receiver_thread->join();
            }
            if (service != nullptr) {
                service->stop();
            }
        }
    } guard{test_bus,
            match_id,
            &export_dispatch_running,
            &export_dispatch_thread,
            &receiver_dispatch_running,
            &receiver_dispatch_thread,
            &exported_service};

    exported_service.m_obj->m_iface1.m_value.set_value(77);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    ASSERT_TRUE(signal_msg.is_valid());
    ASSERT_EQ(signal_msg.get_type(), mc::dbus::message_type::signal);
    ASSERT_EQ(signal_msg.get_path(), "/org/test/protocol_export_signal/TestObject1");
    ASSERT_EQ(signal_msg.get_interface(), "org.freedesktop.DBus.Properties");
    ASSERT_EQ(signal_msg.get_member(), "PropertiesChanged");

    auto                         reader = signal_msg.reader();
    mc::dbus::signature_iterator it("sa{sv}as");
    mc::variant                  interface;
    reader.read_variant_value(it.current_type_code(), interface, 0);
    ASSERT_TRUE(interface.is_string());
    ASSERT_EQ(interface.as_string(), "org.test.sd_bus.TestInterface1");

    it.next();
    mc::variant properties;
    reader.read_variant_value(it.current_type_code(), properties, 0);
    ASSERT_TRUE(properties.is_dict());
    auto d = properties.as_dict();
    ASSERT_TRUE(d.contains("Value"));
    ASSERT_TRUE(d["Value"].is_int32());
    EXPECT_EQ(d["Value"].as_int32(), 77);
}

TEST_F(SdBusTest, test_dbus_service_protocol_emits_interface_lifecycle_signals)
{
    class protocol_export_service : public mc::engine::service {
    public:
        protocol_export_service() : mc::engine::service("org.test.protocol_export_lifecycle_service")
        {}

        void init()
        {
            m_obj = mc::make_shared<tests::dbus::sd_bus::TestObject1>();
            m_obj->set_object_name("ProtocolExportLifecycleTestObject1");
            m_obj->set_object_path("/org/test/protocol_export_lifecycle/TestObject1");
            register_object(*m_obj);
        }

        mc::shared_ptr<tests::dbus::sd_bus::TestObject1> m_obj;
    };

    protocol_export_service exported_service;
    exported_service.init();
    exported_service.start();

    mc::dbus::sd_bus exported_bus(true, false);
    exported_bus.request_name("org.test.protocol_export_lifecycle_service");

    mc::dbus::match_rule added_rule =
        mc::dbus::match_rule::new_signal("InterfacesAdded", "org.freedesktop.DBus.ObjectManager");
    added_rule.with_path("/org/test/protocol_export_lifecycle/TestObject1");
    mc::dbus::message added_msg;
    auto added_match_id = test_bus->add_match(added_rule, [&added_msg](mc::dbus::message& current) {
        added_msg = mc::dbus::message(current);
    });

    mc::dbus::match_rule removed_rule =
        mc::dbus::match_rule::new_signal("InterfacesRemoved", "org.freedesktop.DBus.ObjectManager");
    removed_rule.with_path("/org/test/protocol_export_lifecycle/TestObject1");
    mc::dbus::message removed_msg;
    auto removed_match_id = test_bus->add_match(removed_rule, [&removed_msg](mc::dbus::message& current) {
        removed_msg = mc::dbus::message(current);
    });

    std::atomic_bool export_dispatch_running{true};
    std::thread export_dispatch_thread([&]() {
        while (export_dispatch_running.load(std::memory_order_acquire)) {
            auto& conn     = exported_bus.get_connection();
            auto* raw_conn = conn.get_connection();
            if (raw_conn != nullptr) {
                dbus_connection_read_write(raw_conn, 10);
                conn.dispatch();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });

    std::atomic_bool receiver_dispatch_running{true};
    std::thread receiver_dispatch_thread([&]() {
        while (receiver_dispatch_running.load(std::memory_order_acquire)) {
            auto& conn     = test_bus->get_connection();
            auto* raw_conn = conn.get_connection();
            if (raw_conn != nullptr) {
                dbus_connection_read_write(raw_conn, 10);
                conn.dispatch();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });

    struct cleanup_guard {
        mc::dbus::sd_bus*    bus;
        uint64_t             added_match_id;
        uint64_t             removed_match_id;
        std::atomic_bool*    export_running;
        std::thread*         export_thread;
        std::atomic_bool*    receiver_running;
        std::thread*         receiver_thread;
        mc::engine::service* service;

        ~cleanup_guard()
        {
            if (bus != nullptr) {
                bus->remove_match(added_match_id);
                bus->remove_match(removed_match_id);
            }
            if (export_running != nullptr) {
                export_running->store(false, std::memory_order_release);
            }
            if (receiver_running != nullptr) {
                receiver_running->store(false, std::memory_order_release);
            }
            if (export_thread != nullptr && export_thread->joinable()) {
                export_thread->join();
            }
            if (receiver_thread != nullptr && receiver_thread->joinable()) {
                receiver_thread->join();
            }
            if (service != nullptr) {
                service->stop();
            }
        }
    } guard{test_bus,
            added_match_id,
            removed_match_id,
            &export_dispatch_running,
            &export_dispatch_thread,
            &receiver_dispatch_running,
            &receiver_dispatch_thread,
            &exported_service};

    auto protocol = mc::make_shared<mc::dbus::dbus_service_protocol>(exported_bus);
    exported_service.set_protocol(protocol);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    ASSERT_TRUE(added_msg.is_valid());
    ASSERT_EQ(added_msg.get_type(), mc::dbus::message_type::signal);
    ASSERT_EQ(added_msg.get_path(), "/org/test/protocol_export_lifecycle/TestObject1");
    ASSERT_EQ(added_msg.get_interface(), "org.freedesktop.DBus.ObjectManager");
    ASSERT_EQ(added_msg.get_member(), "InterfacesAdded");

    auto added_args = added_msg.read_args();
    ASSERT_EQ(added_args.size(), 2u);
    ASSERT_TRUE(added_args[1].is_dict());
    ASSERT_TRUE(added_args[1].as_dict().contains("org.test.sd_bus.TestInterface1"));

    exported_service.unregister_object("/org/test/protocol_export_lifecycle/TestObject1");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    ASSERT_TRUE(removed_msg.is_valid());
    ASSERT_EQ(removed_msg.get_type(), mc::dbus::message_type::signal);
    ASSERT_EQ(removed_msg.get_path(), "/org/test/protocol_export_lifecycle/TestObject1");
    ASSERT_EQ(removed_msg.get_interface(), "org.freedesktop.DBus.ObjectManager");
    ASSERT_EQ(removed_msg.get_member(), "InterfacesRemoved");

    auto removed_args = removed_msg.read_args();
    ASSERT_EQ(removed_args.size(), 2u);
    ASSERT_TRUE(removed_args[1].is_array());
    ASSERT_FALSE(removed_args[1].as_array().empty());
    ASSERT_TRUE(removed_args[1].as_array()[0].is_string());
    EXPECT_EQ(removed_args[1].as_array()[0].as_string(), "org.test.sd_bus.TestInterface1");
}

// 测试有异常处理的方法调用
TEST_F(SdBusTest, test_protected_call)
{
    auto [error_1, result_1] = test_bus->pcall({"org.test.test_service_1",
                                                "/org/test/sd_bus/TestObject1",
                                                "org.test.sd_bus.TestInterface1",
                                                "TestThrowUnknownProperty",
                                                "s",
                                                {"TestProperty1"}});
    ASSERT_TRUE(error_1.has_value());
    ASSERT_TRUE(result_1.empty());
    auto [error_2, result_2] = test_bus->timeout_pcall(
        mc::seconds(30), {"org.test.test_service_1", "/org/test/sd_bus/TestObject1", "org.test.sd_bus.TestInterface1",
                          "TestThrowUnknownProperty", "s", mc::variants{"TestProperty2"}});
    ASSERT_TRUE(error_2.has_value());
    ASSERT_TRUE(result_2.empty());
}

// 测试无异常处理的方法调用
TEST_F(SdBusTest, test_unprotected_call)
{
    EXPECT_THROW(
        test_bus->call({"org.test.test_service_1", "/org/test/sd_bus/TestObject1", "org.test.sd_bus.TestInterface1",
                        "TestThrowUnknownProperty", "s", mc::variants{"TestProperty"}}),
        mc::exception);
    EXPECT_THROW(test_bus->timeout_call(mc::seconds(30), {"org.test.test_service_1", "/org/test/sd_bus/TestObject1",
                                                          "org.test.sd_bus.TestInterface1", "TestThrowUnknownProperty",
                                                          "s", mc::variants{"TestProperty"}}),
                 mc::exception);
}

#if defined(ENABLE_CONAN_COMPILE) && ENABLE_CONAN_COMPILE == 1
// 测试共享内存调用
TEST_F(SdBusTest, test_shm_call)
{
    auto result_opt = test_bus->shm_timeout_call(mc::seconds(30),
                                                 {"org.test.test_service_1", "/org/test/sd_bus/TestObject1",
                                                  "org.test.sd_bus.TestInterface1", "SetValue", "i", mc::variants{99}});
    ASSERT_TRUE(result_opt.has_value());
    auto result = result_opt.value();
    ASSERT_TRUE(result.empty());
    result_opt =
        test_bus->shm_timeout_call(mc::seconds(30), {"org.test.test_service_1", "/org/test/sd_bus/TestObject1",
                                                     "org.test.sd_bus.TestInterface1", "GetValue", "", mc::variants{}});
    ASSERT_TRUE(result_opt.has_value());
    result = result_opt.value();
    ASSERT_EQ(result.size(), 1u);
    ASSERT_TRUE(result[0].is_int32());
    EXPECT_EQ(result[0].as_int32(), 99);
}

// 测试共享内存调用请求大小超过限制
TEST_F(SdBusTest, test_shm_call_request_size_over_limit)
{
    g_test_cnt = 0;
    std::string large_string(UINT16_MAX + 1, 'a');
    auto        result_opt = test_bus->shm_timeout_call(
        mc::seconds(30), {"org.test.test_service_1", "/org/test/sd_bus/TestObject1", "org.test.sd_bus.TestInterface1",
                                 "TestIncrement", "s", mc::variants{large_string}});
    ASSERT_FALSE(result_opt.has_value());
    EXPECT_EQ(g_test_cnt, 0);
}
#endif