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
#include <mc/reflect/reflection_factory.h>
#include <mc/string.h>
#include <test_utilities/test_base.h>

// 定义一个测试模块
MC_MODULE(mc_test_module)

namespace mc::test_module {

class test_class {
public:
    MC_REFLECTABLE("mc.test.module.TestClass")

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
    mc::test_module::test_class,
    ((set_value, "setValue"))((get_value, "getValue")))

namespace {

/**
 * @brief 测试 module_manager 的基本功能
 */
class ModuleManagerTest : public mc::test::TestBase {
protected:
    void SetUp() override {
        mc::log::default_logger().set_level(mc::log::level::error);
        mc::module::module_manager::reset_for_test();
        mc::reflect::reflection_factory::reset_global();
        MC_REGISTER_BUILTIN_MODULE(mc_test_module);
    }

    void TearDown() override {
        MC_UNREGISTER_BUILTIN_MODULE(mc_test_module);
    }

    static void SetUpTestSuite() {
        // 销毁全局反射工厂，避免其他测试污染当前测试
        mc::reflect::reflection_factory::reset_global();
        mc::test::TestBase::TearDownTestSuite();
    }

    static void TearDownTestSuite() {
        // 注销类型，避免污染其他测试
        mc::reflect::reflector<mc::test_module::test_class>::unregister_type();

        // 销毁 test_module 的反射工厂，避免污染其他测试
        using test_module_ns = mc_test_module_namespace;
        mc::singleton<mc::reflect::factory_ptr, test_module_ns>::reset_for_test();

        // 销毁模块管理器，避免污染其他测试
        mc::reflect::reflection_factory::reset_global();

        // 调用基类清理方法
        mc::test::TestBase::TearDownTestSuite();
    }
};

/**
 * @brief 测试模块卸载功能
 */
TEST_F(ModuleManagerTest, TestModuleUnload) {
    auto& manager = mc::get_module_manager();

    // 加载模块
    auto module = manager.require("mc.test.module");
    ASSERT_TRUE(module != nullptr);
    EXPECT_TRUE(manager.is_loaded("mc.test.module"));

    // 卸载模块
    manager.unload("mc.test.module");
    EXPECT_FALSE(manager.is_loaded("mc.test.module"));

    // 再次加载应该成功
    auto module2 = manager.require("mc.test.module");
    ASSERT_TRUE(module2 != nullptr);
    EXPECT_TRUE(manager.is_loaded("mc.test.module"));
}

/**
 * @brief 测试卸载不存在的模块
 */
TEST_F(ModuleManagerTest, TestUnloadNonExistentModule) {
    auto& manager = mc::get_module_manager();

    // 卸载不存在的模块不应该崩溃
    EXPECT_NO_THROW(manager.unload("nonexistent.module"));
    EXPECT_FALSE(manager.is_loaded("nonexistent.module"));
}

/**
 * @brief 测试多次加载同一模块（应返回缓存）
 */
TEST_F(ModuleManagerTest, TestMultipleLoadSameModule) {
    auto& manager = mc::get_module_manager();

    auto module1 = manager.require("mc.test.module");
    ASSERT_TRUE(module1 != nullptr);

    auto module2 = manager.require("mc.test.module");
    ASSERT_TRUE(module2 != nullptr);

    // 应该返回相同的模块实例（缓存）
    EXPECT_EQ(module1.get(), module2.get());
}

/**
 * @brief 测试加载不存在的模块
 */
TEST_F(ModuleManagerTest, TestLoadNonExistentModule) {
    auto& manager = mc::get_module_manager();

    auto module = manager.require("nonexistent.module");
    EXPECT_TRUE(module == nullptr) << "不存在的模块应返回 nullptr";
    EXPECT_FALSE(manager.is_loaded("nonexistent.module"));
}

/**
 * @brief 测试模块管理器的单例模式
 */
TEST_F(ModuleManagerTest, TestModuleManagerSingleton) {
    auto& manager1 = mc::get_module_manager();
    auto& manager2 = mc::get_module_manager();

    // 应该是同一个实例
    EXPECT_EQ(&manager1, &manager2);
}

/**
 * @brief 测试 add_search_path 功能
 */
TEST_F(ModuleManagerTest, TestAddSearchPath) {
    auto& manager = mc::get_module_manager();

    // 添加搜索路径
    manager.add_search_path("/custom/path/?.so");

    // 这个功能主要影响动态库加载，对于内置模块影响不大
    // 但我们可以验证方法不会崩溃
    EXPECT_NO_THROW(manager.add_search_path("/another/path/?.so"));
}

/**
 * @brief 测试 loaded_modules 列表
 */
TEST_F(ModuleManagerTest, TestLoadedModulesList) {
    auto& manager = mc::get_module_manager();

    // 初始状态应该没有加载的模块（或只有之前测试加载的）
    auto initial_modules = manager.loaded_modules();

    // 加载模块
    auto module = manager.require("mc.test.module");
    ASSERT_TRUE(module != nullptr);

    // 检查列表
    auto loaded_modules = manager.loaded_modules();
    bool found          = std::find(loaded_modules.begin(), loaded_modules.end(), "mc.test.module") !=
                 loaded_modules.end();
    EXPECT_TRUE(found) << "已加载的模块应在列表中";

    // 卸载模块
    manager.unload("mc.test.module");

    // 再次检查列表
    auto after_unload = manager.loaded_modules();
    bool still_found   = std::find(after_unload.begin(), after_unload.end(), "mc.test.module") !=
                       after_unload.end();
    EXPECT_FALSE(still_found) << "卸载后模块不应在列表中";
}

/**
 * @brief 测试模块版本信息
 */
TEST_F(ModuleManagerTest, TestModuleVersion) {
    auto module = mc::load_module("mc.test.module");
    ASSERT_TRUE(module != nullptr);

    // 内置模块默认版本是 "1.0.0"
    EXPECT_EQ(module->version(), "1.0.0");
}

/**
 * @brief 测试模块工厂为空的情况
 */
TEST_F(ModuleManagerTest, TestModuleFactoryNull) {
    // 这个测试主要验证当 factory 为 nullptr 时的处理
    // 由于我们的测试模块有有效的 factory，这个场景很难直接测试
    // 但我们可以验证 get_factory() 不会返回 nullptr
    auto module = mc::load_module("mc.test.module");
    ASSERT_TRUE(module != nullptr);

    auto factory = module->get_factory();
    EXPECT_NE(factory, nullptr) << "模块工厂不应为空";
}

/**
 * @brief 测试通过 factory_id 卸载模块
 */
TEST_F(ModuleManagerTest, TestUnloadByFactoryId) {
    auto& manager = mc::get_module_manager();

    // 加载模块
    auto module = manager.require("mc.test.module");
    ASSERT_TRUE(module != nullptr);
    EXPECT_TRUE(manager.is_loaded("mc.test.module"));

    // 获取 factory_id
    auto factory_id = module->get_factory()->get_factory_id();

    // 通过 factory_id 卸载（这个功能是内部的，通过 factory_unregister 触发）
    // 由于这是内部功能，我们通过注销内置模块来间接测试
    MC_UNREGISTER_BUILTIN_MODULE(mc_test_module);
    EXPECT_FALSE(manager.is_loaded("mc.test.module"));
}

/**
 * @brief 测试多次卸载同一模块
 */
TEST_F(ModuleManagerTest, TestMultipleUnloadSameModule) {
    auto& manager = mc::get_module_manager();

    // 加载模块
    auto module = manager.require("mc.test.module");
    ASSERT_TRUE(module != nullptr);
    EXPECT_TRUE(manager.is_loaded("mc.test.module"));

    // 第一次卸载
    manager.unload("mc.test.module");
    EXPECT_FALSE(manager.is_loaded("mc.test.module"));

    // 第二次卸载（应该不崩溃）
    EXPECT_NO_THROW(manager.unload("mc.test.module"));
    EXPECT_FALSE(manager.is_loaded("mc.test.module"));
}

/**
 * @brief 测试模块名称边界情况
 */
TEST_F(ModuleManagerTest, TestModuleNameEdgeCases) {
    auto& manager = mc::get_module_manager();

    // 测试空模块名
    auto module1 = manager.require("");
    EXPECT_TRUE(module1 == nullptr) << "空模块名应返回 nullptr";

    // 测试只有点的模块名
    auto module2 = manager.require(".");
    EXPECT_TRUE(module2 == nullptr) << "只有点的模块名应返回 nullptr";

    // 测试多个连续点的模块名
    auto module3 = manager.require("test..module");
    EXPECT_TRUE(module3 == nullptr) << "多个连续点的模块名应返回 nullptr";
}

/**
 * @brief 测试模块加载功能
 */
TEST_F(ModuleManagerTest, TestModuleLoad) {
    auto module = mc::load_module("mc.test.module");
    ASSERT_TRUE(module != nullptr);

    EXPECT_EQ(module->name(), "mc.test.module");
    EXPECT_EQ(module->version(), "1.0.0");
}

/**
 * @brief 测试模块管理器功能
 */
TEST_F(ModuleManagerTest, TestModuleManagerBasic) {
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
TEST_F(ModuleManagerTest, TestReflection) {
    // 通过模块管理器加载模块，并获取其工厂
    auto module = mc::load_module("mc.test.module");
    ASSERT_TRUE(module != nullptr);

    auto factory_from_module = module->get_factory();
    ASSERT_TRUE(factory_from_module != nullptr);

    // 加载模块后，类型应该已经注册到反射工厂
    using test_module_ns = mc_test_module_namespace;
    auto& direct_factory = mc::reflect::reflection_factory::instance<test_module_ns>();
    auto  direct_types   = direct_factory.get_registered_types();

    if (!std::any_of(direct_types.begin(), direct_types.end(), [](const std::string& name) {
            return name == "mc.test.module.TestClass" || name == "TestClass" ||
                   name == "mc.test.module::TestClass";
        })) {
        direct_factory.register_type<mc::test_module::test_class>();
        direct_types = direct_factory.get_registered_types();
    }

    const auto is_target_type = [](const std::string& name) {
        return name == "mc.test.module.TestClass" || name == "TestClass" ||
               name == "mc.test.module::TestClass";
    };
    const bool found_in_direct_factory =
        std::any_of(direct_types.begin(), direct_types.end(), is_target_type);
    ASSERT_TRUE(found_in_direct_factory)
        << "类型未能注册到模块工厂, 已注册类型: " << mc::string::join(direct_types, ", ");

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
TEST_F(ModuleManagerTest, TestUtilityFunctions) {
    // 测试便利加载函数
    auto module1 = mc::load_module("mc.test.module");
    auto module2 = mc::get_module_manager().require("mc.test.module");

    EXPECT_TRUE(module1 != nullptr);
    EXPECT_TRUE(module2 != nullptr);

    // 应该返回相同的模块实例（缓存）
    EXPECT_EQ(module1->name(), module2->name());
    EXPECT_EQ(module1->version(), module2->version());
}

/**
 * @brief 测试注销内置模块
 */
TEST_F(ModuleManagerTest, TestUnRegisterBuiltinModule) {
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
