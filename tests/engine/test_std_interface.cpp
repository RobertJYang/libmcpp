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
#include <chrono>
#include <thread>

// 解析xml
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

namespace tests::engine::std_interface {
class TestInterface1 : public mc::engine::interface<TestInterface1> {
public:
    MC_INTERFACE("org.test.std.TestInterface1")

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
    MC_INTERFACE("org.test.std.TestInterface2")

    std::string                     m_id;
    std::map<std::string, uint32_t> m_map;
};

class TestObject1 : public mc::engine::object<TestObject1> {
public:
    MC_OBJECT(TestObject1, "TestObject", "/org/test/TestObject", (TestInterface1))

    TestInterface1 m_iface1;
};

class TestObject2 : public mc::engine::object<TestObject2> {
public:
    MC_OBJECT(TestObject2, "TestObject", "Id/${Id}", (TestInterface2))

    ~TestObject2() {
    }

    TestInterface2 m_iface2;
};

class TestInterfaceA : public mc::engine::interface<TestInterfaceA> {
public:
    MC_INTERFACE("org.openubmc.test_interface_a")

    int32_t add(int32_t a, int32_t b) {
        return a + b;
    }

    void set_num(uint8_t num) {
        m_num = num;
    }

    void set_str(std::string str) {
        m_str = str;
    }

    std::tuple<uint8_t, std::string> get_num_and_str() {
        return std::make_tuple(m_num.value(), m_str.value());
    }

    mc::engine::property<uint8_t>     m_num{100};
    mc::engine::property<std::string> m_str{"default"};
};

class TestInterfaceB : public mc::engine::interface<TestInterfaceB> {
public:
    MC_INTERFACE("org.openubmc.test_interface_b")

    int32_t increment() {
        m_cnt.set_value(m_cnt.value() + 1);
        return m_cnt.value();
    }

    mc::engine::property<int32_t>              m_cnt{0};
    mc::engine::property<std::vector<uint8_t>> m_arr{std::vector<uint8_t>{1, 2, 3}};
};

class TestObjectA : public mc::engine::object<TestObjectA> {
public:
    MC_OBJECT(TestObjectA, "TestObjectA", "/org/openubmc/test_object_a", (TestInterfaceA))

    void init() {
        set_object_name("TestObjectA");
    }

    TestInterfaceA m_iface;
};

class TestObjectB : public mc::engine::object<TestObjectB> {
public:
    MC_OBJECT(TestObjectB, "TestObjectB", "/org/openubmc/test_object_b", (TestInterfaceB))

    void init() {
        set_object_name("TestObjectB");
    }

    TestInterfaceB m_iface;
};

class TestInterfaceC : public mc::engine::interface<TestInterfaceC> {
public:
    MC_INTERFACE("org.openubmc.test_interface_c")

    mc::engine::property<int32_t> m_prop1{0};
};

class TestObjectC : public mc::engine::object<TestObjectC> {
public:
    MC_OBJECT(TestObjectC, "TestObjectC", "/org/openubmc/test_object_c", (TestInterfaceC))

    void init() {
        set_object_name("TestObjectC");
    }

    TestInterfaceC m_iface;
};

struct test_service_1 : public mc::engine::service {
    test_service_1()
        : mc::engine::service("org.openubmc.test_service_1") {
    }

    bool init(mc::dict args = {}) override {
        mc::dict args_mut(args);
        args_mut["service_path"] = "/org/openubmc/test_service_1";
        args_mut["service_name"] = "org.openubmc.test_service_1";
        return mc::engine::service::init(args_mut);
    }

    bool start() override {
        if (!mc::engine::service::start()) {
            return false;
        }

        m_obj_a = mc::make_shared<TestObjectA>();
        m_obj_a->init();
        register_object(*m_obj_a);
        return true;
    }

    void init_obj_c() {
        m_obj_c = mc::make_shared<TestObjectC>();
        m_obj_c->init();
        register_object(*m_obj_c);
    }

    void remove_obj_c() {
        unregister_object(m_obj_c->get_object_path());
        m_obj_c.reset();
    }

    mc::shared_ptr<TestObjectA> m_obj_a;
    mc::shared_ptr<TestObjectC> m_obj_c;
};

struct test_service_2 : public mc::engine::service {
    test_service_2()
        : mc::engine::service("org.openubmc.test_service_2") {
    }

