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

namespace test_interface_inherit {

// 基础接口
class BaseInterface : public mc::engine::interface<BaseInterface> {
public:
    MC_INTERFACE("org.test.BaseInterface")

    int32_t                            m_base_value{100};
    std::string                        m_common_prop{"base"};
    mc::signal<void(int32_t, int32_t)> m_base_signal;
    mc::signal<void(std::string)>      m_common_signal;

    // 接口方法
    std::string base_method(int32_t val) {
        return "base:" + std::to_string(val);
    }

    // 公共方法 - 在子接口中被覆盖
    std::string common_method(const std::string& val) {
        return "base:" + val;
    }
};

// 中间接口，继承自基础接口
class MiddleInterface : public mc::engine::interface<MiddleInterface, BaseInterface> {
public:
    MC_INTERFACE("org.test.MiddleInterface")

    std::string                   m_middle_value{"middle"};
    std::string                   m_common_prop{"middle"}; // 覆盖基础接口属性
    mc::signal<void(std::string)> m_middle_signal;
    mc::signal<void(std::string)> m_common_signal; // 覆盖基础接口信号

    // 接口方法
    double middle_method(const std::string& val) {
        return std::stod(val) * 2.0;
    }

    // 覆盖基础接口方法
    std::string common_method(const std::string& val) {
        return "middle:" + val;
    }
};

// 扩展接口，继承自中间接口
class ExtendedInterface : public mc::engine::interface<ExtendedInterface, MiddleInterface> {
public:
    MC_INTERFACE("org.test.ExtendedInterface")

    double                        m_extended_value{3.14};
    std::string                   m_common_prop{"extended"}; // 覆盖中间接口属性
    mc::signal<void(double)>      m_extended_signal;
    mc::signal<void(std::string)> m_common_signal; // 覆盖中间接口信号

    // 接口方法
    bool extended_method(double val) {
        return val > 0;
    }

    // 再次覆盖公共方法
    std::string common_method(const std::string& val) {
        return "extended:" + val;
    }
};

// 测试对象，实现扩展接口
class TestObject : public mc::engine::object<TestObject> {
public:
    MC_OBJECT(TestObject, "TestObject", "/org/test/TestObject", (ExtendedInterface))

    // 对象属性覆盖接口属性
    std::string                   m_common_prop{"object"};
    ExtendedInterface             m_iface;
    mc::signal<void(std::string)> m_common_signal; // 覆盖中间接口信号

    // 对象方法覆盖接口方法
    std::string common_method(const std::string& val) {
        return "object:" + val;
    }
};

} // namespace test_interface_inherit

// 反射定义
MC_REFLECT(test_interface_inherit::BaseInterface,
           ((m_base_value, "BaseValue"))((m_common_prop, "CommonProp"))(
               (m_base_signal, "BaseSignal"))((m_common_signal, "CommonSignal"))(
               (base_method, "BaseMethod"))((common_method, "CommonMethod")))

MC_REFLECT(test_interface_inherit::MiddleInterface,
           ((m_middle_value, "MiddleValue"))((m_common_prop, "CommonProp")) // 覆盖基础接口的属性
           ((m_middle_signal, "MiddleSignal"))((m_common_signal,
                                                "CommonSignal"))               // 覆盖基础接口的信号
           ((middle_method, "MiddleMethod"))((common_method, "CommonMethod"))) // 覆盖基础接口的方法

MC_REFLECT(
    test_interface_inherit::ExtendedInterface,
    ((m_extended_value, "ExtendedValue"))((m_common_prop, "CommonProp"))       // 覆盖中间接口的属性
    ((m_extended_signal, "ExtendedSignal"))((m_common_signal, "CommonSignal")) // 覆盖中间接口的信号
    ((extended_method, "ExtendedMethod"))((common_method, "CommonMethod")))    // 覆盖中间接口的方法

MC_REFLECT(test_interface_inherit::TestObject,
           ((m_iface, "iface"))((m_common_prop, "CommonProp"))                   // 覆盖接口的属性
           ((common_method, "CommonMethod"))((m_common_signal, "CommonSignal"))) // 覆盖接口的方法

using namespace test_interface_inherit;

