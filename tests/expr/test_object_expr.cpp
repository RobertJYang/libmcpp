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
#include <mc/expr.h>

namespace test_object_expr {

// 测试接口1：包含整数属性和加减操作
class TestInterface1 : public mc::engine::interface<TestInterface1> {
public:
    MC_INTERFACE("org.test.obj.TestInterface1")

    int32_t m_value = 0;

    int32_t add(int32_t a)
    {
        auto old_value = m_value;
        m_value += a;
        value_changed(old_value, m_value);
        return m_value;
    }

    int32_t subtract(int32_t a)
    {
        auto old_value = m_value;
        m_value -= a;
        value_changed(old_value, m_value);
        return m_value;
    }

    mc::signal<void(int32_t, int32_t)> value_changed;
};

// 测试接口2：包含字符串属性和设置/获取操作
class TestInterface2 : public mc::engine::interface<TestInterface2> {
public:
    MC_INTERFACE("org.test.expr.TestInterface2")

    std::string m_value = "默认值";

    void set_value(std::string_view value)
    {
        auto old_value = m_value;
        m_value        = value;
        value_changed(old_value, m_value);
    }

    std::string get_value() const
    {
        return m_value;
    }

    bool is_empty() const
    {
        return m_value.empty();
    }

    mc::signal<void(std::string_view, std::string_view)> value_changed;
};

// 测试对象：实现两个接口
class TestObject : public mc::engine::object<TestObject> {
public:
    MC_OBJECT(TestObject, "TestObject", "/org/test/TestObject", (TestInterface1)(TestInterface2))

    TestObject(mc::engine::core_object* parent = nullptr)
        : mc::engine::object<TestObject>(parent)
    {
    }

    TestInterface1 m_iface1;
    TestInterface2 m_iface2;
};

} // namespace test_object_expr

// 反射声明
MC_REFLECT(test_object_expr::TestInterface1,
           ((m_value, "value"))((add, "Add"))((subtract, "Subtract"))((value_changed,
                                                                       "value_changed")))
MC_REFLECT(test_object_expr::TestInterface2,
           ((m_value, "value"))((set_value, "SetValue"))((get_value, "GetValue"))(
               (is_empty, "IsEmpty"))((value_changed, "value_changed")))
MC_REFLECT(test_object_expr::TestObject,
           ((m_iface1, "iface1"))((m_iface2, "iface2")))

using namespace test_object_expr;

class object_expr_test : public ::testing::Test {
protected:
    void SetUp() override
    {
        // 设置初始值
        obj                   = TestObject::create();
        obj->m_iface1.m_value = 10;
        obj->m_iface2.set_value("初始值");

        // 创建对象上下文
        obj_ctx = std::make_unique<mc::expr::object_context>(obj.get());

        // 监听信号
        value_change_count  = 0;
        string_change_count = 0;

        value_conn = obj->m_iface1.value_changed.connect([this](int32_t old_val, int32_t new_val) {
            value_change_count++;
            last_old_value = old_val;
            last_new_value = new_val;
        });

        string_conn = obj->m_iface2.value_changed.connect(
            [this](std::string_view old_val, std::string_view new_val) {
            string_change_count++;
            last_old_string = old_val;
            last_new_string = new_val;
        });
    }

    void TearDown() override
    {
        value_conn.disconnect();
        string_conn.disconnect();
        obj_ctx.reset();
        obj.reset();
    }

    mc::shared_ptr<TestObject>                obj;
    std::unique_ptr<mc::expr::object_context> obj_ctx;
    mc::expr::engine                          expr_engine;

    // 信号测试数据
    int         value_change_count  = 0;
    int         string_change_count = 0;
    int32_t     last_old_value      = 0;
    int32_t     last_new_value      = 0;
    std::string last_old_string;
    std::string last_new_string;

    mc::connection_type value_conn;
    mc::connection_type string_conn;
};

// 测试通过表达式读取对象属性
TEST_F(object_expr_test, test_read_properties)
{
    // 读取数值属性（指定interface）
    auto iface1_value = expr_engine.evaluate("iface1.value", *obj_ctx);
    EXPECT_TRUE(iface1_value.is_int32());
    EXPECT_EQ(iface1_value.as_int32(), 10);

    // 读取数值属性（不指定interface，返回的是第一个interface的属性）
    auto value = expr_engine.evaluate("value", *obj_ctx);
    EXPECT_TRUE(value.is_int32());
    EXPECT_EQ(value.as_int32(), 10);

    // 读取字符串属性（同名属性，必须增加限定符才能访问）
    auto iface2_value = expr_engine.evaluate("iface2.value", *obj_ctx);
    EXPECT_TRUE(iface2_value.is_string());
    EXPECT_EQ(iface2_value.as_string(), "初始值");
}

