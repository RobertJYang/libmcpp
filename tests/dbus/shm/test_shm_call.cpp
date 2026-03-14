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
#include <mc/dbus/shm/local_msg.h>
#include <mc/dbus/shm/serialize.h>
#include <mc/dbus/shm/shm_tree.h>
#include <mc/dict.h>
#include <mc/engine.h>
#include <mc/time.h>
#include <mc/variant.h>

using namespace mc::engine;

class TestInterfaceA : public mc::engine::interface<TestInterfaceA> {
public:
    MC_INTERFACE("org.openubmc.test_interface_a")

    int32_t add(int32_t a, int32_t b)
    {
        return a + b;
    }

    void set_num(uint8_t num)
    {
        m_num = num;
    }

    void set_str(std::string str)
    {
        m_str = str;
    }

    std::tuple<uint8_t, std::string> get_num_and_str()
    {
        return std::make_tuple(m_num.value(), m_str.value());
    }

    property<uint8_t>     m_num{100};
    property<std::string> m_str{"default"};
};

class TestInterfaceB : public mc::engine::interface<TestInterfaceB> {
public:
    MC_INTERFACE("org.openubmc.test_interface_b")

    int32_t increment()
    {
        m_cnt.set_value(m_cnt.value() + 1);
        return m_cnt.value();
    }

    property<int32_t>              m_cnt{0};
    property<std::vector<uint8_t>> m_arr{std::vector<uint8_t>{1, 2, 3}};
};

class TestObjectA : public mc::engine::object<TestObjectA> {
public:
    MC_OBJECT(TestObjectA, "TestObjectA", "/org/openubmc/test_object_a", (TestInterfaceA))

    void init()
    {
        set_object_name("TestObjectA");
    }

    TestInterfaceA m_iface;
};

class TestObjectB : public mc::engine::object<TestObjectB> {
public:
    MC_OBJECT(TestObjectB, "TestObjectB", "/org/openubmc/test_object_b", (TestInterfaceB))

    void init()
    {
        set_object_name("TestObjectB");
    }

    TestInterfaceB m_iface;
};

class TestInterfaceC : public mc::engine::interface<TestInterfaceC> {
public:
    MC_INTERFACE("org.openubmc.test_interface_c")

    property<int32_t> m_prop1{0};
};

class TestObjectC : public mc::engine::object<TestObjectC> {
public:
    MC_OBJECT(TestObjectC, "TestObjectC", "/org/openubmc/test_object_c", (TestInterfaceC))

    void init()
    {
        set_object_name("TestObjectC");
    }

    TestInterfaceC m_iface;
};

MC_REFLECT(TestInterfaceA,
           ((add, "Add"))((set_num, "SetNum"))((set_str, "SetStr"))((get_num_and_str,
                                                                     "GetNumAndStr"))((m_num, "Num"))((m_str, "Str")))
MC_REFLECT(TestInterfaceB, ((increment, "Increment"))((m_cnt, "Cnt"))((m_arr, "Arr")))
MC_REFLECT(TestObjectA, ((m_iface, "InterfaceA")))
MC_REFLECT(TestObjectB, ((m_iface, "InterfaceB")))
MC_REFLECT(TestInterfaceC, ((m_prop1, "Prop1")))
MC_REFLECT(TestObjectC, ((m_iface, "InterfaceC")))

struct test_service_1 : public mc::engine::service {
    test_service_1() : mc::engine::service("org.openubmc.test_service_1")
    {}

    bool init(mc::dict args = {}) override
    {
        mc::dict args_mut(args);
        args_mut["service_path"] = "/org/openubmc/test_service_1";
        args_mut["service_name"] = "org.openubmc.test_service_1";
        return mc::engine::service::init(args_mut);
    }

    bool start() override
    {
        if (!mc::engine::service::start()) {
            return false;
        }

        m_obj_a = mc::make_shared<TestObjectA>();
        m_obj_a->init();
        register_object(*m_obj_a);
        return true;
    }

    void init_obj_c()
    {
        m_obj_c = mc::make_shared<TestObjectC>();
        m_obj_c->init();
        register_object(*m_obj_c);
    }

    void remove_obj_c()
    {
        unregister_object(m_obj_c->get_object_path());
        m_obj_c.reset();
    }

    mc::shared_ptr<TestObjectA> m_obj_a;
    mc::shared_ptr<TestObjectC> m_obj_c;
};

struct test_service_2 : public mc::engine::service {
    test_service_2() : mc::engine::service("org.openubmc.test_service_2")
    {}

    bool init(mc::dict args = {}) override
    {
        mc::dict args_mut(args);
        args_mut["service_path"] = "/org/openubmc/test_service_2";
        args_mut["service_name"] = "org.openubmc.test_service_2";
        return mc::engine::service::init(args_mut);
    }

    bool start() override
    {
        if (!mc::engine::service::start()) {
            return false;
        }

        m_obj_b = mc::make_shared<TestObjectB>();
        m_obj_b->init();
        register_object(*m_obj_b);
        return true;
    }

    mc::shared_ptr<TestObjectB> m_obj_b;
};

