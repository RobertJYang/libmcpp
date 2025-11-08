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
#include <mc/expr/context.h>
#include <mc/expr/engine.h>
#include <mc/expr/function.h>

namespace {
class expr_context_test : public ::testing::Test {
protected:
    expr_context_test() {
    }

    void SetUp() override {
    }

    void TearDown() override {
    }

    mc::expr::engine engine;
};
} // namespace

// 测试Context的作用域功能
TEST_F(expr_context_test, ScopeInheritance) {
    mc::expr::context base_ctx;
    base_ctx.register_variable("base_var", 100);
    auto base_func = mc::expr::make_simple_function("base_func", [](double x) {
        return x * 2;
    });
    base_ctx.register_function(base_func);

    // 创建一个子上下文，继承自基础上下文
    mc::expr::context child_ctx(&base_ctx);
    child_ctx.register_variable("child_var", 200);
    auto child_func = mc::expr::make_simple_function("child_func", [](double x) {
        return x + 10;
    });
    child_ctx.register_function(child_func);

    // 测试子上下文可以访问基础上下文中的变量和函数
    EXPECT_TRUE(child_ctx.has_variable("base_var"));
    EXPECT_TRUE(child_ctx.has_variable("child_var"));
    EXPECT_EQ(child_ctx.get_variable("base_var"), 100);
    EXPECT_EQ(child_ctx.get_variable("child_var"), 200);

    EXPECT_TRUE(child_ctx.has_function("base_func"));
    EXPECT_TRUE(child_ctx.has_function("child_func"));

    // 测试基础上下文不能访问子上下文中的变量和函数
    EXPECT_TRUE(base_ctx.has_variable("base_var"));
    EXPECT_FALSE(base_ctx.has_variable("child_var"));
    EXPECT_EQ(base_ctx.get_variable("base_var"), 100);
    EXPECT_TRUE(base_ctx.get_variable("child_var").is_null());

    EXPECT_TRUE(base_ctx.has_function("base_func"));
    EXPECT_FALSE(base_ctx.has_function("child_func"));
}

// 测试多级作用域
TEST_F(expr_context_test, MultiLevelScope) {
    // 创建三级作用域
    mc::expr::context level1;
    mc::expr::context level2(&level1);
    mc::expr::context level3(&level2);

    // 设置各级变量
    level1.register_variable("var1", 1);
    level2.register_variable("var2", 2);
    level3.register_variable("var3", 3);

    // 测试变量查找
    EXPECT_EQ(level3.get_variable("var1"), 1);
    EXPECT_EQ(level3.get_variable("var2"), 2);
    EXPECT_EQ(level3.get_variable("var3"), 3);

    EXPECT_EQ(level2.get_variable("var1"), 1);
    EXPECT_EQ(level2.get_variable("var2"), 2);
    EXPECT_TRUE(level2.get_variable("var3").is_null());

    EXPECT_EQ(level1.get_variable("var1"), 1);
    EXPECT_TRUE(level1.get_variable("var2").is_null());
    EXPECT_TRUE(level1.get_variable("var3").is_null());
}

// 测试变量遮蔽
TEST_F(expr_context_test, VariableShadowing) {
    // 创建两级作用域
    mc::expr::context parent;
    mc::expr::context child(&parent);

    // 在父级设置变量
    parent.register_variable("shared", "parent_value");

    // 在子级设置同名变量
    child.register_variable("shared", "child_value");

    // 验证遮蔽效果
    EXPECT_EQ(parent.get_variable("shared"), "parent_value");
    EXPECT_EQ(child.get_variable("shared"), "child_value");
}

// 测试表达式引擎中使用作用域
TEST_F(expr_context_test, EngineWithScope) {
    auto global_ctx = engine.get_global_context();
    auto local_ctx  = engine.make_context(&global_ctx);

    // 设置变量
    global_ctx.register_variable("x", 10);
    local_ctx.register_variable("y", 20);

    // 验证表达式计算
    EXPECT_EQ(engine.evaluate("x + y", local_ctx), 30);
    EXPECT_EQ(engine.evaluate("x * y", local_ctx), 200);

    // 验证未定义变量的访问
    EXPECT_THROW(engine.evaluate("z", local_ctx), mc::invalid_arg_exception);

    // 测试使用create_context创建的上下文
    auto ctx1 = engine.make_context();
    ctx1.register_variable("a", 100);

    // 创建基于ctx1的子上下文
    auto ctx2 = engine.make_context(&ctx1);
    ctx2.register_variable("b", 200);

    // 在子上下文中可以访问父上下文的变量
    EXPECT_EQ(engine.evaluate("a + b", ctx2), 300);
}

// 测试使用dict创建上下文
TEST_F(expr_context_test, CreateContextWithDict) {
    mc::dict variables;
    variables.insert("x", 10);
    variables.insert("y", 20);

    auto ctx = engine.make_context(variables);
    EXPECT_EQ(engine.evaluate("x + y", ctx), 30);
    EXPECT_EQ(engine.evaluate("abs(-5)", ctx), 5);

    // 创建子上下文
    mc::expr::context child_ctx(ctx);
    child_ctx.register_variable("z", 30);

    // 验证子上下文可以访问父上下文的变量和函数
    EXPECT_EQ(engine.evaluate("x + y + z", child_ctx), 60);
    EXPECT_EQ(engine.evaluate("max(x, y, z)", child_ctx), 30);
}