// 测试通过表达式调用对象方法
TEST_F(object_expr_test, test_call_methods)
{
    // 调用Add方法（指定interface）
    auto add_result = expr_engine.evaluate("iface1.Add(5)", *obj_ctx);
    EXPECT_TRUE(add_result.is_int32());
    EXPECT_EQ(add_result.as_int32(), 15);

    EXPECT_EQ(value_change_count, 1);
    EXPECT_EQ(last_old_value, 10);
    EXPECT_EQ(last_new_value, 15);
    EXPECT_EQ(obj->m_iface1.m_value, 15);

    // 调用Add方法（不指定interface，调用的是第一个实现了该方法的interface的方法）
    auto add_result_1 = expr_engine.evaluate("Add(5)", *obj_ctx);
    EXPECT_TRUE(add_result_1.is_int32());
    EXPECT_EQ(add_result_1.as_int32(), 20);

    EXPECT_EQ(value_change_count, 2);
    EXPECT_EQ(last_old_value, 15);
    EXPECT_EQ(last_new_value, 20);
    EXPECT_EQ(obj->m_iface1.m_value, 20);

    // 调用Subtract方法
    auto sub_result = expr_engine.evaluate("iface1.Subtract(3)", *obj_ctx);
    EXPECT_TRUE(sub_result.is_int32());
    EXPECT_EQ(sub_result.as_int32(), 17);

    // 验证信号被触发
    EXPECT_EQ(value_change_count, 3);
    EXPECT_EQ(last_old_value, 20);
    EXPECT_EQ(last_new_value, 17);
}

// 测试通过表达式修改对象属性
TEST_F(object_expr_test, test_set_properties)
{
    // 调用SetValue方法
    expr_engine.evaluate("iface2.SetValue('通过表达式设置的值')", *obj_ctx);

    // 验证对象属性被修改
    EXPECT_EQ(obj->m_iface2.m_value, "通过表达式设置的值");

    // 验证信号被触发
    EXPECT_EQ(string_change_count, 1);
    EXPECT_EQ(last_old_string, "初始值");
    EXPECT_EQ(last_new_string, "通过表达式设置的值");

    // 通过GetValue方法获取值
    auto get_result = expr_engine.evaluate("iface2.GetValue()", *obj_ctx);
    EXPECT_TRUE(get_result.is_string());
    EXPECT_EQ(get_result.as_string(), "通过表达式设置的值");

    // 调用无参布尔返回方法
    auto empty_result = expr_engine.evaluate("iface2.IsEmpty()", *obj_ctx);
    EXPECT_TRUE(empty_result.is_bool());
    EXPECT_FALSE(empty_result.as_bool());
}

// 测试复杂表达式
TEST_F(object_expr_test, test_complex_expressions)
{
    // 条件表达式
    auto complex_expr = "value > 5 ? Add(10) : Subtract(5)";
    auto result       = expr_engine.evaluate(complex_expr, *obj_ctx);
    EXPECT_TRUE(result.is_int32());
    EXPECT_EQ(result.as_int32(), 20); // 10 + 10 = 20

    // 字符串拼接
    auto concat_expr = "'当前值: ' + iface2.GetValue()";
    auto str_result  = expr_engine.evaluate(concat_expr, *obj_ctx);
    EXPECT_TRUE(str_result.is_string());
    EXPECT_EQ(str_result.as_string(), "当前值: 初始值");

    // 组合多个接口的属性
    auto combined_expr   = "iface1.value > 15 && !iface2.IsEmpty()";
    auto combined_result = expr_engine.evaluate(combined_expr, *obj_ctx);
    EXPECT_TRUE(combined_result.is_bool());
    EXPECT_TRUE(combined_result.as_bool());
}

// 测试错误处理
TEST_F(object_expr_test, test_error_handling)
{
    // 访问不存在的属性
    EXPECT_THROW(expr_engine.evaluate("iface1.non_existent", *obj_ctx), mc::invalid_arg_exception);

    // 调用不存在的方法
    EXPECT_THROW(expr_engine.evaluate("iface1.NonExistentMethod()", *obj_ctx),
                 mc::invalid_arg_exception);

    // 参数错误
    EXPECT_THROW(expr_engine.evaluate("iface1.Add('not a number')", *obj_ctx),
                 mc::invalid_arg_exception);

    // 实参个数比形参个数多，允许
    EXPECT_NO_THROW(expr_engine.evaluate("iface1.Add(1, 2)", *obj_ctx));
}

// 测试 object_method_call_node::get_type() 方法
TEST_F(object_expr_test, ObjectMethodCallNodeGetType)
{
    mc::expr::engine expr_engine;
    auto             obj_ctx = expr_engine.make_context(obj.get());

    // 解析一个对象方法调用表达式
    auto node = expr_engine.compile("iface1.Add(1)");
    EXPECT_EQ(node->get_type(), mc::expr::node_type::object_method_call);
}