static test_service_1*   service_1;
static test_service_2*   service_2;
static constexpr int64_t CALL_TIMEOUT = 1000;

class ShmCallTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        service_1 = new test_service_1();
        service_2 = new test_service_2();
        service_1->init();
        service_2->init();
        service_1->start();
        service_2->start();
    }

    static void TearDownTestSuite()
    {
        service_1->stop();
        service_2->stop();
        delete service_1;
        delete service_2;
    }
};

TEST_F(ShmCallTest, TestRegisterProperties)
{
    mc::variant result = mc::dbus::shm_tree::get_property("org.openubmc.test_service_1", "/org/openubmc/test_object_a",
                                                          "org.openubmc.test_interface_a", "Num");
    ASSERT_EQ(result.as_int32(), 100);

    result = mc::dbus::shm_tree::get_property("org.openubmc.test_service_1", "/org/openubmc/test_object_a",
                                              "org.openubmc.test_interface_a", "Str");
    ASSERT_EQ(result.as_string(), "default");

    result = mc::dbus::shm_tree::get_property("org.openubmc.test_service_1", "/org/openubmc/test_object_a",
                                              "bmc.kepler.Object.Properties", "ObjectName");
    ASSERT_EQ(result.as_string(), "TestObjectA");

    result = mc::dbus::shm_tree::get_property("org.openubmc.test_service_1", "/org/openubmc/test_object_a",
                                              "bmc.kepler.Object.Properties", "ClassName");
    ASSERT_EQ(result.as_string(), "TestObjectA");

    result = mc::dbus::shm_tree::get_property("org.openubmc.test_service_2", "/org/openubmc/test_object_b",
                                              "org.openubmc.test_interface_b", "Cnt");
    ASSERT_EQ(result.as_int32(), 0);

    result = mc::dbus::shm_tree::get_property("org.openubmc.test_service_2", "/org/openubmc/test_object_b",
                                              "bmc.kepler.Object.Properties", "ObjectName");
    ASSERT_EQ(result.as_string(), "TestObjectB");

    result = mc::dbus::shm_tree::get_property("org.openubmc.test_service_2", "/org/openubmc/test_object_b",
                                              "bmc.kepler.Object.Properties", "ClassName");
    ASSERT_EQ(result.as_string(), "TestObjectB");
}

TEST_F(ShmCallTest, TestIncrement)
{
    mc::variant result = service_1->timeout_call(mc::milliseconds(CALL_TIMEOUT), "org.openubmc.test_service_2",
                                                 "/org/openubmc/test_object_b", "org.openubmc.test_interface_b",
                                                 "Increment", "", mc::variants{});
    ASSERT_EQ(result.as_int32(), 1);

    result = service_1->timeout_call(mc::milliseconds(CALL_TIMEOUT), "org.openubmc.test_service_2",
                                     "/org/openubmc/test_object_b", "org.openubmc.test_interface_b", "Increment", "",
                                     mc::variants{});
    ASSERT_EQ(result.as_int32(), 2);

    auto opt = service_1->shm_timeout_call(mc::milliseconds(CALL_TIMEOUT), "org.openubmc.test_service_2",
                                           "/org/openubmc/test_object_b", "org.openubmc.test_interface_b", "Increment",
                                           "", mc::variants{});
    ASSERT_TRUE(opt.has_value());
    result = opt.value();
    ASSERT_EQ(result.as_int32(), 3);

    result = mc::dbus::shm_tree::get_property("org.openubmc.test_service_2", "/org/openubmc/test_object_b",
                                              "org.openubmc.test_interface_b", "Cnt");
    ASSERT_EQ(result.as_int32(), 3);
}

