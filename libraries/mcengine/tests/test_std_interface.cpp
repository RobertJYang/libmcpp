/*
 * Copyright (c) 2024 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 */

#include <gtest/gtest.h>

#include <mc/engine.h>
#include <mc/engine/errors/std_errors.h>
#include <mc/engine/std_interface.h>
#include <mc/exception.h>
#include <mc/string.h>
#include <mc/variant.h>
#include <test_utilities/engine_test_base.h>

#include <string>
#include <vector>

namespace test_std_interface {

class sample_interface : public mc::engine::interface<sample_interface> {
public:
    MC_INTERFACE("org.test.std.SampleInterface")

    int32_t     m_value{42};
    std::string m_label{"hello"};

    int32_t add(int32_t a, int32_t b)
    {
        return a + b;
    }

    mc::signal<void(int32_t)> value_changed;
};

class sample_object : public mc::engine::object<sample_object> {
public:
    MC_OBJECT(sample_object, "SampleObject", "/org/test/std/SampleObject", (sample_interface))

    sample_interface m_iface;
};

class child_interface : public mc::engine::interface<child_interface> {
public:
    MC_INTERFACE("org.test.std.ChildInterface")

    int32_t m_child_value{7};
};

class child_object : public mc::engine::object<child_object> {
public:
    MC_OBJECT(child_object, "ChildObject", "/org/test/std/SampleObject/child", (child_interface))

    child_interface m_iface;
};

struct std_test_service : public mc::engine::service {
    std_test_service() : mc::engine::service("org.test.std.service")
    {}
};

} // namespace test_std_interface

MC_REFLECT(test_std_interface::sample_interface,
           ((m_value, "Value"))((m_label, "Label"))((add, "Add"))(value_changed))
MC_REFLECT(test_std_interface::sample_object, ((m_iface, "Iface")))
MC_REFLECT(test_std_interface::child_interface, ((m_child_value, "ChildValue")))
MC_REFLECT(test_std_interface::child_object, ((m_iface, "Iface")))

using namespace test_std_interface;

class std_interface_test : public mc::test::TestWithEngine {
protected:
    void SetUp() override
    {
        TestWithEngine::SetUp();
        service.init();
        service.start();

        obj = sample_object::create();
        obj->set_object_name("sample");
        service.register_object(obj);

        child = child_object::create();
        child->set_object_name("child");
        child->set_owner(obj.get());
        service.register_object(child);
    }

    void TearDown() override
    {
        service.stop();
        TestWithEngine::TearDown();
    }

    std_test_service                      service;
    mc::shared_ptr<sample_object>         obj;
    mc::shared_ptr<child_object>          child;
};

TEST_F(std_interface_test, non_standard_interface_not_hit)
{
    auto result = mc::engine::standard_interfaces::try_invoke(*obj, "Foo", mc::variants{},
                                                              "org.test.std.SampleInterface");
    EXPECT_FALSE(result.has_value());
}

TEST_F(std_interface_test, peer_ping_and_machine_id)
{
    auto ping_result =
        mc::engine::standard_interfaces::try_invoke(*obj, "Ping", mc::variants{}, "org.freedesktop.DBus.Peer");
    ASSERT_TRUE(ping_result.has_value());
    EXPECT_TRUE(ping_result->result_signature.empty());

    auto mid_result =
        mc::engine::standard_interfaces::try_invoke(*obj, "GetMachineId", mc::variants{}, "org.freedesktop.DBus.Peer");
    ASSERT_TRUE(mid_result.has_value());
    EXPECT_EQ(mid_result->result_signature, "s");
    // mc::string, 当前实现返回空字符串但仍然是合法 variant
    EXPECT_NO_THROW(mid_result->value.as<std::string>());
}

TEST_F(std_interface_test, peer_unknown_method_throws)
{
    EXPECT_THROW(mc::engine::standard_interfaces::try_invoke(*obj, "NoSuch", mc::variants{},
                                                             "org.freedesktop.DBus.Peer"),
                 mc::exception);
}

