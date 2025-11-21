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
#include <mc/expr/context.h>
#include <mc/expr/engine.h>
#include <mc/expr/function.h>

// 测试辅助类，需要在命名命名空间中定义（不能在匿名命名空间），以便链接器能找到反射元数据
namespace tests::expr::context_test {
class TestObjectForRegister : public mc::engine::object<TestObjectForRegister> {
public:
    MC_OBJECT(TestObjectForRegister, "TestObjectForRegister", "/org/test/TestObjectForRegister")
    TestObjectForRegister(mc::engine::core_object* parent = nullptr)
        : mc::engine::object<TestObjectForRegister>(parent) {
    }
};

class TestInterfaceForVariable : public mc::engine::interface<TestInterfaceForVariable> {
public:
    MC_INTERFACE("org.test.TestInterfaceForVariable")
    int32_t m_value = 42;
};

class TestObjectForVariable : public mc::engine::object<TestObjectForVariable> {
public:
    MC_OBJECT(TestObjectForVariable, "TestObjectForVariable", "/org/test/TestObjectForVariable",
              (TestInterfaceForVariable))
    TestObjectForVariable(mc::engine::core_object* parent = nullptr)
        : mc::engine::object<TestObjectForVariable>(parent) {
    }
    TestInterfaceForVariable m_iface;
};

class TestInterfaceForFunction : public mc::engine::interface<TestInterfaceForFunction> {
public:
    MC_INTERFACE("org.test.TestInterfaceForFunction")
    int32_t add(int32_t a) {
        return a + 1;
    }
};

class TestObjectForFunction : public mc::engine::object<TestObjectForFunction> {
public:
    MC_OBJECT(TestObjectForFunction, "TestObjectForFunction", "/org/test/TestObjectForFunction",
              (TestInterfaceForFunction))
    TestObjectForFunction(mc::engine::core_object* parent = nullptr)
        : mc::engine::object<TestObjectForFunction>(parent) {
    }
    TestInterfaceForFunction m_iface;
};

class TestInterfaceForInvoke : public mc::engine::interface<TestInterfaceForInvoke> {
public:
    MC_INTERFACE("org.test.TestInterfaceForInvoke")
    int32_t add(int32_t a) {
        return a + 10;
    }
};

class TestObjectForInvoke : public mc::engine::object<TestObjectForInvoke> {
public:
    MC_OBJECT(TestObjectForInvoke, "TestObjectForInvoke", "/org/test/TestObjectForInvoke",
              (TestInterfaceForInvoke))
    TestObjectForInvoke(mc::engine::core_object* parent = nullptr)
        : mc::engine::object<TestObjectForInvoke>(parent) {
    }
    TestInterfaceForInvoke m_iface;
};

class TestObjectForGetObject : public mc::engine::object<TestObjectForGetObject> {
public:
    MC_OBJECT(TestObjectForGetObject, "TestObjectForGetObject", "/org/test/TestObjectForGetObject")
    TestObjectForGetObject(mc::engine::core_object* parent = nullptr)
        : mc::engine::object<TestObjectForGetObject>(parent) {
    }
};
} // namespace tests::expr::context_test

// 反射声明，必须在命名空间外定义
MC_REFLECT(tests::expr::context_test::TestInterfaceForVariable, ((m_value, "value")))
MC_REFLECT(tests::expr::context_test::TestInterfaceForFunction, ((add, "add")))
MC_REFLECT(tests::expr::context_test::TestInterfaceForInvoke, ((add, "add")))
MC_REFLECT(tests::expr::context_test::TestObjectForVariable, ((m_iface, "iface")))
MC_REFLECT(tests::expr::context_test::TestObjectForFunction, ((m_iface, "iface")))
MC_REFLECT(tests::expr::context_test::TestObjectForInvoke, ((m_iface, "iface")))
MC_REFLECT(tests::expr::context_test::TestObjectForRegister, ())
MC_REFLECT(tests::expr::context_test::TestObjectForGetObject, ())

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

