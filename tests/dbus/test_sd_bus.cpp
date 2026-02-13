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

    void set_value(int32_t value) {
        m_value = value;
    }

    int32_t get_value() const {
        return m_value;
    }

    void sleep(int32_t seconds) {
        std::this_thread::sleep_for(std::chrono::seconds(seconds));
    }

    std::string parse_requestor(mc::dict context) {
        return context["Requestor"].as_string();
    }

    mc::engine::property<int32_t> m_value{0};
};

class TestObject1 : public mc::engine::object<TestObject1> {
public:
    MC_OBJECT(TestObject1, "TestObject1", "/org/test/sd_bus/TestObject1", (TestInterface1))

    void init() {
        set_object_name("TestObject1");
    }

    TestInterface1 m_iface1;
};

class TestInterfaceA : public mc::engine::interface<TestInterfaceA> {
public:
    MC_INTERFACE("org.test.sd_bus.TestInterfaceA")

    void test_increment(std::string_view input) {
        g_test_cnt++;
    }

    std::string test_increment_and_return_string() {
        g_test_cnt++;
        return std::string(UINT16_MAX + 1, 'a');
    }

    void test_throw_unknown_property(std::string_view property) {
        MC_REPLY_ERROR_AND_THROW(mc::engine::errors::unknown_property, ("property", property));
    }
};

class TestObjectA : public mc::engine::object<TestObjectA> {
public:
    MC_OBJECT(TestObjectA, "TestObjectA", "/org/test/sd_bus/TestObjectA", (TestInterfaceA))

    void init() {
        set_object_name("TestObjectA");
    }

    TestInterfaceA m_iface;
};

class TestChipInterface : public mc::engine::interface<TestChipInterface> {
public:
    MC_INTERFACE("bmc.dev.Chip")

    std::vector<uint32_t> BitIORead(uint32_t offset, uint8_t length, uint32_t mask) {
        return {0x12, 0x34, 0x56, 0x78};
    }

    void BitIOWrite(uint32_t offset, uint8_t length, uint32_t mask, const std::vector<uint8_t>& buffer) {
        return;
    }

    std::vector<uint32_t> BlockIORead(uint32_t offset, uint32_t length) {
        return {0x12, 0x34, 0x56, 0x78};
    }

    void BlockIOWrite(uint32_t offset, const std::vector<uint8_t>& buffer) {
        return;
    }

    std::vector<uint32_t> BlockIOWriteRead(const std::vector<uint8_t>& indata, uint32_t read_length) {
        return {0x12, 0x34, 0x56, 0x78};
    }

    std::vector<uint32_t> BlockIOComboWriteRead(uint32_t write_offset, const std::vector<uint8_t>& write_buffer,
                                                uint32_t read_offset, uint32_t read_length) {
        return {0x12, 0x34, 0x56, 0x78};
    }
};

class TestChipObject : public mc::engine::object<TestChipObject> {
public:
    MC_OBJECT(TestChipObject, "TestChipObject", "/bmc/dev/TestChip", (TestChipInterface))

    void init() {
        set_object_name("TestChipObject");
    }

    TestChipInterface m_iface;
};

struct test_service_1 : public mc::engine::service {
    test_service_1() : mc::engine::service("org.test.test_service_1") {
    }

    bool init(mc::dict args = {}) override {
        mc::dict args_mut(args);
        args_mut["service_path"] = "/org/test/test_service_1";
        args_mut["service_name"] = "org.test.test_service_1";
        return mc::engine::service::init(args_mut);
    }