    bool init(mc::dict args = {}) override {
        mc::dict args_mut(args);
        args_mut["service_path"] = "/org/openubmc/test_service_2";
        args_mut["service_name"] = "org.openubmc.test_service_2";
        return mc::engine::service::init(args_mut);
    }

    bool start() override {
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

} // namespace tests::engine::std_interface

MC_REFLECT(tests::engine::std_interface::TestInterface1,
           ((m_value, "Value"))((m_name, "Name"))((set_value, "SetValue"))((get_value, "GetValue"))(
               (m_value_changeds, "ValueChanged")))
MC_REFLECT(tests::engine::std_interface::TestInterface2, ((m_id, "Id"))((m_map, "Map")))
MC_REFLECT(tests::engine::std_interface::TestObject1, ((m_iface1, "Interface1")))
MC_REFLECT(tests::engine::std_interface::TestObject2, ((m_iface2, "Interface2")))

MC_REFLECT(tests::engine::std_interface::TestInterfaceA, ((add, "Add"))((set_num, "SetNum"))((set_str, "SetStr"))(
                                                             (get_num_and_str, "GetNumAndStr"))((m_num, "Num"))((m_str, "Str")))
MC_REFLECT(tests::engine::std_interface::TestInterfaceB, ((increment, "Increment"))((m_cnt, "Cnt"))((m_arr, "Arr")))
MC_REFLECT(tests::engine::std_interface::TestObjectA, ((m_iface, "InterfaceA")))
MC_REFLECT(tests::engine::std_interface::TestObjectB, ((m_iface, "InterfaceB")))
MC_REFLECT(tests::engine::std_interface::TestInterfaceC, ((m_prop1, "Prop1")))
MC_REFLECT(tests::engine::std_interface::TestObjectC, ((m_iface, "InterfaceC")))

namespace bp = boost::property_tree;
using namespace tests::engine::std_interface;
using namespace mc::engine;

static test_service_1*   service_1;
static test_service_2*   service_2;
static constexpr int64_t CALL_TIMEOUT = 1000;

namespace {

/**
 * @brief 轮询等待有效的 D-Bus 方法返回
 */
template <typename Builder>
mc::dbus::message wait_valid_reply(mc::dbus::connection& conn, Builder&& builder,
                                   mc::milliseconds timeout = mc::milliseconds(2000),
                                   int max_attempts = 5,
                                   mc::milliseconds retry_delay = mc::milliseconds(100)) {
    mc::dbus::message reply;
    for (int attempt = 0; attempt < max_attempts; ++attempt) {
        auto msg = builder();
        reply    = conn.send_with_reply(std::move(msg), timeout);
        if (reply.is_valid() && reply.is_method_return()) {
            return reply;
        }
        std::this_thread::sleep_for(static_cast<std::chrono::milliseconds>(retry_delay));
    }
    return reply;
}

} // namespace

class std_interface_test : public mc::test::TestWithEngine {
protected:
    mc::shared_ptr<TestObject1> root;
    TestObject2*                child_obj2;
    TestObject2*                child_obj3;

    ~std_interface_test() {
    }

    static void SetUpTestSuite() {
        mc::test::TestWithEngine::SetUpTestSuite();

        service_1 = new test_service_1();
        service_2 = new test_service_2();
        service_1->init();
        service_2->init();
        service_1->start();
        service_2->start();
    }

    static void TearDownTestSuite() {
        service_1->stop();
        service_2->stop();
        delete service_1;
        delete service_2;

        mc::test::TestWithEngine::TearDownTestSuite();
    }

    void SetUp() override {
        mc::test::TestBase::SetUp();

        root                   = mc::make_shared<TestObject1>();
        root->m_iface1.m_value = 100;
        root->m_iface1.m_name  = "Name";

        child_obj2 = new TestObject2();
        child_obj2->set_parent(root);
        child_obj2->m_iface2.m_id  = "00101";
        child_obj2->m_iface2.m_map = {{"Key1", 1}, {"Key2", 2}};

        child_obj3 = new TestObject2();
        child_obj3->set_parent(root);
        child_obj3->m_iface2.m_id  = "00102";
        child_obj3->m_iface2.m_map = {{"Key3", 3}, {"Key4", 4}};

        child_obj2->set_owner(root.get());
        child_obj3->set_owner(root.get());
    }

