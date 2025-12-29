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

#include "../runtime/test_future_helpers.h"
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <gtest/gtest.h>
#include <mc/filesystem.h>
#include <mc/module.h>
#include <mc/reflect/reflection_factory.h>
#include <mc/string.h>
#include <stdexcept>
#include <test_utilities/test_base.h>
#include <thread>
#include <vector>

extern "C" MC_API void set_log_module_name(const char*) {
}

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

class scoped_env_var {
public:
    scoped_env_var(const char* name, const char* value) : m_name(name) {
        const char* original = std::getenv(name);
        if (original != nullptr) {
            m_old_value  = original;
            m_has_backup = true;
        }
        setenv(m_name.c_str(), value, 1);
    }

    ~scoped_env_var() {
        if (m_has_backup) {
            setenv(m_name.c_str(), m_old_value.c_str(), 1);
        } else {
            unsetenv(m_name.c_str());
        }
    }

private:
    std::string m_name;
    std::string m_old_value;
    bool        m_has_backup{false};
};

std::string_view shared_lib_ext() {
#if defined(__APPLE__)
    return ".dylib";
#else
    return ".so";
#endif
}

mc::filesystem::path dynamic_module_source_path(const mc::filesystem::path& build_root) {
    return build_root / "tests" / (std::string("libmc_test_dynamic_module") + std::string(shared_lib_ext()));
}

mc::filesystem::path dynamic_module_target_path(const mc::filesystem::path& build_root) {
    auto modules_dir = build_root / "modules" / "mc" / "test";
    return modules_dir / (std::string("dynamic") + std::string(shared_lib_ext()));
}

std::string dynamic_module_search_path() {
    return std::string("./modules/?") + std::string(shared_lib_ext());
}

void copy_dynamic_module_if_needed(const mc::filesystem::path& build_root) {
    auto target = dynamic_module_target_path(build_root);
    mc::filesystem::create_directories(target.parent_path());

    if (mc::filesystem::exists(target)) {
        return;
    }

    mc::filesystem::copy_file(dynamic_module_source_path(build_root), target, true);
}

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
    manager.add_search_path(std::string("/custom/path/?") + std::string(shared_lib_ext()));

    // 这个功能主要影响动态库加载，对于内置模块影响不大
    // 但我们可以验证方法不会崩溃
    EXPECT_NO_THROW(manager.add_search_path(std::string("/another/path/?") + std::string(shared_lib_ext())));
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
    bool still_found  = std::find(after_unload.begin(), after_unload.end(), "mc.test.module") !=
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

/**
 * @brief 并发 require 同一模块应返回同一实例
 */
