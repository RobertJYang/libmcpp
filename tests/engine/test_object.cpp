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

namespace {

class TestInterface1 : public mc::engine::interface<TestInterface1> {
public:
    MC_INTERFACE("org.test.TestInterface1")

    int32_t m_value = 0;

    int32_t add(int32_t a) {
        auto old_value = m_value;
        m_value += a;
        value_changed(old_value, m_value);
        return m_value;
    }

    int32_t subtract(int32_t a) {
        auto old_value = m_value;
        m_value -= a;
        value_changed(old_value, m_value);
        return m_value;
    }

    mc::signal<void(int32_t, int32_t)> value_changed;
};

class TestInterface2 : public mc::engine::interface<TestInterface2> {
public:
    MC_INTERFACE("org.test.TestInterface2")

    std::string m_value = "default";

    void set_value(std::string_view value) {
        auto old_value = m_value;
        m_value        = value;
        value_changed(old_value, m_value);
    }

    std::string get_value() const {
        return m_value;
    }

    mc::signal<void(std::string_view, std::string_view)> value_changed;
};

class TestObject : public mc::engine::object<TestObject> {
public:
    MC_OBJECT(TestObject, "/org/test/TestObject", (TestInterface1)(TestInterface2))

    TestObject(mc::engine::core_object* parent = nullptr) : mc::engine::object<TestObject>(parent) {
    }

    int32_t m_prev_value = 0;

    TestInterface1 m_iface1;
    TestInterface2 m_iface2;
};

} // namespace

MC_REFLECT(TestInterface1,
           ((m_value, "value"))((add, "Add"))((subtract, "Subtract"))((value_changed,
                                                                       "value_changed")))
MC_REFLECT(TestInterface2, ((m_value, "value"))((set_value, "SetValue"))((get_value, "GetValue"))(
                               (value_changed, "value_changed")))
MC_REFLECT(TestObject, ((m_iface1, "iface1"))((m_iface2, "iface2")))

class object_test : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }

    TestObject                   obj;
    mc::engine::abstract_object& obj_base = obj;
};

TEST_F(object_test, test_object_metadata) {
    std::vector<std::string_view> expected = {"iface1", "iface2"};

    // 获取静态接口信息
    std::vector<std::string_view> interfaces;
    mc::traits::tuple_for_each(TestObject::get_static_interface_infos(), [&](auto& info) {
        interfaces.emplace_back(info.name);
    });
    EXPECT_EQ(interfaces, expected);

    // 获取运行时接口信息
    std::vector<std::string_view> interfaces1;
    for (auto& info : TestObject::get_interface_infos()) {
        interfaces1.emplace_back(info.second->name);
    }
    std::sort(interfaces1.begin(), interfaces1.end());
    EXPECT_EQ(interfaces1, expected);
}

TEST_F(object_test, test_get_interface) {
    auto iface1 = obj.get_interface("iface1");
    EXPECT_EQ(iface1, &obj.m_iface1);

    auto iface2 = obj.get_interface("iface2");
    EXPECT_EQ(iface2, &obj.m_iface2);
}

TEST_F(object_test, test_property) {
    // value 在两个接口中都有，默认是第一个接口的属性
    obj_base.set_property("value", 100);
    EXPECT_EQ(obj_base.get_property("value"), 100);

    // 指定接口在对象中的属性名称设置接口属性
    obj_base.set_property("value", "hello", "iface2");
    EXPECT_EQ(obj_base.get_property("value", "iface2"), "hello");

    // 指定接口名设置接口属性
    obj_base.set_property("value", "world", "org.test.TestInterface2");
    EXPECT_EQ(obj_base.get_property("value", "org.test.TestInterface2"), "world");
}

TEST_F(object_test, test_method) {
    obj_base.invoke("Add", {100});
    EXPECT_EQ(obj_base.get_property("value"), 100);

    obj_base.invoke("Subtract", {50});
    EXPECT_EQ(obj_base.get_property("value"), 50);

    obj_base.invoke("SetValue", {"hello"});
    EXPECT_EQ(obj_base.get_property("value", "iface2"), "hello");

    // 指定接口名设置接口属性
    obj_base.invoke("SetValue", {"world"}, "org.test.TestInterface2");
    EXPECT_EQ(obj_base.get_property("value", "org.test.TestInterface2"), "world");
}