    void TearDown() override {
        mc::test::TestBase::TearDown();
    }

    static mc::dict decode_introspect(const std::string& xml) {
        std::stringstream ss(xml);
        bp::ptree         pt;

        EXPECT_NO_THROW(bp::read_xml(ss, pt));

        mc::dict object;
        for (const auto& child : pt.get_child("node")) {
            if (child.first == "interface") {
                mc::dict interface;
                get_interface(child.second, interface);

                auto name    = child.second.get<std::string>("<xmlattr>.name");
                object[name] = interface;
            }
        }

        return object;
    }

    static void get_interface(const bp::ptree& pt, mc::dict& object) {
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
        auto     type      = pt.get<std::string>("<xmlattr>.type");
        auto     direction = pt.get<std::string>("<xmlattr>.direction", "");
        mc::dict arg       = {{"type", type}};
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
    auto value = root->invoke("Get", mc::variants{"org.test.std.TestInterface1", "Value"},
                              properties_interface_name);
    EXPECT_EQ(value, 100);

    auto i1values =
        root->invoke("GetAll", mc::variants{"org.test.std.TestInterface1"}, properties_interface_name);
    EXPECT_EQ(i1values, (mc::dict{{"Value", 100}, {"Name", "Name"}}));

    auto i2values = child_obj2->invoke("GetAll", mc::variants{"org.test.std.TestInterface2"},
                                       properties_interface_name);
    EXPECT_EQ(i2values, (mc::dict{{"Id", "00101"}, {"Map", mc::dict{{"Key1", 1}, {"Key2", 2}}}}));

    root->invoke("Set", mc::variants{"org.test.std.TestInterface1", "Value", 200},
                 properties_interface_name);
    EXPECT_EQ(root->m_iface1.m_value, 200);

    EXPECT_EQ(root->get_object_path(), "/org/test/TestObject");
    EXPECT_EQ(child_obj2->get_object_path(), "/org/test/TestObject/Id/00101");
    EXPECT_EQ(child_obj3->get_object_path(), "/org/test/TestObject/Id/00102");
}

TEST_F(std_interface_test, test_introspect) {
    auto xml = root->invoke("Introspect", mc::variants{}, introspectable_interface_name);

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
    mc::dict object = decode_introspect(xml.get_string());
    EXPECT_TRUE(object.contains(mc::engine::properties_interface_name));
    EXPECT_TRUE(object.contains(mc::engine::introspectable_interface_name));
    EXPECT_TRUE(object.contains(mc::engine::peer_interface_name));
    EXPECT_TRUE(object.contains(mc::engine::object_manager_interface_name));
    object.erase(mc::engine::properties_interface_name);
    object.erase(mc::engine::introspectable_interface_name);
    object.erase(mc::engine::peer_interface_name);
    object.erase(mc::engine::object_manager_interface_name);
    object.erase(mc::engine::common_properties_name);

    auto expected = mc::dict{{
        "org.test.std.TestInterface1",
        mc::dict{
            {
                "properties",
                mc::variants{mc::dict{{"name", "Value"}, {"type", "i"}, {"access", "readwrite"}},
                             mc::dict{{"name", "Name"}, {"type", "s"}, {"access", "readwrite"}}},
            },
            {
                "methods",
                mc::variants{mc::dict{{"name", "SetValue"},
                                      {"args", mc::variants{mc::dict{{"type", "i"},
                                                                     {"direction", "in"}}}}},
                             mc::dict{{"name", "GetValue"},
                                      {"args", mc::variants{mc::dict{{"type", "i"},
                                                                     {"direction", "out"}}}}}},
            },
            {
                "signals",
                mc::variants{
                    mc::dict{{"name", "ValueChanged"},
                             {"args",
                              mc::variants{mc::dict{{"type", "i"}}, mc::dict{{"type", "i"}}}}},
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

TEST_F(std_interface_test, TestGetWithContext) {
    auto conn = mc::dbus::connection::open_session_bus(mc::get_io_context());
    conn.start();
    std::map<std::string, std::string> ctx;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto reply = wait_valid_reply(
        conn,
        [&]() {
    auto msg =
        mc::dbus::message::new_method_call("org.openubmc.test_service_2",
                                           "/org/openubmc/test_object_b",
                                           "bmc.kepler.Object.Properties",
                                           "GetWithContext");
    msg.set_sender("bmc.kepler.test_client");
    msg.set_serial(1);
            auto writer = msg.writer();
    writer << ctx << "org.openubmc.test_interface_b" << "Arr";
            return msg;
        });
    ASSERT_TRUE(reply.is_valid() && reply.is_method_return())
        << "reply_valid=" << reply.is_valid()
        << " reply_type=" << static_cast<int>(reply.get_type())
        << " reply_error=" << (reply.is_error() ? reply.get_error_name() : "");
    mc::variant result;
    reply >> result;
    ASSERT_TRUE(result.is_array());
    auto arr = result.as_array();
    ASSERT_EQ(arr.size(), 3);
    ASSERT_EQ(arr[0].as_uint8(), 1);
    ASSERT_EQ(arr[1].as_uint8(), 2);
    ASSERT_EQ(arr[2].as_uint8(), 3);
}

TEST_F(std_interface_test, TestSetWithContext) {
    auto conn = mc::dbus::connection::open_session_bus(mc::get_io_context());
    conn.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::map<std::string, std::string> ctx;
    mc::variants                       arr;
    arr.push_back(4);
    arr.push_back(5);
    arr.push_back(6);

    auto setter_reply = wait_valid_reply(
        conn,
        [&]() {
            auto msg =
                mc::dbus::message::new_method_call(
                    "org.openubmc.test_service_2",
                    "/org/openubmc/test_object_b", "bmc.kepler.Object.Properties", "SetWithContext");
            msg.set_sender("bmc.kepler.test_client");
            auto writer = msg.writer();
    writer << ctx << "org.openubmc.test_interface_b" << "Arr" << arr;
            return msg;
        });
    ASSERT_TRUE(setter_reply.is_valid() && setter_reply.is_method_return())
        << "reply_valid=" << setter_reply.is_valid()
        << " reply_type=" << static_cast<int>(setter_reply.get_type())
        << " reply_error=" << (setter_reply.is_error() ? setter_reply.get_error_name() : "");

    auto getter_reply = wait_valid_reply(
        conn,
        [&]() {
            auto msg = mc::dbus::message::new_method_call("org.openubmc.test_service_2",
                                             "/org/openubmc/test_object_b",
                                             "bmc.kepler.Object.Properties",
                                             "GetWithContext");
    msg.set_sender("bmc.kepler.test_client");
            auto writer = msg.writer();
    writer << ctx << "org.openubmc.test_interface_b" << "Arr";
            return msg;
        });
    ASSERT_TRUE(getter_reply.is_valid() && getter_reply.is_method_return())
        << "reply_valid=" << getter_reply.is_valid()
        << " reply_type=" << static_cast<int>(getter_reply.get_type())
        << " reply_error=" << (getter_reply.is_error() ? getter_reply.get_error_name() : "");

    mc::variant result;
    getter_reply >> result;
    ASSERT_TRUE(result.is_array());
    auto reply_arr = result.as_array();
    ASSERT_EQ(reply_arr.size(), 3);
    ASSERT_EQ(reply_arr[0].as_uint8(), 4);
    ASSERT_EQ(reply_arr[1].as_uint8(), 5);
    ASSERT_EQ(reply_arr[2].as_uint8(), 6);
}

TEST_F(std_interface_test, PropertiesInterfaceWithoutObject) {
    auto& props = properties_interface::get_instance();
    EXPECT_TRUE(props.get("org.openubmc.test_interface_a", "Num").is_null());
    auto all_props = props.get_all("org.openubmc.test_interface_a");
    EXPECT_TRUE(all_props.empty());
    EXPECT_NO_THROW(props.set("org.openubmc.test_interface_a", "Num", mc::variant(1)));
}

// TEST_F(std_interface_test, PropertiesInterfaceSetAndCommonProperties) {
//     TestObjectA obj;
//     obj.init();
//     TestObjectA parent;
//     parent.init();
//     parent.set_object_path("/org/openubmc/parent");
//     obj.set_object_path("/org/openubmc/parent/child");
//     obj.set_owner(&parent);

//     mc::engine::object_call_stack::context ctx{obj.get_service(), obj};
//     auto&                                  props = properties_interface::get_instance();

//     auto value = props.get("org.openubmc.test_interface_a", "Num");
//     EXPECT_EQ(value.as_uint64(), static_cast<uint64_t>(100));

//     props.set("org.openubmc.test_interface_a", "Num", mc::variant(uint8_t(77)));
//     EXPECT_EQ(obj.m_iface.m_num.value(), 77);

//     auto object_name =
//         props.get(mc::engine::common_properties_name, "ObjectName").as_string();
//     EXPECT_EQ(object_name, "TestObjectA");

//     auto common_dict = props.get_all(mc::engine::common_properties_name);
//     EXPECT_EQ(common_dict.at("ParentPath").as_string(), "/org/openubmc/parent");
//     EXPECT_EQ(common_dict.at("ClassName").as_string(), "TestObjectA");

//     props.set(mc::engine::common_properties_name, "ObjectName", mc::variant("Changed"));
//     EXPECT_EQ(obj.get_object_name(), "TestObjectA");
// }

// TEST_F(std_interface_test, CommonPropertiesContextHelpers) {
//     TestObjectA obj;
//     obj.init();
//     TestObjectA parent;
//     parent.init();
//     parent.set_object_path("/org/openubmc/test_parent");
//     obj.set_object_path("/org/openubmc/test_parent/child");
//     obj.set_owner(&parent);

//     // 压栈对象，验证 common_properties 的上下文接口
//     mc::engine::object_call_stack::context ctx{nullptr, obj};
//     auto& common_props = mc::engine::common_properties_interface::get_instance();

//     auto parent_path = common_props.get("ParentPath").as_string();
//     EXPECT_EQ(parent_path, "/org/openubmc/test_parent");

//     auto all_props = common_props.get_all();
//     EXPECT_EQ(all_props.at("ObjectName").as_string(), "TestObjectA");

//     // GetWithContext 传入 common 接口应直接返回通用属性
//     auto ctx_value =
//         common_props.get_with_context({}, mc::engine::common_properties_name, "ClassName");
//     EXPECT_EQ(ctx_value.as_string(), "TestObjectA");

//     // SetWithContext 在 common 接口下应忽略写入
//     common_props.set_with_context({}, mc::engine::common_properties_name, "ParentPath",
//                                   mc::variant("/tmp/ignored"));
//     EXPECT_EQ(common_props.get("ParentPath").as_string(), "/org/openubmc/test_parent");
// }

TEST_F(std_interface_test, CommonPropertiesSetWithoutContextIsSafe) {
    // 无对象上下文时 set_with_context 应安全返回
    auto& common_props = mc::engine::common_properties_interface::get_instance();
    EXPECT_NO_THROW(common_props.set_with_context({}, "org.test.custom", "Any", mc::variant(1)));
}

// TEST_F(std_interface_test, ObjectManagerEnumeratesChildren) {
//     TestObjectA parent;
//     parent.init();
//     parent.set_object_path("/org/openubmc/test_parent");

//     TestObjectA child;
//     child.init();
//     child.set_object_path("/org/openubmc/test_parent/child");
//     child.set_parent(&parent);
//     child.set_owner(&parent);

//     // 压栈后调用对象管理接口，确保 visitor 各分支被覆盖
//     // 在 context 存在时提取所有需要的数据，确保生命周期安全
//     std::string child_path;
//     size_t      managed_size = 0;
//     bool        has_interface = false;
//     {
//         mc::engine::object_call_stack::context ctx{nullptr, parent};
//         auto&                               mgr = mc::engine::object_manager_interface::get_instance();
//         auto                                managed = mgr.get_managed_objects();
        
//         managed_size = managed.size();
//         if (managed_size > 0) {
//             const auto& entry = *managed.begin();
//             child_path = entry.first.str(); // 提取字符串，确保生命周期安全
//             has_interface = entry.second.count("org.openubmc.test_interface_a") > 0;
//         }
//     } // ctx 在这里销毁

//     ASSERT_EQ(managed_size, 1U);
//     EXPECT_EQ(child_path, "/org/openubmc/test_parent/child");
//     EXPECT_TRUE(has_interface);
// }