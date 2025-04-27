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

class TestInterface : public mc::engine::interface<TestInterface> {
public:
    MC_INTERFACE("org.test.TestInterface")

    TestInterface() = default;
    TestInterface(int32_t value) : m_value(value) {
    }

    // 属性
    int32_t                  m_value   = 0;
    std::string              m_name    = "default";
    bool                     m_enabled = true;
    std::string              m_status  = "idle";
    std::vector<std::string> m_logs;

    // 基本操作方法
    int32_t add(int32_t a) {
        if (!m_enabled) {
            return m_value;
        }

        m_value += a;
        m_status = "added";
        value_changed(m_value, a);
        status_changed(m_status);
        return m_value;
    }

    int32_t subtract(int32_t a) {
        if (!m_enabled) {
            return m_value;
        }

        m_value -= a;
        m_status = "subtracted";
        value_changed(m_value, a);
        status_changed(m_status);
        return m_value;
    }

    void set_name(const std::string& name) {
        if (m_name != name) {
            m_name = name;
            name_changed(m_name);
        }
    }

    std::string get_name() const {
        return m_name;
    }

    void reset() {
        int32_t old_value = m_value;
        m_value           = 0;
        m_name            = "default";
        m_status          = "reset";
        reset_signal(old_value);
        value_changed(m_value, 0);
        status_changed(m_status);
    }

    void set_enabled(bool enabled) {
        if (m_enabled != enabled) {
            m_enabled = enabled;
            m_status  = enabled ? "enabled" : "disabled";
            enabled_changed(m_enabled);
            status_changed(m_status);
        }
    }

    // 日志相关方法
    void log(const std::string& message) {
        m_logs.push_back(message);
        log_added(message);
    }

    void clear_logs() {
        m_logs.clear();
        log_cleared();
    }

    // 信号
    mc::signal<int32_t(int32_t, int32_t)> value_changed;   // 返回新值
    mc::signal<void(const std::string&)>  name_changed;    // 名称改变
    mc::signal<void(const std::string&)>  status_changed;  // 状态改变
    mc::signal<void(int32_t)>             reset_signal;    // 重置信号
    mc::signal<void(bool)>                enabled_changed; // 启用状态改变
    mc::signal<void(const std::string&)>  log_added;       // 添加日志
    mc::signal<void()>                    log_cleared;     // 清空日志
};

} // namespace

// 反射所有属性、方法和信号
MC_REFLECT(
    TestInterface,
    ((m_value, "value"))((m_name, "name"))((m_status, "status"))((m_enabled, "enabled"))(
        (m_logs, "logs"))((add, "Add"))((subtract, "Subtract"))((set_name, "SetName"))((get_name,
                                                                                        "GetName"))(
        (reset, "Reset"))((set_enabled, "SetEnabled"))((log, "Log"))((clear_logs, "ClearLogs"))(
        value_changed)(name_changed)(status_changed)(reset_signal)(enabled_changed)(log_added)(log_cleared))

class interface_test : public ::testing::Test {
protected:
    TestInterface               obj;
    mc::engine::interface_base* iface;

    interface_test() : obj(10) {
    }

    void SetUp() override {
        iface = &obj;
    }

    void TearDown() override {
    }
};

TEST_F(interface_test, test_interface) {
    TestInterface value1(0);
    TestInterface value2(100);

    // 通过反射链接信息号
    mc::engine::interface_base* v1 = &value1;
    mc::engine::interface_base* v2 = &value2;
    v1->connect("value_changed", [v2](mc::variants args) {
        return v2->invoke("Add", args);
    });

    value1.add(1);

    EXPECT_EQ(value2.m_value, 101);

    EXPECT_EQ(v1->get_interface_name(), TestInterface::interface_name);
    EXPECT_EQ(v2->get_interface_name(), TestInterface::interface_name);
}

// 测试方法调用
TEST_F(interface_test, test_method_invoke) {
    TestInterface               value(10);
    mc::engine::interface_base* iface = &value;

    // 测试Add方法
    mc::variant result = iface->invoke("Add", {5});
    EXPECT_EQ(result, 15);
    EXPECT_EQ(value.m_value, 15);

    // 测试Sub方法
    result = iface->invoke("Subtract", {3});
    EXPECT_EQ(result, 12);
    EXPECT_EQ(value.m_value, 12);

    // 测试SetName方法 - 无返回值
    iface->invoke("SetName", {"test_name"});
    EXPECT_EQ(value.m_name, "test_name");

    // 测试GetName方法
    result = iface->invoke("GetName", {});
    EXPECT_EQ(result, "test_name");

    // 测试不存在的方法
    EXPECT_THROW(iface->invoke("NonExistentMethod", {}), mc::bad_function_call_exception);
}

