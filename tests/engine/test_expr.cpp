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
#include <mc/engine/engine.h>
#include <mc/engine/expr.h>
#include <mc/engine/object.h>
#include <mc/engine/service.h>
#include <mc/expr.h>
#include <mc/log.h>

namespace {

// 创建一个简单的服务用于测试
class test_service : public mc::engine::service {
public:
    test_service() : mc::engine::service("test_service") {
        // 注册服务方法
        register_method("add", [this](const mc::dict& params) -> mc::variant {
            int a = params.get("arg0", 0).as_int32();
            int b = params.get("arg1", 0).as_int32();
            return a + b;
        });

        register_method("multiply", [this](const mc::dict& params) -> mc::variant {
            int a = params.get("arg0", 0).as_int32();
            int b = params.get("arg1", 0).as_int32();
            return a * b;
        });

        register_method("greet", [this](const mc::dict& params) -> mc::variant {
            std::string name = params.get("arg0", "").get_string();
            return "Hello, " + name + "!";
        });
    }
};

// 测试夹具
class EngineExprTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 启动引擎
        m_engine.start(1);

        // 注册测试服务
        m_service = std::make_shared<test_service>();
        m_engine.register_service(m_service);

        // 创建表达式引擎
        m_expr_engine = std::make_shared<mc::expr::engine>();
    }

    void TearDown() override {
        m_engine.stop();
    }

    mc::engine::engine&               m_engine = mc::engine::get_engine();
    std::shared_ptr<test_service>     m_service;
    std::shared_ptr<mc::expr::engine> m_expr_engine;
};

} // namespace

// 测试注册单个方法
TEST_F(EngineExprTest, RegisterMethod) {
    // 注册引擎方法
    mc::engine::register_engine_method(*m_expr_engine, "test_service", "add");

    // 创建上下文
    auto ctx = m_expr_engine->create_context();

    // 测试方法调用
    mc::variant result = m_expr_engine->evaluate("add(5, 3)", ctx);
    EXPECT_EQ(result.as_int32(), 8);
}

// 测试注册整个服务
TEST_F(EngineExprTest, RegisterService) {
    // 注册整个服务
    mc::engine::register_engine_service(*m_expr_engine, "test_service");

    // 创建上下文
    auto ctx = m_expr_engine->create_context();

    // 测试各个方法
    EXPECT_EQ(m_expr_engine->evaluate("test_service_add(10, 20)", ctx).as_int32(), 30);
    EXPECT_EQ(m_expr_engine->evaluate("test_service_multiply(5, 6)", ctx).as_int32(), 30);
    EXPECT_EQ(m_expr_engine->evaluate("test_service_greet('Alice')", ctx).get_string(),
              "Hello, Alice!");
}

// 测试使用自定义前缀注册服务
TEST_F(EngineExprTest, RegisterServiceWithPrefix) {
    // 使用自定义前缀注册服务
    mc::engine::register_engine_service(*m_expr_engine, "test_service", "ts_");

    // 创建上下文
    auto ctx = m_expr_engine->create_context();

    // 测试带前缀的方法
    EXPECT_EQ(m_expr_engine->evaluate("ts_add(15, 25)", ctx).as_int32(), 40);
    EXPECT_EQ(m_expr_engine->evaluate("ts_multiply(7, 8)", ctx).as_int32(), 56);
}

// 测试对象上下文适配器
TEST_F(EngineExprTest, ObjectContextAdapter) {
    // 创建测试对象
    auto object = std::make_shared<mc::engine::object>("test_object");
    object->set_property("name", "Bob");
    object->set_property("age", 25);
    object->set_property("is_active", true);

    // 注册对象到引擎
    auto& object_table = m_engine.get_object_table();
    object_table.add(object);

    // 创建对象上下文适配器
    mc::engine::object_context_adapter adapter("test_object");

    // 测试表达式计算
    EXPECT_EQ(adapter.evaluate("'My name is ' + name").get_string(), "My name is Bob");
    EXPECT_EQ(adapter.evaluate("age + 10").as_int32(), 35);
    EXPECT_TRUE(adapter.evaluate("is_active").as_bool());
    EXPECT_EQ(adapter.evaluate("age * 2").as_int32(), 50);
}

// 测试全局表达式引擎
TEST_F(EngineExprTest, GlobalExprEngine) {
    // 获取全局表达式引擎
    auto& global_engine = mc::engine::get_expr_engine();

    // 注册服务
    mc::engine::register_engine_service(global_engine, "test_service", "g_");

    // 创建上下文
    auto ctx = global_engine.create_context();

    // 测试方法调用
    EXPECT_EQ(global_engine.evaluate("g_add(100, 200)", ctx).as_int32(), 300);
    EXPECT_EQ(global_engine.evaluate("g_multiply(10, 10)", ctx).as_int32(), 100);
}

// 测试复杂表达式
TEST_F(EngineExprTest, ComplexExpression) {
    // 注册服务方法
    mc::engine::register_engine_method(*m_expr_engine, "test_service", "add");
    mc::engine::register_engine_method(*m_expr_engine, "test_service", "multiply");

    // 创建上下文
    auto ctx = m_expr_engine->create_context();
    ctx.set_variable("x", 5);
    ctx.set_variable("y", 3);

    // 测试复杂表达式
    EXPECT_EQ(m_expr_engine->evaluate("add(x, y) * multiply(x, y)", ctx).as_int32(), 8 * 15);
    EXPECT_EQ(m_expr_engine->evaluate("add(multiply(2, x), multiply(3, y))", ctx).as_int32(),
              (2 * 5) + (3 * 3));
}

// 测试错误处理
TEST_F(EngineExprTest, ErrorHandling) {
    // 注册服务方法
    mc::engine::register_engine_method(*m_expr_engine, "test_service", "add");

    // 测试非法参数
    auto ctx = m_expr_engine->create_context();
    EXPECT_THROW(m_expr_engine->evaluate("add('abc', 123)", ctx), mc::expr::type_error);

    // 测试不存在的方法
    EXPECT_THROW(m_expr_engine->evaluate("test_service_not_exist()", ctx), mc::expr::eval_error);

    // 测试不存在的服务
    mc::engine::register_engine_method(*m_expr_engine, "nonexistent_service", "method");
    EXPECT_THROW(m_expr_engine->evaluate("method()", ctx), mc::expr::eval_error);
}