class interface_inherit_test : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }

    TestObject obj;
};

// 测试接口属性继承
TEST_F(interface_inherit_test, test_interface_property_inherit) {
    // 通过接口直接访问属性
    EXPECT_EQ(obj.m_iface.m_base_value, 100);
    EXPECT_EQ(obj.m_iface.m_middle_value, "middle");
    EXPECT_EQ(obj.m_iface.m_extended_value, 3.14);
    EXPECT_EQ(obj.m_iface.m_common_prop, "extended"); // 最终应该是扩展接口的值
}

// 测试通过 abstract_interface 访问属性
TEST_F(interface_inherit_test, test_abstract_interface) {
    mc::engine::abstract_interface* base_iface = &obj.m_iface;

    // 测试能否通过抽象接口获取所有属性（包括继承的）
    EXPECT_EQ(base_iface->get_property("BaseValue"), 100);
    EXPECT_EQ(base_iface->get_property("MiddleValue"), "middle");
    EXPECT_EQ(base_iface->get_property("ExtendedValue"), 3.14);
    EXPECT_EQ(base_iface->get_property("CommonProp"), "extended"); // 应该是扩展接口的值

    // 测试设置属性
    base_iface->set_property("BaseValue", 200);
    EXPECT_EQ(obj.m_iface.m_base_value, 200);

    base_iface->set_property("MiddleValue", "new_middle");
    EXPECT_EQ(obj.m_iface.m_middle_value, "new_middle");

    base_iface->set_property("ExtendedValue", 6.28);
    EXPECT_EQ(obj.m_iface.m_extended_value, 6.28);

    base_iface->set_property("CommonProp", "new_common");
    EXPECT_EQ(obj.m_iface.m_common_prop, "new_common");
}

// 测试通过 abstract_object 访问属性
TEST_F(interface_inherit_test, test_abstract_object) {
    mc::engine::abstract_object* base_obj = &obj;

    // 测试不指定接口名时，访问对象属性
    EXPECT_EQ(base_obj->get_property("CommonProp"), "object");

    // 测试指定接口名时，访问接口属性
    EXPECT_EQ(base_obj->get_property("BaseValue"), 100);
    EXPECT_EQ(base_obj->get_property("MiddleValue"), "middle");
    EXPECT_EQ(base_obj->get_property("ExtendedValue"), 3.14);
    EXPECT_EQ(base_obj->get_property("CommonProp", "org.test.ExtendedInterface"), "extended");

    // 测试设置对象属性
    base_obj->set_property("CommonProp", "new_object_value");
    EXPECT_EQ(obj.m_common_prop, "new_object_value");
    EXPECT_EQ(obj.m_iface.m_common_prop, "extended"); // 接口属性不变

    // 测试设置接口属性
    base_obj->set_property("CommonProp", "new_interface_value", "org.test.ExtendedInterface");
    EXPECT_EQ(obj.m_common_prop, "new_object_value");            // 对象属性不变
    EXPECT_EQ(obj.m_iface.m_common_prop, "new_interface_value"); // 接口属性改变
}

// 测试 get_all_properties 方法
TEST_F(interface_inherit_test, test_get_all_properties) {
    obj.m_iface.m_base_value     = 123;
    obj.m_iface.m_middle_value   = "test_middle";
    obj.m_iface.m_extended_value = 9.99;
    obj.m_iface.m_common_prop    = "test_common";
    obj.m_common_prop            = "object_common";

    // 测试通过接口获取所有属性
    mc::engine::abstract_interface* iface       = obj.get_interface("org.test.ExtendedInterface");
    mc::dict                        iface_props = iface->get_all_properties();

    mc::dict expected_iface_props = {{"BaseValue", 123},
                                     {"MiddleValue", "test_middle"},
                                     {"ExtendedValue", 9.99},
                                     {"CommonProp", "test_common"}};

    EXPECT_EQ(iface_props, expected_iface_props) << iface_props.to_string() << "\n"
                                                 << expected_iface_props.to_string();

    // 测试通过对象获取指定接口的所有属性
    mc::dict obj_iface_props = obj.get_all_properties("org.test.ExtendedInterface");
    EXPECT_EQ(obj_iface_props, expected_iface_props) << obj_iface_props.to_string() << "\n"
                                                     << expected_iface_props.to_string();

    // 测试通过对象获取所有属性（包括对象自身的属性）
    mc::dict all_props = obj.get_all_properties({}, mc::engine::property_options::with_object_property);

    mc::dict expected_all_props = {
        {"org.test.ExtendedInterface", mc::dict{{"BaseValue", 123},
                                                {"MiddleValue", "test_middle"},
                                                {"ExtendedValue", 9.99},
                                                {"CommonProp", "test_common"}}},
        // 包含对象级别的属性
        {"CommonProp", "object_common"},
        {"path", "/org/test/TestObject"},
        {"class_name", "TestObject"},
        {"object_name", ""},
        {"position", ""},
        {"object_id", 0}};

    EXPECT_EQ(all_props, expected_all_props) << all_props.to_string() << "\n"
                                             << expected_all_props.to_string();
}