// 测试属性读写
TEST_F(interface_test, test_property) {
    TestInterface               value(42);
    mc::engine::interface_base* iface = &value;

    // 获取属性值
    mc::variant val_prop = iface->get_property("value");
    EXPECT_EQ(val_prop, 42);

    // 设置属性值
    iface->set_property("value", 100);
    EXPECT_EQ(value.m_value, 100);

    // 获取字符串属性
    mc::variant name_prop = iface->get_property("name");
    EXPECT_EQ(name_prop, "default");

    // 设置字符串属性
    iface->set_property("name", "new_name");
    EXPECT_EQ(value.m_name, "new_name");

    // 不存在的属性
    mc::variant non_exist = iface->get_property("non_existent");
    EXPECT_TRUE(non_exist.is_null());
}

// 测试信号连接和触发
TEST_F(interface_test, test_signal_connection) {
    TestInterface               value(0);
    mc::engine::interface_base* iface = &value;

    int signal_called_count = 0;
    int last_value          = 0;
    int last_delta          = 0;

    // 连接value_changed信号
    iface->connect("value_changed", [&](mc::variants args) -> mc::variant {
        signal_called_count++;
        last_value = args[0].as<int32_t>();
        last_delta = args[1].as<int32_t>();
        return {0};
    });

    // 触发信号
    value.add(5);
    EXPECT_EQ(signal_called_count, 1);
    EXPECT_EQ(last_value, 5);
    EXPECT_EQ(last_delta, 5);

    value.add(10);
    EXPECT_EQ(signal_called_count, 2);
    EXPECT_EQ(last_value, 15);
    EXPECT_EQ(last_delta, 10);
}

// 测试多个信号
TEST_F(interface_test, test_multiple_signals) {
    TestInterface               value(10);
    mc::engine::interface_base* iface = &value;

    bool value_changed_called = false;
    bool name_changed_called  = false;
    bool reset_called         = false;

    // 连接多个信号
    iface->connect("value_changed", [&](mc::variants args) -> mc::variant {
        value_changed_called = true;
        return {0};
    });

    iface->connect("name_changed", [&](mc::variants args) -> mc::variant {
        name_changed_called = true;
        return {0};
    });

    iface->connect("reset_signal", [&](mc::variants args) -> mc::variant {
        reset_called = true;
        EXPECT_EQ(args[0], 15);
        return {0};
    });

    // 触发不同的信号
    value.set_name("new_name");
    EXPECT_TRUE(name_changed_called);
    EXPECT_FALSE(value_changed_called);
    EXPECT_FALSE(reset_called);

    // 重置标志
    name_changed_called = false;

    // 触发另一个信号
    value.add(5);
    EXPECT_TRUE(value_changed_called);
    EXPECT_FALSE(name_changed_called);
    EXPECT_FALSE(reset_called);

    // 重置标志
    value_changed_called = false;

    // 触发重置信号
    value.reset();
    EXPECT_TRUE(reset_called);
    EXPECT_TRUE(value_changed_called); // 重置信号也会触发value_changed信号
    EXPECT_FALSE(name_changed_called);

    // 检查重置后的值
    EXPECT_EQ(value.m_value, 0);
    EXPECT_EQ(value.m_name, "default");
}

// 测试信号发射
TEST_F(interface_test, test_emit_signal) {
    TestInterface               value(0);
    mc::engine::interface_base* iface = &value;

    int signal_calls = 0;

    // 连接信号
    value.value_changed.connect([&](int32_t new_val, int32_t delta) {
        signal_calls++;
        return new_val;
    });

    // 通过interface发射信号
    mc::variant result = iface->emit("value_changed", {42, 10});

    // 验证信号被调用
    EXPECT_EQ(signal_calls, 1);
    EXPECT_EQ(result, 42);

    // 尝试发射不存在的信号
    result = iface->emit("non_existent_signal", {});
    EXPECT_TRUE(result.is_null());
}

// 测试有无返回值的信号
TEST_F(interface_test, test_signals_with_and_without_return) {
    TestInterface               value(0);
    mc::engine::interface_base* iface = &value;

    bool void_signal_called = false;

    // 连接无返回值的信号
    iface->connect("name_changed", [&](mc::variants args) {
        void_signal_called = true;
        return mc::variant(); // 无返回值
    });

    // 触发无返回值的信号
    mc::variant void_result = iface->emit("name_changed", {"test"});
    EXPECT_TRUE(void_signal_called);
    EXPECT_TRUE(void_result.is_null());

    // 连接有返回值的信号
    int return_value = 0;
    iface->connect("value_changed", [&](mc::variants args) {
        return_value = args[0].as<int32_t>() * 2;
        return mc::variant(return_value);
    });

    // 触发有返回值的信号
    mc::variant return_result = iface->emit("value_changed", {5, 5});
    EXPECT_EQ(return_result, 10);
    EXPECT_EQ(return_value, 10);
}