TEST_F(object_test, test_signal_connect) {
    int old_value = 0;
    int new_value = 0;

    // 连接 iface1 的 value_changed 信号
    auto conn = obj_base.connect(
        "value_changed",
        [&](const mc::variants& args) -> mc::variant {
            old_value = args[0].as<int32_t>();
            new_value = args[1].as<int32_t>();
            return {};
        },
        "iface1");

    EXPECT_TRUE(conn.connected());

    // 触发信号
    obj_base.invoke("Add", {100});
    EXPECT_EQ(old_value, 0);
    EXPECT_EQ(new_value, 100);

    // 断开连接
    conn.disconnect();
    EXPECT_FALSE(conn.connected());

    // 再次触发信号，但槽函数不应被调用
    obj_base.invoke("Add", {50});
    EXPECT_EQ(old_value, 0); // 值没有变化，因为槽函数未被调用
    EXPECT_EQ(new_value, 100);
}

TEST_F(object_test, test_signal_emit) {
    std::string old_value;
    std::string new_value;

    // 连接 iface2 的 value_changed 信号
    auto conn = obj_base.connect(
        "value_changed",
        [&](const mc::variants& args) -> mc::variant {
            old_value = args[0].as<std::string>();
            new_value = args[1].as<std::string>();
            return {};
        },
        "iface2");

    // 通过 emit 直接发送信号
    obj_base.emit("value_changed", {"old_test", "new_test"}, "iface2");

    EXPECT_EQ(old_value, "old_test");
    EXPECT_EQ(new_value, "new_test");

    // 前面直接通过 emit 发送信号，因为没有调用过 SetValue 方法，所以 old_value 仍然为默认值
    obj_base.invoke("SetValue", {"updated_value"}, "iface2");
    EXPECT_EQ(old_value, "default");
    EXPECT_EQ(new_value, "updated_value");
}

TEST_F(object_test, test_connect_by_interface_name) {
    int signal_count = 0;

    // 使用接口名称连接信号
    auto conn = obj_base.connect(
        "value_changed",
        [&](const mc::variants& args) -> mc::variant {
            signal_count++;
            return {};
        },
        "org.test.TestInterface1");

    EXPECT_TRUE(conn.connected());

    // 触发信号
    obj_base.invoke("Add", {100}, "org.test.TestInterface1");
    EXPECT_EQ(signal_count, 1);

    // 再次触发
    obj_base.invoke("Subtract", {50}, "org.test.TestInterface1");
    EXPECT_EQ(signal_count, 2);
}

TEST_F(object_test, test_multiple_connections) {
    int count1 = 0;
    int count2 = 0;

    // 第一个连接
    auto conn1 = obj_base.connect(
        "value_changed",
        [&](const mc::variants& args) -> mc::variant {
            count1++;
            return {};
        },
        "iface1");

    // 第二个连接
    auto conn2 = obj_base.connect(
        "value_changed",
        [&](const mc::variants& args) -> mc::variant {
            count2++;
            return {};
        },
        "iface1");

    // 触发信号
    obj_base.invoke("Add", {100});
    EXPECT_EQ(count1, 1);
    EXPECT_EQ(count2, 1);

    // 断开第一个连接
    conn1.disconnect();

    // 再次触发信号
    obj_base.invoke("Add", {50});
    EXPECT_EQ(count1, 1); // 未增加
    EXPECT_EQ(count2, 2); // 增加了一次

    // 断开第二个连接
    conn2.disconnect();

    // 再次触发信号
    obj_base.invoke("Add", {50});
    EXPECT_EQ(count1, 1);
    EXPECT_EQ(count2, 2);
}

TEST_F(object_test, test_invalid_signal) {
    auto conn = obj_base.connect("invalid_signal", [](const mc::variants& args) -> mc::variant {
        return {};
    });
    EXPECT_FALSE(conn.connected());

    // 尝试触发不存在的信号
    EXPECT_TRUE(obj_base.emit("invalid_signal", {}).is_null());
}

TEST_F(object_test, test_has_interface) {
    // 测试存在的接口
    EXPECT_TRUE(obj.has_interface("iface1"));
    EXPECT_TRUE(obj.has_interface("iface2"));
    EXPECT_TRUE(obj.has_interface("org.test.TestInterface1"));
    EXPECT_TRUE(obj.has_interface("org.test.TestInterface2"));

    // 测试不存在的接口
    EXPECT_FALSE(obj.has_interface("non_existing_interface"));
    EXPECT_FALSE(obj.has_interface("org.test.NonExistingInterface"));
}