// 测试属性存在性检查
TEST_F(interface_inherit_test, test_property_existence) {
    mc::engine::abstract_object* base_obj = &obj;

    // 测试对象属性
    EXPECT_TRUE(base_obj->has_property("CommonProp"));
    EXPECT_FALSE(base_obj->has_property("NonExistentProperty"));

    // 测试接口属性
    EXPECT_TRUE(base_obj->has_property("BaseValue"));
    EXPECT_TRUE(base_obj->has_property("MiddleValue"));
    EXPECT_TRUE(base_obj->has_property("ExtendedValue"));
    EXPECT_TRUE(base_obj->has_property("CommonProp"));
    EXPECT_FALSE(base_obj->has_property("NonExistentProperty"));

    // 测试接口存在性
    EXPECT_TRUE(base_obj->has_interface("org.test.ExtendedInterface"));
    EXPECT_FALSE(base_obj->has_interface("org.test.NonExistentInterface"));
}

// 测试信号连接和触发
TEST_F(interface_inherit_test, test_signals) {
    {
        bool base_signal_called     = false;
        bool middle_signal_called   = false;
        bool extended_signal_called = false;

        // 连接基础接口信号
        auto con1 = obj.m_iface.m_base_signal.connect([&](int32_t a, int32_t b) {
            base_signal_called = true;
            EXPECT_EQ(a, 1);
            EXPECT_EQ(b, 2);
        });

        // 连接中间接口信号
        auto con2 = obj.m_iface.m_middle_signal.connect([&](const std::string& s) {
            middle_signal_called = true;
            EXPECT_EQ(s, "test");
        });

        // 连接扩展接口信号
        auto con3 = obj.m_iface.m_extended_signal.connect([&](double d) {
            extended_signal_called = true;
            EXPECT_EQ(d, 3.14);
        });

        // 触发信号
        obj.m_iface.m_base_signal(1, 2);
        obj.m_iface.m_middle_signal("test");
        obj.m_iface.m_extended_signal(3.14);

        EXPECT_TRUE(base_signal_called);
        EXPECT_TRUE(middle_signal_called);
        EXPECT_TRUE(extended_signal_called);

        con1.disconnect();
        con2.disconnect();
        con3.disconnect();
    }

    {
        // 测试通过抽象对象连接信号
        bool                         abstract_signal_called = false;
        mc::engine::abstract_object* base_obj               = &obj;

        base_obj->connect("BaseSignal", [&](const mc::variants& args) -> mc::variant {
            abstract_signal_called = true;
            EXPECT_EQ(args.size(), 2);
            EXPECT_EQ(args[0], 3);
            EXPECT_EQ(args[1], 4);
            return {};
        });

        // 触发信号
        base_obj->emit("BaseSignal", {3, 4});

        EXPECT_TRUE(abstract_signal_called);
    }
}

// 测试方法的覆盖关系
TEST_F(interface_inherit_test, test_method_override) {
    // 直接调用对象方法和接口方法
    EXPECT_EQ(obj.common_method("test"), "object:test");           // 对象方法
    EXPECT_EQ(obj.m_iface.common_method("test"), "extended:test"); // 接口方法(最终覆盖版本)
    EXPECT_EQ(obj.m_iface.base_method(42), "base:42");             // 基础接口方法
    EXPECT_EQ(obj.m_iface.middle_method("21"), 42.0);              // 中间接口方法
    EXPECT_TRUE(obj.m_iface.extended_method(10.0));                // 扩展接口方法
}