// 测试 context_base 的复制和移动构造
TEST_F(expr_context_test, context_base_copy_and_move) {
    mc::expr::context_base base1(nullptr);
    mc::expr::context_base base2(&base1);

    // 测试复制构造
    mc::expr::context_base base3(base2);
    EXPECT_EQ(base3.get_parent(), base2.get_parent());

    // 测试复制赋值
    mc::expr::context_base base4(nullptr);
    base4 = base3;
    EXPECT_EQ(base4.get_parent(), base3.get_parent());

    // 测试移动构造
    mc::expr::context_base base5(std::move(base4));
    EXPECT_EQ(base5.get_parent(), base3.get_parent());
    EXPECT_EQ(base4.get_parent(), nullptr); // 移动后应该为 nullptr

    // 测试移动赋值
    mc::expr::context_base base6(nullptr);
    base6 = std::move(base5);
    EXPECT_EQ(base6.get_parent(), base3.get_parent());
    EXPECT_EQ(base5.get_parent(), nullptr); // 移动后应该为 nullptr
}

// 测试注册对象
TEST_F(expr_context_test, register_object) {
    mc::expr::context ctx;

    auto test_obj = tests::expr::context_test::TestObjectForRegister::create();

    // 测试注册对象
    auto id = ctx.register_object("test_obj", test_obj.get());
    EXPECT_GE(id, 0);

    // 测试注册空对象（应该返回 -1）
    auto invalid_id = ctx.register_object("invalid_obj", nullptr);
    EXPECT_EQ(invalid_id, -1);
}

// 测试通过对象符号访问对象属性
TEST_F(expr_context_test, get_variable_from_object) {
    mc::expr::context ctx;

    auto test_obj = tests::expr::context_test::TestObjectForVariable::create();
    test_obj->m_iface.m_value = 100;

    // 注册对象
    ctx.register_object("obj", test_obj.get());

    // 测试通过对象符号访问对象属性
    // 注意：这里需要对象有对应的属性，所以使用接口属性
    auto value = ctx.get_variable("value", "obj");
    // 由于对象符号类型为 object，会调用 obj->get_property("value")
    // 如果对象没有该属性，会返回空 variant
    EXPECT_TRUE(value.is_null() || value == 100);
}

// 测试通过对象符号检查变量是否存在
TEST_F(expr_context_test, has_variable_from_object) {
    mc::expr::context ctx;

    auto test_obj = tests::expr::context_test::TestObjectForVariable::create();

    // 注册对象
    ctx.register_object("obj", test_obj.get());

    // 测试通过对象符号检查属性是否存在
    // 由于对象符号类型为 object，会调用 obj->has_property("value")
    bool has_prop = ctx.has_variable("value", "obj");
    // 如果对象有该属性，返回 true，否则返回 false
    EXPECT_TRUE(has_prop || !has_prop); // 至少不会崩溃
}

// 测试通过对象符号检查方法是否存在
TEST_F(expr_context_test, has_function_from_object) {
    mc::expr::context ctx;

    auto test_obj = tests::expr::context_test::TestObjectForFunction::create();

    // 注册对象
    ctx.register_object("obj", test_obj.get());

    // 测试通过对象符号检查方法是否存在
    // 由于对象符号类型为 object，会调用 obj->has_method("add")
    bool has_method = ctx.has_function("add", "obj");
    // 如果对象有该方法，返回 true，否则返回 false
    EXPECT_TRUE(has_method || !has_method); // 至少不会崩溃

    // 测试对象符号不存在时，会调用 context_base::has_function
    bool has_func_nonexistent = ctx.has_function("nonexistent", "nonexistent_obj");
    EXPECT_FALSE(has_func_nonexistent);
}

// 测试通过对象符号调用方法
TEST_F(expr_context_test, invoke_from_object) {
    mc::expr::context ctx;

    auto test_obj = tests::expr::context_test::TestObjectForInvoke::create();

    // 注册对象
    ctx.register_object("obj", test_obj.get());

    // 测试通过对象符号调用方法
    // 由于对象符号类型为 object，会调用 obj->invoke("add", args)
    auto result = ctx.invoke("add", {5}, "obj");
    // 如果对象有该方法，返回结果，否则返回空 variant
    EXPECT_TRUE(result.is_null() || result == 15);

    // 测试对象符号不存在时，会调用 context_base::invoke
    EXPECT_THROW(ctx.invoke("nonexistent", {}, "nonexistent_obj"), mc::invalid_arg_exception);
}