TEST_F(std_interface_test, properties_get_and_set)
{
    // Get
    mc::variants get_args{mc::string("org.test.std.SampleInterface"), mc::string("Value")};
    auto         get_hit = mc::engine::standard_interfaces::try_invoke(*obj, "Get", get_args,
                                                                       "org.freedesktop.DBus.Properties");
    ASSERT_TRUE(get_hit.has_value());
    EXPECT_EQ(get_hit->result_signature, "v");
    EXPECT_EQ(get_hit->value.as<int32_t>(), 42);

    // Set
    mc::variants set_args{mc::string("org.test.std.SampleInterface"), mc::string("Value"), mc::variant{int32_t{99}}};
    auto         set_hit = mc::engine::standard_interfaces::try_invoke(*obj, "Set", set_args,
                                                                       "org.freedesktop.DBus.Properties");
    ASSERT_TRUE(set_hit.has_value());
    EXPECT_TRUE(set_hit->result_signature.empty());
    EXPECT_EQ(obj->m_iface.m_value, 99);
}

TEST_F(std_interface_test, properties_get_all)
{
    mc::variants args{mc::string("org.test.std.SampleInterface")};
    auto         hit = mc::engine::standard_interfaces::try_invoke(*obj, "GetAll", args,
                                                                   "org.freedesktop.DBus.Properties");
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->result_signature, "a{sv}");
    auto d = hit->value.as<mc::dict>();
    EXPECT_EQ(d["Value"], 42);
    EXPECT_EQ(d["Label"], "hello");
}

TEST_F(std_interface_test, properties_unknown_interface_errors)
{
    mc::variants args{mc::string("org.no.such.Interface"), mc::string("Value")};
    EXPECT_THROW(mc::engine::standard_interfaces::try_invoke(*obj, "Get", args,
                                                             "org.freedesktop.DBus.Properties"),
                 mc::exception);
}

TEST_F(std_interface_test, properties_unknown_property_errors)
{
    mc::variants args{mc::string("org.test.std.SampleInterface"), mc::string("NoSuchProp")};
    EXPECT_THROW(mc::engine::standard_interfaces::try_invoke(*obj, "Get", args,
                                                             "org.freedesktop.DBus.Properties"),
                 mc::exception);
}

TEST_F(std_interface_test, introspect_contains_own_and_standard_interfaces)
{
    auto hit = mc::engine::standard_interfaces::try_invoke(*obj, "Introspect", mc::variants{},
                                                           "org.freedesktop.DBus.Introspectable");
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->result_signature, "s");
    mc::string xml = hit->value.as<mc::string>();
    EXPECT_NE(xml.find("<interface name=\"org.test.std.SampleInterface\">"), mc::string::npos) << xml;
    EXPECT_NE(xml.find("<interface name=\"org.freedesktop.DBus.Properties\">"), mc::string::npos);
    EXPECT_NE(xml.find("<interface name=\"org.freedesktop.DBus.Introspectable\">"), mc::string::npos);
    EXPECT_NE(xml.find("<interface name=\"org.freedesktop.DBus.Peer\">"), mc::string::npos);
    EXPECT_NE(xml.find("<interface name=\"org.freedesktop.DBus.ObjectManager\">"), mc::string::npos);
    EXPECT_NE(xml.find("<method name=\"Add\">"), mc::string::npos);
    EXPECT_NE(xml.find("<property name=\"Value\""), mc::string::npos);
    EXPECT_NE(xml.find("<signal name=\"value_changed\">"), mc::string::npos);
    // 子节点
    EXPECT_NE(xml.find("<node name=\"child\">"), mc::string::npos) << xml;
}

TEST_F(std_interface_test, object_manager_get_managed_objects)
{
    auto hit = mc::engine::standard_interfaces::try_invoke(*obj, "GetManagedObjects", mc::variants{},
                                                           "org.freedesktop.DBus.ObjectManager");
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->result_signature, "a{oa{sa{sv}}}");
    auto map = hit->value.as<mc::engine::object_manager_interface::objects_type>();
    auto it  = map.find(mc::engine::path("/org/test/std/SampleObject/child"));
    ASSERT_NE(it, map.end());
    auto& interfaces = it->second;
    auto  iface_it   = interfaces.find("org.test.std.ChildInterface");
    ASSERT_NE(iface_it, interfaces.end());
    EXPECT_EQ(iface_it->second["ChildValue"], 7);
}

TEST_F(std_interface_test, try_invoke_skips_non_dbus_prefix)
{
    // 走 dispatcher 正常分支：对象自有接口的调用不应被 try_invoke 吞掉
    auto hit = mc::engine::standard_interfaces::try_invoke(*obj, "Add",
                                                           mc::variants{int32_t{2}, int32_t{3}},
                                                           "org.test.std.SampleInterface");
    EXPECT_FALSE(hit.has_value());
}