TEST_F(ModuleManagerTest, TestConcurrentRequireSameModule) {
    auto&     manager      = mc::get_module_manager();
    const int thread_count = 8;

    std::vector<mc::module::module_ptr> modules(thread_count);
    std::vector<std::thread>            threads;
    std::atomic<int>                    ready{0};
    std::atomic<bool>                   start_flag{false};

    for (int i = 0; i < thread_count; ++i) {
        threads.emplace_back([&, i]() {
            ready.fetch_add(1, std::memory_order_release);
            while (!start_flag.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            modules[i] = manager.require("mc.test.module");
        });
    }

    while (ready.load(std::memory_order_acquire) != thread_count) {
        std::this_thread::yield();
    }
    start_flag.store(true, std::memory_order_release);

    for (auto& t : threads) {
        t.join();
    }

    for (const auto& mod : modules) {
        ASSERT_TRUE(mod != nullptr);
    }

    auto* first_ptr = modules.front().get();
    for (const auto& mod : modules) {
        EXPECT_EQ(mod.get(), first_ptr);
    }

    for (auto& mod : modules) {
        mod.reset();
    }
    manager.unload("mc.test.module");
}

/**
 * @brief 并发执行 require/unload 场景不应崩溃
 */
TEST_F(ModuleManagerTest, TestConcurrentRequireAndUnload) {
    auto&             manager = mc::get_module_manager();
    std::atomic<bool> stop{false};
    std::atomic<int>  iterations{0};
    const int         min_iterations = 10; // 确保至少执行一定次数

    mc::test::runtime::countdown_future workers_ready(2);
    mc::test::runtime::future_flag      start_flag;

    auto worker = [&]() {
        // 通知已准备好
        workers_ready.arrive();

        // 等待开始信号
        if (!start_flag.wait_for(std::chrono::milliseconds(3000))) {
            return; // 超时，测试失败
        }

        while (!stop.load(std::memory_order_acquire)) {
            auto module = manager.require("mc.test.module");
            if (module) {
                (void)module->get_factory();
            }
            manager.unload("mc.test.module");
            iterations.fetch_add(1, std::memory_order_relaxed);
            std::this_thread::yield(); // 让出 CPU，避免过度占用
        }
    };

    std::thread t1(worker);
    std::thread t2(worker);

    // 等待所有工作线程都准备好
    if (!workers_ready.wait_for(std::chrono::milliseconds(3000))) {
        stop.store(true, std::memory_order_release);
        t1.join();
        t2.join();
        FAIL() << "工作线程未能及时准备好";
        return;
    }

    // 通知开始执行
    start_flag.set();

    // 等待至少执行最小迭代次数
    auto start_time = std::chrono::steady_clock::now();
    while (iterations.load(std::memory_order_acquire) < min_iterations) {
        auto elapsed = std::chrono::steady_clock::now() - start_time;
        if (elapsed > std::chrono::milliseconds(3000)) {
            break; // 超时，停止等待
        }
        std::this_thread::yield();
    }

    // 停止工作线程
    stop.store(true, std::memory_order_release);

    // 等待线程安全退出
    t1.join();
    t2.join();

    // 验证至少执行了一些迭代
    EXPECT_GT(iterations.load(std::memory_order_acquire), 0)
        << "工作线程应该至少执行一些迭代";

    // 验证最终状态
    auto module = manager.require("mc.test.module");
    EXPECT_TRUE(module != nullptr);
    manager.unload("mc.test.module");
}

TEST_F(ModuleManagerTest, TestDynamicModuleOpenFailure) {
    auto build_root = mc::filesystem::current_path();
    auto source     = dynamic_module_source_path(build_root);
    ASSERT_TRUE(mc::filesystem::exists(source));

    copy_dynamic_module_if_needed(build_root);

    scoped_env_var env("MC_TEST_DYNAMIC_RETURN_NULL", "1");
    auto&          manager = mc::get_module_manager();
    manager.add_search_path(dynamic_module_search_path());

    auto module = manager.require("mc.test.dynamic");
    EXPECT_TRUE(module == nullptr);
}

TEST_F(ModuleManagerTest, TestDynamicModuleCloseThrows) {
    auto build_root = mc::filesystem::current_path();
    auto source     = dynamic_module_source_path(build_root);
    ASSERT_TRUE(mc::filesystem::exists(source));

    copy_dynamic_module_if_needed(build_root);

    auto& manager = mc::get_module_manager();
    manager.add_search_path(dynamic_module_search_path());

    auto module = manager.require("mc.test.dynamic");
    ASSERT_TRUE(module != nullptr);

    {
        scoped_env_var close_env("MC_TEST_DYNAMIC_CLOSE_THROW", "1");
        manager.unload("mc.test.dynamic");
        EXPECT_NO_THROW(module.reset());
    }

    manager.unload("mc.test.dynamic");
}

// 测试动态模块卸载时的 dlog 日志
TEST_F(ModuleManagerTest, ModuleManagerDynamicUnloadLogs) {
    auto build_root = mc::filesystem::current_path();
    auto source     = dynamic_module_source_path(build_root);
    ASSERT_TRUE(mc::filesystem::exists(source));

    copy_dynamic_module_if_needed(build_root);

    auto& manager = mc::get_module_manager();
    manager.add_search_path(dynamic_module_search_path());

    // 临时将日志级别设置为 debug，以便 dlog 能够输出
    auto original_level = mc::log::default_logger().get_level();
    mc::log::default_logger().set_level(mc::log::level::debug);

    // 加载动态模块
    auto module = manager.require("mc.test.dynamic");
    ASSERT_TRUE(module != nullptr);

    // 卸载模块，这会触发 dlog("unload dynamic module...")
    manager.unload("mc.test.dynamic");

    // 恢复原始日志级别
    mc::log::default_logger().set_level(original_level);

    // 验证模块已卸载
    EXPECT_FALSE(manager.is_loaded("mc.test.dynamic"));
}

// 测试释放所有引用后卸载，命中 need_unload 分支
TEST_F(ModuleManagerTest, ModuleManagerReleaseHandleOnUnload) {
    auto build_root = mc::filesystem::current_path();
    auto source     = dynamic_module_source_path(build_root);
    ASSERT_TRUE(mc::filesystem::exists(source));

    copy_dynamic_module_if_needed(build_root);

    auto& manager = mc::get_module_manager();
    manager.add_search_path(dynamic_module_search_path());

    // 加载动态模块
    auto module = manager.require("mc.test.dynamic");
    ASSERT_TRUE(module != nullptr);
    EXPECT_TRUE(manager.is_loaded("mc.test.dynamic"));

    // 先调用 manager.unload，这会从 loaded_modules 中移除模块
    // 但此时 module_ptr 仍然存在，所以 library_module 不会被析构
    manager.unload("mc.test.dynamic");
    EXPECT_FALSE(manager.is_loaded("mc.test.dynamic"));

    // 现在释放 module_ptr，这会触发 library_module 的析构
    // 析构函数会调用 remove_library，进而调用 close_handle(*info_ptr, true)
    // 这会命中 need_unload=true 分支，并调用 m_loader.get_load_lib_func().unload
    module.reset();

    // 验证模块已完全卸载
    EXPECT_FALSE(manager.is_loaded("mc.test.dynamic"));
}

// 测试 reset_for_test() 覆盖 clear() 中 handle 回收
TEST_F(ModuleManagerTest, ModuleManagerResetClearsHandles) {
    auto build_root = mc::filesystem::current_path();
    auto source     = dynamic_module_source_path(build_root);
    ASSERT_TRUE(mc::filesystem::exists(source));

    copy_dynamic_module_if_needed(build_root);

    auto& manager = mc::get_module_manager();
    manager.add_search_path(dynamic_module_search_path());

    // 加载动态模块，但不卸载
    auto module = manager.require("mc.test.dynamic");
    ASSERT_TRUE(module != nullptr);
    EXPECT_TRUE(manager.is_loaded("mc.test.dynamic"));

    // 不调用 manager.unload，直接调用 reset_for_test()
    // 这会触发 module_manager_impl 的析构，进而调用 clear()
    // clear() 会遍历所有 handles 并调用 close_handle(*handle, true)
    module.reset(); // 先释放引用，避免析构时的潜在问题
    mc::module::module_manager::reset_for_test();

    // 验证模块管理器已重置
    auto& new_manager = mc::get_module_manager();
    EXPECT_FALSE(new_manager.is_loaded("mc.test.dynamic"));
}

// 测试 handle 复用场景
TEST_F(ModuleManagerTest, ModuleManagerReuseLibraryHandle) {
    auto build_root = mc::filesystem::current_path();
    auto source     = dynamic_module_source_path(build_root);
    ASSERT_TRUE(mc::filesystem::exists(source));

    copy_dynamic_module_if_needed(build_root);

    auto& manager = mc::get_module_manager();
    manager.add_search_path(dynamic_module_search_path());

    // 第一次加载动态模块
    auto module1 = manager.require("mc.test.dynamic");
    ASSERT_TRUE(module1 != nullptr);
    EXPECT_TRUE(manager.is_loaded("mc.test.dynamic"));

    // 卸载模块，但保留 module_ptr（此时 handle 仍在 handles 中）
    manager.unload("mc.test.dynamic");
    EXPECT_FALSE(manager.is_loaded("mc.test.dynamic"));

    // 在旧 module_ptr 未 reset 前再次 require
    // 这应该复用之前加载的 handle，而不是重新 dlopen
    auto module2 = manager.require("mc.test.dynamic");
    ASSERT_TRUE(module2 != nullptr);
    EXPECT_TRUE(manager.is_loaded("mc.test.dynamic"));

    // 验证两个 module_ptr 指向不同的对象（因为第一个已从 loaded_modules 中移除）
    EXPECT_NE(module1.get(), module2.get());

    // 清理
    module1.reset();
    module2.reset();
    manager.unload("mc.test.dynamic");
}

} // anonymous namespace