// 测试信号连接成链
TEST_F(interface_test, test_signal_chain) {
    TestInterface value1(0);
    TestInterface value2(0);
    TestInterface value3(0);

    mc::engine::interface_base* v1 = &value1;
    mc::engine::interface_base* v2 = &value2;
    mc::engine::interface_base* v3 = &value3;

    // 创建信号链: value1 -> value2 -> value3
    v1->connect("value_changed", [v2](mc::variants args) {
        return v2->invoke("Add", {args[1]});
    });

    v2->connect("value_changed", [v3](mc::variants args) {
        return v3->invoke("Add", {args[1]});
    });

    // 触发第一个信号
    value1.add(5);

    // 验证信号链的传递
    EXPECT_EQ(value1.m_value, 5);
    EXPECT_EQ(value2.m_value, 5);
    EXPECT_EQ(value3.m_value, 5);

    // 再次触发
    value1.add(10);
    EXPECT_EQ(value1.m_value, 15);
    EXPECT_EQ(value2.m_value, 15);
    EXPECT_EQ(value3.m_value, 15);
}

// 测试静态方法
TEST_F(interface_test, test_static_methods) {
    // 测试获取信号
    EXPECT_TRUE(TestInterface::has_signal("value_changed"));
    EXPECT_TRUE(TestInterface::has_signal("name_changed"));
    EXPECT_TRUE(TestInterface::has_signal("reset_signal"));
    EXPECT_FALSE(TestInterface::has_signal("non_existent_signal"));

    const auto* signal_info = TestInterface::get_signal("value_changed");
    EXPECT_NE(signal_info, nullptr);

    const auto* non_existent = TestInterface::get_signal("non_existent_signal");
    EXPECT_EQ(non_existent, nullptr);
}

// 测试参数异常情况
TEST_F(interface_test, test_invalid_arguments) {
    TestInterface               value(10);
    mc::engine::interface_base* iface = &value;

    EXPECT_THROW(iface->invoke("Add", {}), mc::bad_function_call_exception);

    // 提供过多参数（这种情况通常不会抛出异常，但会忽略多余参数）
    mc::variant result = iface->invoke("Add", {5, 10, 15});
    EXPECT_EQ(result, 15);
    EXPECT_EQ(value.m_value, 15);

    // 属性设置类型错误
    EXPECT_FALSE(iface->set_property("value", "not_a_number"));
}

// 测试多个插槽连接到同一信号
TEST_F(interface_test, test_multiple_slots) {
    TestInterface               value(0);
    mc::engine::interface_base* iface = &value;

    int slot1_calls = 0;
    int slot2_calls = 0;

    // 连接第一个插槽
    iface->connect("value_changed", [&](mc::variants args) {
        slot1_calls++;
        return mc::variant{0};
    });

    iface->connect("value_changed", [&](mc::variants args) {
        slot2_calls++;
        return mc::variant{0};
    });

    // 触发信号
    value.add(5);

    // 验证两个插槽都被调用
    EXPECT_EQ(slot1_calls, 1);
    EXPECT_EQ(slot2_calls, 1);
}

// 测试连接管理
TEST_F(interface_test, test_connection_management) {
    TestInterface               value(0);
    mc::engine::interface_base* iface = &value;

    int call_count = 0;

    // 创建连接并保存连接对象
    mc::connection_type connection = iface->connect("value_changed", [&](mc::variants args) {
        call_count++;
        return mc::variant(0);
    });

    // 触发信号，连接应正常工作
    value.add(5);
    EXPECT_EQ(call_count, 1);

    // 断开连接
    connection.disconnect();

    // 再次触发信号，插槽不应被调用
    value.add(5);
    EXPECT_EQ(call_count, 1); // 计数应保持不变

    // 测试连接已断开的信号
    mc::connection_type new_connection = iface->connect("value_changed", [&](mc::variants args) {
        call_count++;
        return mc::variant(0);
    });

    // 触发信号，新连接应工作
    value.add(5);
    EXPECT_EQ(call_count, 2);
}

// 测试接口类型层次结构
TEST_F(interface_test, test_interface_hierarchy) {
    TestInterface value(42);

    // 测试对象类型
    EXPECT_TRUE((std::is_same_v<decltype(value)::object_type, TestInterface>));

    // 测试继承关系
    mc::engine::interface_base*           base_ptr    = &value;
    mc::engine::interface<TestInterface>* derived_ptr = &value;

    EXPECT_EQ(base_ptr, static_cast<mc::engine::interface_base*>(derived_ptr));
}

