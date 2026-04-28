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

    int32_t readonly_value() const
    {
        return m_value * 2;
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
           ((m_value, "Value"))((m_label, "Label"))(MC_COMPUTED_PROPERTY("ReadonlyValue",
                                                                         readonly_value))((add, "Add"))(value_changed))
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

    std_test_service              service;
    mc::shared_ptr<sample_object> obj;
    mc::shared_ptr<child_object>  child;
};

TEST_F(std_interface_test, non_standard_interface_not_hit)
{
    auto result = mc::engine::standard_interfaces::try_invoke(service, obj.get(), obj->get_object_path(), "Foo",
                                                              mc::variants{}, "org.test.std.SampleInterface");
    EXPECT_FALSE(result.has_value());
}

TEST_F(std_interface_test, peer_ping_and_machine_id)
{
    auto ping_result = mc::engine::standard_interfaces::try_invoke(service, obj.get(), obj->get_object_path(), "Ping",
                                                                   mc::variants{}, "org.freedesktop.DBus.Peer");
    ASSERT_TRUE(ping_result.has_value());
    EXPECT_TRUE(ping_result->result_signature.empty());

    auto mid_result = mc::engine::standard_interfaces::try_invoke(
        service, obj.get(), obj->get_object_path(), "GetMachineId", mc::variants{}, "org.freedesktop.DBus.Peer");
    ASSERT_TRUE(mid_result.has_value());
    EXPECT_EQ(mid_result->result_signature, "s");
    // mc::string, machine-id 不存在时允许返回空字符串
    EXPECT_NO_THROW(mid_result->value.as<std::string>());
}

TEST_F(std_interface_test, std_iface_constants_are_quark_backed)
{
    // 框架预定义的常量必须是 quark backend
    EXPECT_TRUE(mc::engine::std_ifaces::properties.is_quark());
    EXPECT_TRUE(mc::engine::std_ifaces::peer.is_quark());
    EXPECT_TRUE(mc::engine::std_ifaces::get.is_quark());
    EXPECT_TRUE(mc::engine::std_ifaces::introspect.is_quark());
}

TEST_F(std_interface_test, peer_unknown_method_throws)
{
    EXPECT_THROW(mc::engine::standard_interfaces::try_invoke(service, obj.get(), obj->get_object_path(), "NoSuch",
                                                             mc::variants{}, "org.freedesktop.DBus.Peer"),
                 mc::exception);
}

TEST_F(std_interface_test, properties_get_and_set)
{
    // Get
    mc::variants get_args{mc::string("org.test.std.SampleInterface"), mc::string("Value")};
    auto get_hit = mc::engine::standard_interfaces::try_invoke(service, obj.get(), obj->get_object_path(), "Get",
                                                               get_args, "org.freedesktop.DBus.Properties");
    ASSERT_TRUE(get_hit.has_value());
    EXPECT_EQ(get_hit->result_signature, "v");
    EXPECT_EQ(get_hit->value.as<int32_t>(), 42);

    // Set
    mc::variants set_args{mc::string("org.test.std.SampleInterface"), mc::string("Value"), mc::variant{int32_t{99}}};
    auto set_hit = mc::engine::standard_interfaces::try_invoke(service, obj.get(), obj->get_object_path(), "Set",
                                                               set_args, "org.freedesktop.DBus.Properties");
    ASSERT_TRUE(set_hit.has_value());
    EXPECT_TRUE(set_hit->result_signature.empty());
    EXPECT_EQ(obj->m_iface.m_value, 99);
}

TEST_F(std_interface_test, properties_set_readonly_errors)
{
    mc::variants set_args{mc::string("org.test.std.SampleInterface"), mc::string("ReadonlyValue"),
                          mc::variant{int32_t{123}}};
    EXPECT_THROW(mc::engine::standard_interfaces::try_invoke(service, obj.get(), obj->get_object_path(), "Set",
                                                             set_args, "org.freedesktop.DBus.Properties"),
                 mc::exception);
    EXPECT_EQ(obj->m_iface.m_value, 42);
}