    bool start() override {
        if (!mc::engine::service::start()) {
            return false;
        }
#if defined(ENABLE_CONAN_COMPILE) && ENABLE_CONAN_COMPILE == 1
#else
        auto ins = shm::shared_memory::get_instance();
        ins.get_tree("harbor.org.test.test_service_1")->set_harbor_name("");
#endif
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
    test_devmon_service() : mc::engine::service("bmc.kepler.devmon") {
    }

    bool init(mc::dict args = {}) override {
        return mc::engine::service::init(args);
    }

    bool start() override {
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
static sd_bus*                                   test_bus;

class SdBusTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        service_1 = new tests::dbus::sd_bus::test_service_1();
        service_1->init();
        service_1->start();
        devmon_service = new tests::dbus::sd_bus::test_devmon_service();
        devmon_service->init();
        devmon_service->start();
        test_bus = new sd_bus(true, false);
        test_bus->request_name("org.openubmc.test_bus");
    }

    static void TearDownTestSuite() {
        service_1->stop();
        devmon_service->stop();
        delete service_1;
        delete devmon_service;
        delete test_bus;
    }
};

// 测试有效参数的调用
TEST_F(SdBusTest, test_valid_args_call) {
    auto result = test_bus->call({"org.test.test_service_1", "/org/test/sd_bus/TestObject1", "org.test.sd_bus.TestInterface1", "SetValue", "i", {12}});
    ASSERT_TRUE(result.empty());
    result = test_bus->call({"org.test.test_service_1", "/org/test/sd_bus/TestObject1", "org.test.sd_bus.TestInterface1", "GetValue", "", {}});
    ASSERT_EQ(result.size(), 1u);
    ASSERT_TRUE(result[0].is_int32());
    EXPECT_EQ(result[0].as_int32(), 12);
}

// 测试无效参数的调用
TEST_F(SdBusTest, test_invalid_args_call) {
    EXPECT_THROW(test_bus->call({"org.test.test_service_1", "/org/test/sd_bus/TestObjectA", "org.test.sd_bus.TestInterfaceA", "NonExistentMethod", "", {}}),
                 mc::exception);
    auto [error, result] = test_bus->pcall({"org.test.test_service_1", "/org/test/sd_bus/TestObjectA", "org.test.sd_bus.TestInterfaceA", "NonExistentMethod", "", {}});
    ASSERT_TRUE(error.has_value());
    ASSERT_TRUE(result.empty());
}

// 测试阻塞式DBus调用
TEST_F(SdBusTest, test_blocking_bus_call) {
    sd_bus blocking_bus(true, true);
    auto   result = blocking_bus.call({"org.test.test_service_1", "/org/test/sd_bus/TestObject1", "org.test.sd_bus.TestInterface1", "SetValue", "i", {-33}});
    ASSERT_TRUE(result.empty());
    result = blocking_bus.call({"org.test.test_service_1", "/org/test/sd_bus/TestObject1", "org.test.sd_bus.TestInterface1", "GetValue", "", {}});
    ASSERT_EQ(result.size(), 1u);
    ASSERT_TRUE(result[0].is_int32());
    EXPECT_EQ(result[0].as_int32(), -33);
}

// 测试指定超时时间调用
TEST_F(SdBusTest, test_call_timeout) {
    auto result = test_bus->timeout_call(mc::seconds(3), {"org.test.test_service_1", "/org/test/sd_bus/TestObject1",
                                                          "org.test.sd_bus.TestInterface1", "Sleep", "i", mc::variants{2}});
    ASSERT_TRUE(result.empty());
    EXPECT_THROW(test_bus->timeout_call(mc::seconds(1), {"org.test.test_service_1", "/org/test/sd_bus/TestObject1",
                                                         "org.test.sd_bus.TestInterface1", "Sleep", "i", mc::variants{2}}),
                 mc::exception);
}

// 测试devmon chip接口和方法映射调用
TEST_F(SdBusTest, test_devmon_chip_methods) {
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

    result = test_bus->call({"bmc.kepler.devmon", "/bmc/dev/TestChip", "bmc.kepler.Chip.BlockIO", "ComboWriteRead",
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
TEST_F(SdBusTest, test_context_requestor) {
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

// 测试有异常处理的方法调用
TEST_F(SdBusTest, test_protected_call) {
    auto [error_1, result_1] =
        test_bus->pcall({"org.test.test_service_1", "/org/test/sd_bus/TestObject1", "org.test.sd_bus.TestInterface1", "TestThrowUnknownProperty", "s", {"TestProperty1"}});
    ASSERT_TRUE(error_1.has_value());
    ASSERT_TRUE(result_1.empty());
    auto [error_2, result_2] =
        test_bus->timeout_pcall(mc::seconds(30), {"org.test.test_service_1", "/org/test/sd_bus/TestObject1",
                                                  "org.test.sd_bus.TestInterface1", "TestThrowUnknownProperty", "s", mc::variants{"TestProperty2"}});
    ASSERT_TRUE(error_2.has_value());
    ASSERT_TRUE(result_2.empty());
}

// 测试无异常处理的方法调用
TEST_F(SdBusTest, test_unprotected_call) {
    EXPECT_THROW(test_bus->call({"org.test.test_service_1", "/org/test/sd_bus/TestObject1", "org.test.sd_bus.TestInterface1",
                                 "TestThrowUnknownProperty", "s", mc::variants{"TestProperty"}}),
                 mc::exception);
    EXPECT_THROW(test_bus->timeout_call(mc::seconds(30), {"org.test.test_service_1", "/org/test/sd_bus/TestObject1",
                                                          "org.test.sd_bus.TestInterface1", "TestThrowUnknownProperty", "s",
                                                          mc::variants{"TestProperty"}}),
                 mc::exception);
}

#if defined(ENABLE_CONAN_COMPILE) && ENABLE_CONAN_COMPILE == 1
// 测试共享内存调用
TEST_F(SdBusTest, test_shm_call) {
    auto result_opt =
        test_bus->shm_timeout_call(mc::seconds(30), {"org.test.test_service_1", "/org/test/sd_bus/TestObject1",
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
TEST_F(SdBusTest, test_shm_call_request_size_over_limit) {
    g_test_cnt = 0;
    std::string large_string(UINT16_MAX + 1, 'a');
    auto        result_opt =
        test_bus->shm_timeout_call(mc::seconds(30), {"org.test.test_service_1", "/org/test/sd_bus/TestObject1",
                                                     "org.test.sd_bus.TestInterface1", "TestIncrement", "s", mc::variants{large_string}});
    ASSERT_FALSE(result_opt.has_value());
    EXPECT_EQ(g_test_cnt, 0);
}
#endif