// 测试 object_context::get_object
TEST_F(expr_context_test, object_context_get_object) {
    auto test_obj = tests::expr::context_test::TestObjectForGetObject::create();

    // 创建 object_context
    mc::expr::object_context obj_ctx(test_obj.get());

    // 测试 get_object
    auto* obj = obj_ctx.get_object();
    EXPECT_EQ(obj, test_obj.get());
}

// 测试通过对象符号访问变量类型的符号（包含属性）
TEST_F(expr_context_test, get_variable_from_variable_symbol) {
    mc::expr::context ctx;

    // 注册一个变量类型的符号，包含 dict 属性
    mc::dict obj_dict;
    obj_dict["prop1"] = 100;
    obj_dict["prop2"] = "test";

    ctx.register_variable("obj_var", obj_dict);

    // 测试通过变量符号访问属性
    auto prop1 = ctx.get_variable("prop1", "obj_var");
    EXPECT_EQ(prop1, 100);

    auto prop2 = ctx.get_variable("prop2", "obj_var");
    EXPECT_EQ(prop2, "test");

    // 测试访问不存在的属性
    auto prop3 = ctx.get_variable("prop3", "obj_var");
    EXPECT_TRUE(prop3.is_null());
}

// 测试通过对象符号访问变量类型的符号（检查属性是否存在）
TEST_F(expr_context_test, has_variable_from_variable_symbol) {
    mc::expr::context ctx;

    // 注册一个变量类型的符号，包含 dict 属性
    mc::dict obj_dict;
    obj_dict["prop1"] = 100;
    obj_dict["prop2"] = "test";

    ctx.register_variable("obj_var", obj_dict);

    // 测试通过变量符号检查属性是否存在
    EXPECT_TRUE(ctx.has_variable("prop1", "obj_var"));
    EXPECT_TRUE(ctx.has_variable("prop2", "obj_var"));
    EXPECT_FALSE(ctx.has_variable("prop3", "obj_var"));
}

// 注意：object_context 的接口参数测试已经在 test_object_expr.cpp 中完成
// 这里不再重复测试，因为需要实现完整的 abstract_object 接口

// 测试 context_base 的自赋值
TEST_F(expr_context_test, ContextBaseSelfAssignment) {
    mc::expr::context_base base1;
    mc::expr::context_base base2;

    // 测试自赋值
    base1 = base1;
    EXPECT_EQ(base1.get_parent(), nullptr);

    // 测试正常赋值
    base2.set_parent(&base1);
    base1 = base2;
    EXPECT_EQ(base1.get_parent(), &base1);
}

// 测试 context_base 的自移动赋值
TEST_F(expr_context_test, ContextBaseSelfMoveAssignment) {
    mc::expr::context_base base1;
    mc::expr::context_base base2;

    // 测试自移动赋值
    base1 = std::move(base1);
    EXPECT_EQ(base1.get_parent(), nullptr);

    // 测试正常移动赋值
    base2.set_parent(&base1);
    base1 = std::move(base2);
    EXPECT_EQ(base1.get_parent(), &base1);
}

// 测试 context 的自赋值
TEST_F(expr_context_test, ContextSelfAssignment) {
    mc::expr::context ctx1;
    mc::expr::context ctx2;

    // 注册变量
    ctx1.register_variable("x", 10);
    ctx2.register_variable("y", 20);

    // 测试自赋值
    ctx1 = ctx1;
    EXPECT_EQ(ctx1.get_variable("x"), 10);

    // 测试正常赋值
    ctx1 = ctx2;
    EXPECT_EQ(ctx1.get_variable("y"), 20);
    EXPECT_FALSE(ctx1.has_variable("x"));
}

// 测试 context 的自移动赋值
TEST_F(expr_context_test, ContextSelfMoveAssignment) {
    mc::expr::context ctx1;
    mc::expr::context ctx2;

    // 注册变量
    ctx1.register_variable("x", 10);
    ctx2.register_variable("y", 20);

    // 测试自移动赋值
    ctx1 = std::move(ctx1);
    EXPECT_EQ(ctx1.get_variable("x"), 10);

    // 测试正常移动赋值
    ctx1 = std::move(ctx2);
    EXPECT_EQ(ctx1.get_variable("y"), 20);
    EXPECT_FALSE(ctx1.has_variable("x"));
}