TEST_F(std_interface_test, properties_get_all)
{
    mc::variants args{mc::string("org.test.std.SampleInterface")};
    auto hit = mc::engine::standard_interfaces::try_invoke(service, obj.get(), obj->get_object_path(), "GetAll", args,
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
    EXPECT_THROW(mc::engine::standard_interfaces::try_invoke(service, obj.get(), obj->get_object_path(), "Get", args,
                                                             "org.freedesktop.DBus.Properties"),
                 mc::exception);
}

TEST_F(std_interface_test, properties_unknown_property_errors)
{
    mc::variants args{mc::string("org.test.std.SampleInterface"), mc::string("NoSuchProp")};
    EXPECT_THROW(mc::engine::standard_interfaces::try_invoke(service, obj.get(), obj->get_object_path(), "Get", args,
                                                             "org.freedesktop.DBus.Properties"),
                 mc::exception);
}

TEST_F(std_interface_test, introspect_contains_own_and_standard_interfaces)
{
    auto hit = mc::engine::standard_interfaces::try_invoke(service, obj.get(), obj->get_object_path(), "Introspect",
                                                           mc::variants{}, "org.freedesktop.DBus.Introspectable");
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->result_signature, "s");
    mc::string xml = hit->value.as<mc::string>();
    EXPECT_NE(xml.find("<interface name=\"org.test.std.SampleInterface\">"), mc::string::npos) << xml;
    EXPECT_NE(xml.find("<interface name=\"org.freedesktop.DBus.Properties\">"), mc::string::npos);
    EXPECT_NE(xml.find("<interface name=\"org.freedesktop.DBus.Introspectable\">"), mc::string::npos);
    EXPECT_NE(xml.find("<interface name=\"org.freedesktop.DBus.Peer\">"), mc::string::npos);
    EXPECT_NE(xml.find("<interface name=\"org.freedesktop.DBus.ObjectManager\">"), mc::string::npos);
    EXPECT_NE(xml.find("<method name=\"Add\">"), mc::string::npos);
    EXPECT_NE(xml.find("<property name=\"Value\" type=\"i\" access=\"readwrite\" />"), mc::string::npos) << xml;
    EXPECT_NE(xml.find("<property name=\"ReadonlyValue\" type=\"i\" access=\"read\" />"), mc::string::npos) << xml;
    EXPECT_NE(xml.find("<signal name=\"value_changed\">"), mc::string::npos);
    // 子节点
    EXPECT_NE(xml.find("<node name=\"child\">"), mc::string::npos) << xml;
}

TEST_F(std_interface_test, object_manager_get_managed_objects)
{
    auto hit =
        mc::engine::standard_interfaces::try_invoke(service, obj.get(), obj->get_object_path(), "GetManagedObjects",
                                                    mc::variants{}, "org.freedesktop.DBus.ObjectManager");
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->result_signature, "a{oa{sa{sv}}}");
    auto raw_dict = hit->value.as<mc::dict>();
    mc::engine::object_manager_interface::objects_type map;
    for (auto entry_it = raw_dict.begin(); entry_it != raw_dict.end(); ++entry_it) {
        mc::engine::path obj_path(entry_it->key.as_string());
        mc::engine::object_manager_interface::interfaces_type ifaces;
        auto ifaces_dict = entry_it->value.as<mc::dict>();
        for (auto iface_it2 = ifaces_dict.begin(); iface_it2 != ifaces_dict.end(); ++iface_it2) {
            ifaces[iface_it2->key.as<mc::string>()] = iface_it2->value.as<mc::dict>();
        }
        map[obj_path] = std::move(ifaces);
    }
    auto it = map.find(mc::engine::path("/org/test/std/SampleObject/child"));
    ASSERT_NE(it, map.end());
    auto& interfaces = it->second;
    auto  iface_it   = interfaces.find("org.test.std.ChildInterface");
    ASSERT_NE(iface_it, interfaces.end());
    EXPECT_EQ(iface_it->second["ChildValue"], 7);
}

TEST_F(std_interface_test, try_invoke_skips_non_dbus_prefix)
{
    // 走 dispatcher 正常分支：对象自有接口的调用不应被 try_invoke 吞掉
    auto hit = mc::engine::standard_interfaces::try_invoke(service, obj.get(), obj->get_object_path(), "Add",
                                                           mc::variants{int32_t{2}, int32_t{3}},
                                                           "org.test.std.SampleInterface");
    EXPECT_FALSE(hit.has_value());
}

TEST_F(std_interface_test, try_invoke_no_object_introspect_intermediate_returns_xml)
{
    // path /org/test/std 上无对象，但其下挂有 SampleObject，应返回最小节点 XML 列出下一段子节点。
    // 注意：grandchild（SampleObject/child）只贡献第一段 SampleObject，不应单独出现 <node name="child"/>。
    auto hit =
        mc::engine::standard_interfaces::try_invoke(service, nullptr, mc::string_view("/org/test/std"), "Introspect",
                                                    mc::variants{}, "org.freedesktop.DBus.Introspectable");
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->result_signature, "s");
    auto xml = hit->value.as<mc::string>();
    EXPECT_NE(xml.find("<node name=\"SampleObject\"/>"), mc::string::npos) << xml;
    EXPECT_EQ(xml.find("<node name=\"child\"/>"), mc::string::npos)
        << "中间节点 Introspect 只能列出直接子段，不应暴露 grandchild 段名: " << xml;
    EXPECT_EQ(xml.find("<interface"), mc::string::npos) << "中间节点不应列出接口: " << xml;
}