// 测试设置父级上下文
TEST_F(expr_context_test, SetParent) {
    // 创建两个上下文
    mc::expr::context ctx1;
    mc::expr::context ctx2;

    ctx1.register_variable("var1", 100);
    ctx2.register_variable("var2", 200);

    // 初始状态下互相不可见
    EXPECT_FALSE(ctx1.has_variable("var2"));
    EXPECT_FALSE(ctx2.has_variable("var1"));

    // 设置ctx2的父级为ctx1
    ctx2.set_parent(&ctx1);

    // 设置父级后，ctx2可以访问ctx1的变量
    EXPECT_TRUE(ctx2.has_variable("var1"));
    EXPECT_EQ(ctx2.get_variable("var1"), 100);

    // ctx1仍然不能访问ctx2的变量
    EXPECT_FALSE(ctx1.has_variable("var2"));
}

// 注意：object_context 的测试已经在 test_object_expr.cpp 中完成
// 这里不再重复测试，因为需要实现完整的 abstract_object 接口

// 测试上下文中的函数注册和调用
TEST_F(expr_context_test, function_registration) {
    mc::expr::context ctx;

    // 注册函数
    auto func1 = mc::expr::make_simple_function("add", [](double a, double b) {
        return a + b;
    });
    auto id1 = ctx.register_function(func1);
    EXPECT_GE(id1, 0);

    // 注册同名函数（应该覆盖）
    auto func2 = mc::expr::make_simple_function("add", [](double a, double b, double c) {
        return a + b + c;
    });
    auto id2 = ctx.register_function(func2);
    EXPECT_GE(id2, 0);

    // 测试函数调用
    EXPECT_TRUE(ctx.has_function("add"));
    auto result = ctx.invoke("add", {1.0, 2.0, 3.0}, "");
    EXPECT_EQ(result, 6.0);
}

// 测试变量注册和获取
TEST_F(expr_context_test, variable_registration) {
    mc::expr::context ctx;

    // 注册变量
    auto id1 = ctx.register_variable("x", 10);
    EXPECT_GE(id1, 0);

    auto id2 = ctx.register_variable("y", "hello");
    EXPECT_GE(id2, 0);

    // 测试变量获取
    EXPECT_TRUE(ctx.has_variable("x"));
    EXPECT_EQ(ctx.get_variable("x"), 10);
    EXPECT_TRUE(ctx.has_variable("y"));
    EXPECT_EQ(ctx.get_variable("y"), "hello");

    // 测试未定义的变量
    EXPECT_FALSE(ctx.has_variable("z"));
    EXPECT_TRUE(ctx.get_variable("z").is_null());
}

// 测试从 dict 导入变量
TEST_F(expr_context_test, import_from_dict) {
    mc::expr::context ctx;

    mc::dict data;
    data["a"] = 1;
    data["b"] = 2.5;
    data["c"] = "test";

    ctx.import_from_dict(data);

    EXPECT_TRUE(ctx.has_variable("a"));
    EXPECT_EQ(ctx.get_variable("a"), 1);
    EXPECT_TRUE(ctx.has_variable("b"));
    EXPECT_EQ(ctx.get_variable("b"), 2.5);
    EXPECT_TRUE(ctx.has_variable("c"));
    EXPECT_EQ(ctx.get_variable("c"), "test");
}

// 测试上下文复制和移动
TEST_F(expr_context_test, context_copy_and_move) {
    mc::expr::context ctx1;
    ctx1.register_variable("x", 10);

    // 测试复制构造
    mc::expr::context ctx2(ctx1);
    EXPECT_TRUE(ctx2.has_variable("x"));
    EXPECT_EQ(ctx2.get_variable("x"), 10);

    // 测试移动构造
    mc::expr::context ctx3(std::move(ctx2));
    EXPECT_TRUE(ctx3.has_variable("x"));
    EXPECT_EQ(ctx3.get_variable("x"), 10);

    // 测试复制赋值
    mc::expr::context ctx4;
    ctx4 = ctx3;
    EXPECT_TRUE(ctx4.has_variable("x"));
    EXPECT_EQ(ctx4.get_variable("x"), 10);

    // 测试移动赋值
    mc::expr::context ctx5;
    ctx5 = std::move(ctx4);
    EXPECT_TRUE(ctx5.has_variable("x"));
    EXPECT_EQ(ctx5.get_variable("x"), 10);
}

// 测试上下文基类的默认行为
TEST_F(expr_context_test, context_base_default_behavior) {
    mc::expr::context_base base_ctx(nullptr);

    // 测试默认行为
    EXPECT_FALSE(base_ctx.has_variable("x"));
    EXPECT_FALSE(base_ctx.has_function("func"));
    EXPECT_TRUE(base_ctx.get_variable("x").is_null());

    // 测试调用不存在的函数
    EXPECT_THROW(base_ctx.invoke("func", {}, ""), mc::invalid_arg_exception);
    EXPECT_THROW(base_ctx.invoke("func", {}, "iface"), mc::invalid_arg_exception);
}

// 注意：object_context 的接口参数测试已经在 test_object_expr.cpp 中完成
// 这里不再重复测试，因为需要实现完整的 abstract_object 接口