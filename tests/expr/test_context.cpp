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
    auto local_ctx  = engine.create_context(&global_ctx);

    // 设置变量
    global_ctx.register_variable("x", 10);
    local_ctx.register_variable("y", 20);

    // 验证表达式计算
    EXPECT_EQ(engine.evaluate("x + y", local_ctx), 30);
    EXPECT_EQ(engine.evaluate("x * y", local_ctx), 200);

    // 验证未定义变量的访问
    EXPECT_THROW(engine.evaluate("z", local_ctx), mc::invalid_arg_exception);

    // 测试使用create_context创建的上下文
    auto ctx1 = engine.create_context();
    ctx1.register_variable("a", 100);

    // 创建基于ctx1的子上下文
    auto ctx2 = engine.create_context(&ctx1);
    ctx2.register_variable("b", 200);

    // 在子上下文中可以访问父上下文的变量
    EXPECT_EQ(engine.evaluate("a + b", ctx2), 300);
}

// 测试使用dict创建上下文
TEST_F(expr_context_test, CreateContextWithDict) {
    mc::mutable_dict variables;
    variables.insert("x", 10);
    variables.insert("y", 20);

    auto ctx = engine.create_context(variables);
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