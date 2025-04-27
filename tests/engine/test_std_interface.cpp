/*
 * Copyright (c) 2024 Huawei Technologies Co., Ltd.
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

#include <test_utilities/test_base.h>
#include <thread>

// 解析xml
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

namespace tests::engine::std_interface {
class TestInterface1 : public mc::engine::interface<TestInterface1> {
public:
    MC_INTERFACE("org.test.TestInterface1")

    void set_value(int32_t value) {
        m_value = value;
    }

    int32_t get_value() const {
        return m_value;
    }

    int32_t     m_value{0};
    std::string m_name;

    mc::signal<void(int32_t, int32_t)> m_value_changeds;
};

class TestInterface2 : public mc::engine::interface<TestInterface2> {
public:
    MC_INTERFACE("org.test.TestInterface2")

    std::string                     m_id;
    std::map<std::string, uint32_t> m_map;
};

class TestObject1 : public mc::engine::object<TestObject1> {
public:
    MC_OBJECT("/org/test/TestObject", (TestInterface1))

    TestInterface1 m_iface1;
};

class TestObject2 : public mc::engine::object<TestObject2> {
public:
    MC_OBJECT("/Id/${Id}", (TestInterface2))

    const std::string& get_object_path() const override {
        if (this->m_object_path.empty()) {
            if (get_parent()) {
                this->m_object_path = get_parent()->get_object_path();
            }
            this->m_object_path += mc::string::format(path_pattern, {{"Id", this->m_iface2.m_id}});
        }

        return this->m_object_path;
    }

    TestInterface2 m_iface2;
};

} // namespace tests::engine::std_interface

MC_REFLECT(tests::engine::std_interface::TestInterface1,
           ((m_value, "Value"))((m_name, "Name"))((set_value, "SetValue"))((get_value, "GetValue"))(
               (m_value_changeds, "ValueChanged")))
MC_REFLECT(tests::engine::std_interface::TestInterface2, ((m_id, "Id"))((m_map, "Map")))
MC_REFLECT(tests::engine::std_interface::TestObject1, ((m_iface1, "Interface1")))
MC_REFLECT(tests::engine::std_interface::TestObject2, ((m_iface2, "Interface2")))

namespace bp = boost::property_tree;
using namespace tests::engine::std_interface;
using namespace mc::engine;

class std_interface_test : public mc::test::TestBaseWithEngine {
protected:
    TestObject1 obj1;
    TestObject2 obj2;
    TestObject2 obj3;

    void SetUp() override {
        obj1.m_iface1.m_value = 100;
        obj1.m_iface1.m_name  = "Name";

        obj2.m_iface2.m_id  = "00101";
        obj2.m_iface2.m_map = {{"Key1", 1}, {"Key2", 2}};

        obj3.m_iface2.m_id  = "00102";
        obj3.m_iface2.m_map = {{"Key3", 3}, {"Key4", 4}};

        obj1.add_child(&obj2);
        obj1.add_child(&obj3);
    }

    void TearDown() override {
    }

    static mc::dict decode_introspect(const std::string& xml) {
        std::stringstream ss(xml);
        bp::ptree         pt;

        EXPECT_NO_THROW(bp::read_xml(ss, pt));

        mc::mutable_dict object;
        for (const auto& child : pt.get_child("node")) {
            if (child.first == "interface") {
                mc::mutable_dict interface;
                get_interface(child.second, interface);

                auto name    = child.second.get<std::string>("<xmlattr>.name");
                object[name] = interface;
            }
        }

        return object;
    }

    static void get_interface(const bp::ptree& pt, mc::mutable_dict& object) {
        auto name = pt.get<std::string>("<xmlattr>.name");

        mc::variants props;
        mc::variants methods;
        mc::variants signals;
        for (const auto& child : pt) {
            if (child.first == "property") {
                get_props(child.second, props);
            } else if (child.first == "signal") {
                get_signal(child.second, signals);
            } else if (child.first == "method") {
                get_method(child.second, methods);
            }
        }

        if (!props.empty()) {
            object["properties"] = props;
        }
        if (!methods.empty()) {
            object["methods"] = methods;
        }
        if (!signals.empty()) {
            object["signals"] = signals;
        }
    }

    static void get_method(const bp::ptree& pt, mc::variants& methods) {
        auto name = pt.get<std::string>("<xmlattr>.name");

        mc::variants args;
        for (const auto& child : pt) {
            if (child.first == "arg") {
                get_arg(child.second, args);
            }
        }
        methods.push_back(mc::dict{{"name", name}, {"args", args}});
    }

    static void get_arg(const bp::ptree& pt, mc::variants& method) {
        auto             name      = pt.get<std::string>("<xmlattr>.name");
        auto             type      = pt.get<std::string>("<xmlattr>.type");
        auto             direction = pt.get<std::string>("<xmlattr>.direction", "");
        mc::mutable_dict arg       = {{"name", name}, {"type", type}};
        if (!direction.empty()) {
            arg["direction"] = direction;
        }
        method.push_back(arg);
    }

    static void get_props(const bp::ptree& pt, mc::variants& props) {
        auto name   = pt.get<std::string>("<xmlattr>.name");
        auto type   = pt.get<std::string>("<xmlattr>.type");
        auto access = pt.get<std::string>("<xmlattr>.access", "");
        props.push_back(mc::dict{{"name", name}, {"type", type}, {"access", access}});
    }

    static void get_signal(const bp::ptree& pt, mc::variants& signals) {
        auto name = pt.get<std::string>("<xmlattr>.name");

        mc::variants args;
        for (const auto& child : pt) {
            if (child.first == "arg") {
                get_arg(child.second, args);
            }
        }
        signals.push_back(mc::dict{{"name", name}, {"args", args}});
    }
};

TEST_F(std_interface_test, test_properties) {
    auto value = obj1.invoke("Get", mc::variants{"org.test.TestInterface1", "Value"},
                             properties_interface_name);
    EXPECT_EQ(value, 100);

    auto i1values =
        obj1.invoke("GetAll", mc::variants{"org.test.TestInterface1"}, properties_interface_name);
    EXPECT_EQ(i1values, (mc::dict{{"Value", 100}, {"Name", "Name"}}));

    auto i2values =
        obj2.invoke("GetAll", mc::variants{"org.test.TestInterface2"}, properties_interface_name);
    EXPECT_EQ(i2values, (mc::dict{{"Id", "00101"}, {"Map", mc::dict{{"Key1", 1}, {"Key2", 2}}}}));

    obj1.invoke("Set", mc::variants{"org.test.TestInterface1", "Value", 200},
                properties_interface_name);
    EXPECT_EQ(obj1.m_iface1.m_value, 200);

    EXPECT_EQ(obj1.get_object_path(), "/org/test/TestObject");
    EXPECT_EQ(obj2.get_object_path(), "/org/test/TestObject/Id/00101");
    EXPECT_EQ(obj3.get_object_path(), "/org/test/TestObject/Id/00102");
}

TEST_F(std_interface_test, test_introspect) {
    auto xml = obj1.invoke("Introspect", mc::variants{}, introspectable_interface_name);

    /*
    <!DOCTYPE node PUBLIC "-//freedesktop//DTD D-BUS Object Introspection 1.0//EN"
    "http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd">
    <node>
    <interface name = "Interface1">
            <property name="Value" type="i" access="readwrite" />
            <property name="Name" type="s" access="readwrite" />
            <method name="SetValue">
                    <arg name="arg1" type="i" direction="in" />
            </method>
            <method name="GetValue">
                    <arg name="result" type = "i" direction="out" />
            </method>
            <signal name="ValueChanged" >
                    <arg name="arg1" type="i" />
                    <arg name="arg2" type="i" />
            </signal>
    </interface>
    <node name="Id"/>
    </node>
    */
    auto object   = decode_introspect(xml.get_string());
    auto expected = mc::dict{{
        "Interface1",
        mc::dict{
            {
                "properties",
                mc::variants{mc::dict{{"name", "Value"}, {"type", "i"}, {"access", "readwrite"}},
                             mc::dict{{"name", "Name"}, {"type", "s"}, {"access", "readwrite"}}},
            },
            {
                "methods",
                mc::variants{
                    mc::dict{{"name", "SetValue"},
                             {"args", mc::variants{{mc::dict{
                                          {"name", "arg1"}, {"type", "i"}, {"direction", "in"}}}}}},
                    mc::dict{{"name", "GetValue"},
                             {"args",
                              mc::variants{{mc::dict{
                                  {{"name", "result"}, {"type", "i"}, {"direction", "out"}}}}}}}},
            },
            {
                "signals",
                mc::variants{
                    mc::dict{{"name", "ValueChanged"},
                             {"args", mc::variants{{mc::dict{{"name", "arg1"}, {"type", "i"}}},
                                                   {mc::dict{{"name", "arg2"}, {"type", "i"}}}}}},
                },
            },
        },
    }};
    EXPECT_EQ(object, expected) << "======== xml ========" << std::endl
                                << xml.get_string() << std::endl
                                << "======== object ========" << std::endl
                                << object.to_string() << std::endl
                                << "======== expected ========" << std::endl
                                << expected.to_string();
}
