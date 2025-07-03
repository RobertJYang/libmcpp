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
#include <mc/module.h>
#include <test_utilities/test_base.h>

// 定义一个测试模块
MC_MODULE(mc_test_module)

namespace mc::test_module {

class test_class {
public:
    test_class() = default;

    void set_value(int v) {
        m_value = v;
    }
    int get_value() const {
        return m_value;
    }

    int m_value{42};
};

} // namespace mc::test_module

// 实现测试模块
MC_BUILTIN_MODULE_IMPL(mc_test_module);

// 导出测试类
MC_MODULE_REFLECT(
    mc_test_module,
    (mc::test_module::test_class, "TestClass"),
    ((set_value, "setValue"))((get_value, "getValue")))

namespace {

/**
 * @brief 测试模块系统基本功能
 */
class builtin_module_test : public mc::test::TestBase {
protected:
    void SetUp() override {
        mc::log::default_logger().set_level(mc::log::level::error);
        mc::singleton<mc::module::module_manager>::reset_for_test();
        mc::singleton<mc::reflect::factory_ptr, mc::reflect::global_namespace>::reset_for_test();
        MC_REGISTER_BUILTIN_MODULE(mc_test_module);
    }

    void TearDown() override {
        MC_UNREGISTER_BUILTIN_MODULE(mc_test_module);
    }

    static void SetUpTestSuite() {
        // 销毁全局反射工厂，避免其他测试污染当前测试
        mc::singleton<mc::reflect::factory_ptr, mc::reflect::global_namespace>::reset_for_test();

        mc::test::TestBase::TearDownTestSuite();
    }

    static void TearDownTestSuite() {
        // 注销类型，避免污染其他测试
        mc::reflect::reflector<mc::test_module::test_class>::unregister_type();

        // 销毁 test_module 的反射工厂，避免污染其他测试
        using test_module_ns = mc_test_module_namespace;
        mc::singleton<mc::reflect::factory_ptr, test_module_ns>::reset_for_test();

        // 销毁全局反射工厂，避免污染其他测试
        mc::singleton<mc::reflect::reflection_factory>::reset_for_test();

        // 销毁模块管理器，避免污染其他测试
        mc::singleton<mc::reflect::factory_ptr, mc::reflect::global_namespace>::reset_for_test();

        // 调用基类清理方法
        mc::test::TestBase::TearDownTestSuite();
    }
};

/**
 * @brief 测试模块加载功能
 */
TEST_F(builtin_module_test, TestModuleLoad) {
    auto module = mc::load_module("mc.test.module");
    ASSERT_TRUE(module != nullptr);

    EXPECT_EQ(module->name(), "mc.test.module");
    EXPECT_EQ(module->version(), "1.0.0");
}

/**
 * @brief 测试模块管理器功能
 */
TEST_F(builtin_module_test, TestModuleManager) {
    auto& manager = mc::get_module_manager();

    // 测试模块加载状态查询
    bool is_loaded = manager.is_loaded("mc.test.module");
    if (!is_loaded) {
        // 如果还没加载，先加载
        auto module = manager.require("mc.test.module");
        ASSERT_TRUE(module != nullptr);
        is_loaded = manager.is_loaded("mc.test.module");
    }
    EXPECT_TRUE(is_loaded);

    // 测试已加载模块列表
    auto loaded_modules    = manager.loaded_modules();
    bool found_test_module = std::find(
                                 loaded_modules.begin(),
                                 loaded_modules.end(),
                                 "mc.test.module") != loaded_modules.end();
    EXPECT_TRUE(found_test_module);
}

/**
 * @brief 测试反射功能
 */
TEST_F(builtin_module_test, TestReflection) {
    // 直接获取模块的反射工厂，绕过模块管理器
    // 这可以验证类型是否通过静态初始化正确注册到了工厂单例中
    using test_module_ns = mc_test_module_namespace;
    auto& direct_factory = mc::reflect::reflection_factory::instance<test_module_ns>();
    auto  direct_types   = direct_factory.get_registered_types();

    // 查询返回的类型会带上命名空间前缀
    bool found_in_direct_factory = std::find(
                                       direct_types.begin(),
                                       direct_types.end(),
                                       "mc.test.module.TestClass") != direct_types.end();
    ASSERT_TRUE(found_in_direct_factory) << "类型未能通过静态初始化注册到模块工厂";

    // 通过模块管理器加载模块，并获取其工厂
    auto module = mc::load_module("mc.test.module");
    ASSERT_TRUE(module != nullptr);

    auto factory_from_module = module->get_factory();
    ASSERT_TRUE(factory_from_module != nullptr);

    // 确认两个工厂是同一个实例
    ASSERT_EQ(&direct_factory, factory_from_module) << "模块管理器返回的工厂与类型注册的工厂不是同一个实例";

    // 验证对象创建和方法调用（创建对象可以只给类型名称，因为在同一个模块下的类型名称是唯一的）
    auto obj = factory_from_module->try_create_object("TestClass");
    ASSERT_TRUE(obj) << "无法从模块工厂创建对象";

    auto result = obj->invoke_method("getValue", {});
    EXPECT_EQ(result.as<int>(), 42);

    obj->invoke_method("setValue", {mc::variant(100)});
    auto new_result = obj->invoke_method("getValue", {});
    EXPECT_EQ(new_result.as<int>(), 100);
}

/**
 * @brief 测试便利函数
 */
TEST_F(builtin_module_test, TestUtilityFunctions) {
    // 测试便利加载函数
    auto module1 = mc::load_module("mc.test.module");
    auto module2 = mc::get_module_manager().require("mc.test.module");

    EXPECT_TRUE(module1 != nullptr);
    EXPECT_TRUE(module2 != nullptr);

    // 应该返回相同的模块实例（缓存）
    EXPECT_EQ(module1->name(), module2->name());
    EXPECT_EQ(module1->version(), module2->version());
}

TEST_F(builtin_module_test, TestUnRegisterBuiltinModule) {
    auto& manager = mc::get_module_manager();

    bool is_loaded = manager.is_loaded("mc.test.module");
    ASSERT_FALSE(is_loaded);

    auto module = manager.require("mc.test.module");
    ASSERT_TRUE(module != nullptr);
    is_loaded = manager.is_loaded("mc.test.module");
    EXPECT_TRUE(is_loaded);

    MC_UNREGISTER_BUILTIN_MODULE(mc_test_module);

    is_loaded = manager.is_loaded("mc.test.module");
    EXPECT_FALSE(is_loaded);
}

} // anonymous namespace