// 测试空信号的行为
TEST_F(interface_test, test_empty_signal) {
    TestInterface               value(0);
    mc::engine::interface_base* iface = &value;

    // 不连接任何插槽，直接发射信号
    mc::variant result = iface->emit("value_changed", {42, 10});

    // 没有连接的信号应返回空值
    EXPECT_TRUE(result.is_null());

    // 添加一个返回特定值的插槽
    iface->connect("value_changed", [](mc::variants args) {
        return mc::variant(100);
    });

    // 现在信号应返回插槽返回的值
    result = iface->emit("value_changed", {42, 10});
    EXPECT_EQ(result, 100);
}

// 复杂场景测试
TEST_F(interface_test, test_complex_scenario) {
    TestInterface counter(10);
    TestInterface logger;

    mc::engine::interface_base* counter_iface = &counter;
    mc::engine::interface_base* logger_iface  = &logger;

    // 将Counter的状态变化连接到Logger
    counter_iface->connect("status_changed", [logger_iface](mc::variants args) -> mc::variant {
        std::string status      = args[0].as<std::string>();
        std::string log_message = "状态变化为: " + status;
        logger_iface->invoke("Log", {log_message});
        return {0};
    });

    // 将计数变化连接到Logger
    counter_iface->connect("value_changed", [logger_iface](mc::variants args) -> mc::variant {
        int32_t     count       = args[0].as<int32_t>();
        std::string log_message = "计数变化为: " + std::to_string(count);
        logger_iface->invoke("Log", {log_message});
        return {0};
    });

    // 测试初始状态
    EXPECT_EQ(counter.m_value, 10);
    EXPECT_EQ(counter.m_status, "idle");
    EXPECT_TRUE(counter.m_enabled);
    EXPECT_TRUE(logger.m_logs.empty());

    // 测试增加操作
    counter_iface->invoke("Add", {1});
    EXPECT_EQ(counter.m_value, 11);
    EXPECT_EQ(counter.m_status, "added");
    EXPECT_EQ(logger.m_logs.size(), 2);
    EXPECT_EQ(logger.m_logs[0], "计数变化为: 11");
    EXPECT_EQ(logger.m_logs[1], "状态变化为: added");

    // 测试减少操作
    counter_iface->invoke("Subtract", {1});
    EXPECT_EQ(counter.m_value, 10);
    EXPECT_EQ(counter.m_status, "subtracted");
    EXPECT_EQ(logger.m_logs.size(), 4);
    EXPECT_EQ(logger.m_logs[2], "计数变化为: 10");
    EXPECT_EQ(logger.m_logs[3], "状态变化为: subtracted");

    // 测试禁用操作 - 使用SetEnabled方法而不是直接设置属性
    counter_iface->invoke("SetEnabled", {false});
    EXPECT_FALSE(counter.m_enabled);
    EXPECT_EQ(counter.m_status, "disabled");
    EXPECT_EQ(logger.m_logs.size(), 5);
    EXPECT_EQ(logger.m_logs[4], "状态变化为: disabled");

    // 禁用状态下操作应无效
    counter_iface->invoke("Add", {1});
    EXPECT_EQ(counter.m_value, 10);     // 不变
    EXPECT_EQ(logger.m_logs.size(), 5); // 没有新日志

    // 测试重置操作
    counter_iface->invoke("Reset", {});
    EXPECT_EQ(counter.m_value, 0);
    EXPECT_EQ(counter.m_status, "reset");
    EXPECT_EQ(logger.m_logs.size(), 7);

    // 测试属性读取
    mc::variant count_prop = counter_iface->get_property("value");
    EXPECT_EQ(count_prop, 0);

    mc::variant status_prop = counter_iface->get_property("status");
    EXPECT_EQ(status_prop, "reset");

    // 测试属性设置
    counter_iface->set_property("value", 42);
    EXPECT_EQ(counter.m_value, 42);
    EXPECT_EQ(logger.m_logs.size(), 7); // 直接设置属性不触发信号

    // 测试Logger清除
    logger_iface->invoke("ClearLogs", {});
    EXPECT_TRUE(logger.m_logs.empty());

    // 模拟自定义事件处理链
    counter_iface->invoke("SetEnabled", {true});
    counter_iface->connect("enabled_changed", [counter_iface](mc::variants args) {
        bool enabled = args[0].as<bool>();
        if (enabled) {
            return counter_iface->invoke("Add", {1});
        } else {
            return counter_iface->invoke("Reset", {});
        }
    });

    // 禁用应触发重置
    counter_iface->invoke("SetEnabled", {false});
    EXPECT_EQ(counter.m_value, 0);

    // 启用应触发递增
    counter_iface->invoke("SetEnabled", {true});
    EXPECT_EQ(counter.m_value, 1);
}