TEST_F(std_interface_test, try_invoke_no_object_introspect_lists_multiple_unique_child_segments)
{
    // 在 /org/test/std 下额外挂一个 sibling 对象，验证 introspect_intermediate_node 同时列出多个直接子段，
    // 且对于多个共享 prefix 的对象（A、A 的 grandchild 等）做去重。
    auto sibling = sample_object::create();
    sibling->set_object_path("/org/test/std/AnotherObject");
    sibling->set_object_name("another");
    service.register_object(sibling);

    auto sibling_grandchild = child_object::create();
    sibling_grandchild->set_object_path("/org/test/std/AnotherObject/leaf");
    sibling_grandchild->set_object_name("leaf");
    sibling_grandchild->set_owner(sibling.get());
    service.register_object(sibling_grandchild);

    auto hit =
        mc::engine::standard_interfaces::try_invoke(service, nullptr, mc::string_view("/org/test/std"), "Introspect",
                                                    mc::variants{}, "org.freedesktop.DBus.Introspectable");
    ASSERT_TRUE(hit.has_value());
    auto xml = hit->value.as<mc::string>();
    EXPECT_NE(xml.find("<node name=\"SampleObject\"/>"), mc::string::npos) << xml;
    EXPECT_NE(xml.find("<node name=\"AnotherObject\"/>"), mc::string::npos) << xml;
    EXPECT_EQ(xml.find("<node name=\"leaf\"/>"), mc::string::npos)
        << "AnotherObject/leaf 只贡献 AnotherObject，不应单独列 leaf: " << xml;
    // 直接子段 AnotherObject 只应出现一次（即使被多个后代共享）
    auto first  = xml.find("<node name=\"AnotherObject\"/>");
    auto second = xml.find("<node name=\"AnotherObject\"/>", first + 1);
    EXPECT_EQ(second, mc::string::npos) << "重复的 child segment 必须去重: " << xml;
}

TEST_F(std_interface_test, try_invoke_no_object_introspect_returns_only_next_segment_across_multi_level_gap)
{
    // /org/test/group 与 /org/test/group/x 都未注册，但 /org/test/group/x/y/z 注册了对象。
    // 在 /org/test/group 上 Introspect 应只返回下一段 x，不应暴露 y/z。
    auto deep = child_object::create();
    deep->set_object_path("/org/test/group/x/y/z");
    deep->set_object_name("deep");
    service.register_object(deep);

    auto hit =
        mc::engine::standard_interfaces::try_invoke(service, nullptr, mc::string_view("/org/test/group"), "Introspect",
                                                    mc::variants{}, "org.freedesktop.DBus.Introspectable");
    ASSERT_TRUE(hit.has_value());
    auto xml = hit->value.as<mc::string>();
    EXPECT_NE(xml.find("<node name=\"x\"/>"), mc::string::npos) << xml;
    EXPECT_EQ(xml.find("<node name=\"y\"/>"), mc::string::npos) << "跨级空路径不应暴露中间段 y: " << xml;
    EXPECT_EQ(xml.find("<node name=\"z\"/>"), mc::string::npos) << "跨级空路径不应暴露叶子段 z: " << xml;
}

TEST_F(std_interface_test, try_invoke_no_object_introspect_without_descendant_returns_nullopt)
{
    // path /org/test/no/such 上无对象也无后代，try_invoke 返回 nullopt（dispatcher 据此回 unknown_object）。
    auto hit = mc::engine::standard_interfaces::try_invoke(service, nullptr, mc::string_view("/org/test/no/such"),
                                                           "Introspect", mc::variants{},
                                                           "org.freedesktop.DBus.Introspectable");
    EXPECT_FALSE(hit.has_value());
}

TEST_F(std_interface_test, try_invoke_no_object_for_non_introspect_returns_nullopt)
{
    // 无对象上下文下，除了 Introspectable.Introspect 之外的标准接口一律返回 nullopt。
    mc::variants get_args{mc::string("org.test.std.SampleInterface"), mc::string("Value")};
    auto hit = mc::engine::standard_interfaces::try_invoke(service, nullptr, mc::string_view("/org/test/std"), "Get",
                                                           get_args, "org.freedesktop.DBus.Properties");
    EXPECT_FALSE(hit.has_value());
}