// 测试通过 abstract_interface 调用方法
TEST_F(interface_inherit_test, test_abstract_interface_method) {
    mc::engine::abstract_interface* iface = obj.get_interface("org.test.ExtendedInterface");
    ASSERT_NE(iface, nullptr);

    // 调用各个接口的方法
    mc::engine::invoke_result base_result = iface->invoke("BaseMethod", {42});
    EXPECT_EQ(base_result, "base:42");

    mc::engine::invoke_result middle_result = iface->invoke("MiddleMethod", {"21"});
    EXPECT_EQ(middle_result, 42.0);

    mc::engine::invoke_result extended_result = iface->invoke("ExtendedMethod", {10.0});
    EXPECT_EQ(extended_result, true);

    // 调用被覆盖的方法 - 应该调用扩展接口的版本
    mc::engine::invoke_result common_result = iface->invoke("CommonMethod", {"test"});
    EXPECT_EQ(common_result, "extended:test");
}

// 测试通过 abstract_object 调用方法
TEST_F(interface_inherit_test, test_abstract_object_method) {
    mc::engine::abstract_object* base_obj = &obj;

    // 不指定接口名 - 应该调用对象的方法
    mc::engine::invoke_result obj_result = base_obj->invoke("CommonMethod", {"test"});
    EXPECT_EQ(obj_result, "object:test");

    // 指定接口名 - 仍然被对象接口覆盖
    mc::engine::invoke_result iface_result =
        base_obj->invoke("CommonMethod", {"test"}, "org.test.ExtendedInterface");
    EXPECT_EQ(iface_result, "object:test");

    // 调用只在接口中存在的方法
    mc::engine::invoke_result base_result = base_obj->invoke("BaseMethod", {42});
    EXPECT_EQ(base_result, "base:42");
}

// 测试信号的覆盖关系
TEST_F(interface_inherit_test, test_signal_override) {
    // 测试不同级别的同名信号
    bool base_signal_triggered     = false;
    bool middle_signal_triggered   = false;
    bool extended_signal_triggered = false;

    // 连接基础接口的公共信号 - 这里应该连接到最终覆盖版本(扩展接口)
    auto con1 = obj.m_iface.m_common_signal.connect([&](const std::string& s) {
        extended_signal_triggered = true;
        EXPECT_EQ(s, "common");
    });

    // 触发公共信号
    obj.m_iface.m_common_signal("common");

    // 验证只有扩展接口的信号被触发
    EXPECT_FALSE(base_signal_triggered);
    EXPECT_FALSE(middle_signal_triggered);
    EXPECT_TRUE(extended_signal_triggered);

    con1.disconnect();
}

// 测试通过 abstract_object 连接和触发覆盖的信号
TEST_F(interface_inherit_test, test_abstract_signal_override) {
    mc::engine::abstract_object* base_obj = &obj;

    {
        bool signal_triggered = false;

        // 连接公共信号 - 不指定接口名，应该连接到对象级别的信号处理
        auto conn = base_obj->connect("CommonSignal", [&](const mc::variants& args) -> mc::variant {
            signal_triggered = true;
            EXPECT_EQ(args.size(), 1);
            EXPECT_EQ(args[0], "test_common");
            return {};
        });

        // 触发信号
        base_obj->emit("CommonSignal", {"test_common"});

        EXPECT_TRUE(signal_triggered);
        conn.disconnect();
    }

    {
        bool signal_triggered = false;

        // 连接到指定接口的信号
        auto conn2 = base_obj->connect(
            "CommonSignal",
            [&](const mc::variants& args) -> mc::variant {
            signal_triggered = true;
            EXPECT_EQ(args.size(), 1);
            EXPECT_EQ(args[0], "interface_common");
            return {};
        },
            "org.test.ExtendedInterface");

        // 触发指定接口的信号
        base_obj->emit("CommonSignal", {"interface_common"}, "org.test.ExtendedInterface");

        EXPECT_TRUE(signal_triggered);
    }
}