TEST_F(ShmCallTest, TestNumAndStr)
{
    mc::variant result = service_2->timeout_call(mc::milliseconds(CALL_TIMEOUT), "org.openubmc.test_service_1",
                                                 "/org/openubmc/test_object_a", "org.openubmc.test_interface_a",
                                                 "GetNumAndStr", "", mc::variants{});
    ASSERT_EQ(result.get_type(), mc::type_id::array_type);
    auto arr = result.as_array();
    ASSERT_EQ(arr.size(), 2);
    ASSERT_EQ(arr[0].as_uint8(), 100);
    ASSERT_EQ(arr[1].as_string(), "default");

    result = service_2->timeout_call(mc::milliseconds(CALL_TIMEOUT), "org.openubmc.test_service_1",
                                     "/org/openubmc/test_object_a", "org.openubmc.test_interface_a", "SetNum", "y",
                                     mc::variants{200});
    ASSERT_TRUE(result.is_null());

    result = service_2->timeout_call(mc::milliseconds(CALL_TIMEOUT), "org.openubmc.test_service_1",
                                     "/org/openubmc/test_object_a", "org.openubmc.test_interface_a", "SetStr", "s",
                                     mc::variants{"new_str"});
    ASSERT_TRUE(result.is_null());

    auto opt = service_2->shm_timeout_call(mc::milliseconds(CALL_TIMEOUT), "org.openubmc.test_service_1",
                                           "/org/openubmc/test_object_a", "org.openubmc.test_interface_a",
                                           "GetNumAndStr", "", mc::variants{});
    ASSERT_TRUE(opt.has_value());
    result = opt.value();
    ASSERT_EQ(result.get_type(), mc::type_id::array_type);
    arr = result.as_array();
    ASSERT_EQ(arr.size(), 2);
    ASSERT_EQ(arr[0].as_uint8(), 200);
    ASSERT_EQ(arr[1].as_string(), "new_str");

    result = service_2->shm_timeout_call(mc::milliseconds(CALL_TIMEOUT), "org.openubmc.test_service_1",
                                         "/org/openubmc/test_object_a", "org.openubmc.test_interface_a", "SetNum", "y",
                                         mc::variants{69});
    ASSERT_TRUE(result.is_null());

    result = service_2->shm_timeout_call(mc::milliseconds(CALL_TIMEOUT), "org.openubmc.test_service_1",
                                         "/org/openubmc/test_object_a", "org.openubmc.test_interface_a", "SetStr", "s",
                                         mc::variants{"new_str_2"});
    ASSERT_TRUE(result.is_null());

    opt = service_2->shm_timeout_call(mc::milliseconds(CALL_TIMEOUT), "org.openubmc.test_service_1",
                                      "/org/openubmc/test_object_a", "org.openubmc.test_interface_a", "GetNumAndStr",
                                      "", mc::variants{});
    ASSERT_TRUE(opt.has_value());
    result = opt.value();
    ASSERT_EQ(result.get_type(), mc::type_id::array_type);
    arr = result.as_array();
    ASSERT_EQ(arr.size(), 2);
    ASSERT_EQ(arr[0].as_uint8(), 69);
    ASSERT_EQ(arr[1].as_string(), "new_str_2");

    result = mc::dbus::shm_tree::get_property("org.openubmc.test_service_1", "/org/openubmc/test_object_a",
                                              "org.openubmc.test_interface_a", "Num");
    ASSERT_EQ(result.as_uint8(), 69);

    result = mc::dbus::shm_tree::get_property("org.openubmc.test_service_1", "/org/openubmc/test_object_a",
                                              "org.openubmc.test_interface_a", "Str");
    ASSERT_EQ(result.as_string(), "new_str_2");
}

TEST_F(ShmCallTest, TestAdd)
{
    mc::variant result = service_2->timeout_call(mc::milliseconds(CALL_TIMEOUT), "org.openubmc.test_service_1",
                                                 "/org/openubmc/test_object_a", "org.openubmc.test_interface_a", "Add",
                                                 "ii", mc::variants{1, 2});
    ASSERT_EQ(result.as_int32(), 3);

    result = service_2->timeout_call(mc::milliseconds(CALL_TIMEOUT), "org.openubmc.test_service_1",
                                     "/org/openubmc/test_object_a", "org.openubmc.test_interface_a", "Add", "ii",
                                     mc::variants{77, 23});
    ASSERT_EQ(result.as_int32(), 100);

    auto opt = service_2->shm_timeout_call(mc::milliseconds(CALL_TIMEOUT), "org.openubmc.test_service_1",
                                           "/org/openubmc/test_object_a", "org.openubmc.test_interface_a", "Add", "ii",
                                           mc::variants{89, 23});
    ASSERT_EQ(opt.has_value(), true);
    result = opt.value();
    ASSERT_EQ(result.as_int32(), 112);
}

TEST_F(ShmCallTest, TestSubscribePropertiesChanged)
{
    mc::dbus::match_rule rule =
        mc::dbus::match_rule::new_signal(mc::dbus::PROPERTIES_CHANGED_MEMBER, mc::dbus::DBUS_PROPERTIES_INTERFACE);
    rule.with_path("/org/openubmc/test_object_a");
    mc::dbus::message msg;
    service_2->add_match(rule, [&msg](mc::dbus::message& signal_msg) {
        msg = mc::dbus::message(signal_msg);
    });
    service_1->m_obj_a->m_iface.m_str.set_value("test_property_changed");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ASSERT_TRUE(msg.is_valid());
    ASSERT_EQ(msg.get_type(), mc::dbus::message_type::signal);
    ASSERT_EQ(msg.get_path(), "/org/openubmc/test_object_a");
    ASSERT_EQ(msg.get_interface(), "org.freedesktop.DBus.Properties");
    ASSERT_EQ(msg.get_member(), "PropertiesChanged");

    auto                         reader = msg.reader();
    mc::dbus::signature_iterator it("sa{sv}as");
    mc::variant                  interface;
    reader.read_variant_value(it.current_type_code(), interface, 0);
    ASSERT_TRUE(interface.is_string());
    ASSERT_EQ(interface.as_string(), "org.openubmc.test_interface_a");
    it.next();
    mc::variant properties;
    reader.read_variant_value(it.current_type_code(), properties, 0);
    ASSERT_TRUE(properties.is_dict());
    auto d = properties.as_dict();
    ASSERT_EQ(d.size(), 1);
    ASSERT_EQ(d.contains("Str"), true);
    ASSERT_TRUE(d["Str"].is_string());
    ASSERT_EQ(d["Str"].as_string(), "test_property_changed");
}