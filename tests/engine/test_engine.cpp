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
#include <mc/dbus/connection.h>
#include <mc/engine.h>
#include <mc/singleton.h>
#include <test_utilities/test_base.h>

namespace {

struct test_service : public mc::engine::service {
    test_service() : mc::engine::service("org.openubmc.test_service") {
    }
};

template <typename T>
using property = mc::engine::property<T>;

class test_interface_1 : public mc::engine::interface<test_interface_1> {
public:
    MC_INTERFACE("org.test.test_interface_1")

    property<int32_t>          m_i32;
    property<std::string>      m_str;
    property<std::vector<int>> m_vec;
    int                        m_normal_v;
};

class test_interface_2 : public mc::engine::interface<test_interface_2> {
public:
    MC_INTERFACE("org.test.test_interface_2")

    property<mc::variant> m_variant;
};

class test_object : public mc::engine::object<test_object> {
public:
    MC_OBJECT(test_object, "/org/test/object_1", (test_interface_1)(test_interface_2))

    test_interface_1 m_iface_1;
    test_interface_2 m_iface_2;
};

class engine_test : public mc::test::TestWithEngine {
protected:
    void SetUp() override {
        service.init();
        service.start();
    }

    void TearDown() override {
        service.stop();
    }

    test_service service;
};

} // namespace

MC_REFLECT(test_interface_1,
           ((m_i32, "i32"))((m_str, "str"))((m_vec, "vec"))((m_normal_v, "normal_v")))
MC_REFLECT(test_interface_2, ((m_variant, "variant")))
MC_REFLECT(test_object, ((m_iface_1, "iface_1"))((m_iface_2, "iface_2")))

TEST_F(engine_test, test_engine) {
    auto strand = mc::engine::make_strand();
    auto conn   = mc::dbus::connection::open_session_bus(strand);
    conn->start();

    auto msg   = mc::dbus::message::new_method_call("org.freedesktop.DBus", "/org/freedesktop/DBus",
                                                    "org.freedesktop.DBus", "ListNames");
    auto reply = conn->send_with_reply(std::move(msg), mc::milliseconds(1000));
    ASSERT_TRUE(reply.is_valid() && reply.is_method_return());

    std::set<std::string> names;
    reply >> names;
    EXPECT_GE(names.count("org.openubmc.test_service"), 1);
}

TEST_F(engine_test, test_object_property_changed_sig) {
    auto obj = test_object::create();
    service.register_object(obj);

    mc::mutable_dict values;
    obj->property_changed().connect([&](const mc::variant& value, const auto& prop) {
        values[prop.get_name()] = value;
    });

    obj->m_iface_1.m_i32.set_value(10);
    obj->m_iface_1.m_i32.set_value(20);
    obj->m_iface_1.m_str.set_value("123");
    obj->m_iface_1.m_str.set_value(std::string("456"));
    std::string str = "000";
    obj->m_iface_1.m_str.set_value(str);
    obj->m_iface_1.m_vec.modify([&](auto& vec) {
        vec.push_back(1);
        return true;
    });
    obj->m_iface_2.m_variant.set_value(100);

    // 普通变量修改不会触发信号
    obj->m_iface_1.m_normal_v = 100;

    mc::dict expected = {{"i32", 10},     {"i32", 20},    {"str", "123"},
                         {"str", "456"},  {"str", "000"}, {"vec", mc::variants{1}},
                         {"variant", 100}};
    EXPECT_EQ(values, expected);
    EXPECT_EQ(str, obj->m_iface_1.m_str);
}

TEST_F(engine_test, test_interface_property_changed_sig) {
    auto obj = test_object::create();
    service.register_object(obj);

    mc::mutable_dict values;
    obj->m_iface_1.property_changed().connect([&](const mc::variant& value, const auto& prop) {
        values[prop.get_name()] = value;
    });

    obj->m_iface_1.m_i32.set_value(10);
    obj->m_iface_1.m_str.set_value("123");

    // 接口2的属性修改不会触发接口1的信号
    obj->m_iface_2.m_variant.set_value(100);

    mc::dict expected = {{"i32", 10}, {"str", "123"}};
    EXPECT_EQ(values, expected);
}

TEST_F(engine_test, test_property_changed_sig) {
    auto obj = test_object::create();
    service.register_object(obj);

    // 指定订阅接口1的i32属性
    mc::mutable_dict values;
    obj->m_iface_1.m_i32.property_changed().connect(
        [&](const mc::variant& value, const auto& prop) {
            values[prop.get_name()] = value;
        });

    obj->m_iface_1.m_i32.set_value(10);

    // 接口1的其他属性修改不会触发
    obj->m_iface_1.m_str.set_value("123");

    // 接口2的属性修改不会触发接口1的信号
    obj->m_iface_2.m_variant.set_value(100);

    mc::dict expected = {{"i32", 10}};
    EXPECT_EQ(values, expected);
}

TEST_F(engine_test, test_property_changed_sig_use_abstract_object) {
    auto obj = test_object::create();
    service.register_object(obj);

    // 1：通过引擎从全局对象表里面找到对象
    auto engine = mc::engine::get_engine();
    auto table  = engine.find_table("object_tree");
    EXPECT_NE(table, nullptr);

    // 2：通过路径全局对象表里面找到对象
    auto result = table->find_object(mc::engine::by_path::field == obj->get_object_path());
    EXPECT_EQ(result, obj.get());
    auto* res_obj = static_cast<mc::engine::abstract_object*>(result.get());

    mc::mutable_dict values;
    res_obj->property_changed().connect([&](const mc::variant& value, const auto& prop) {
        values[prop.get_name()] = value;
    });

    res_obj->set_property("i32", 10);

    mc::dict expected = {{"i32", 10}};
    EXPECT_EQ(values, expected);
}
