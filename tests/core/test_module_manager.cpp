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

#include "mc/core/module_manager.h"
#include "mc/core/config_manager.h"
#include "mc/core/module.h"
#include <gtest/gtest.h>
#include <memory>
#include <filesystem>

namespace mc {
namespace test {

// 测试用模块类
class test_module : public module {
public:
    test_module(std::string name, std::string version, std::vector<std::string> deps = {})
        : m_info{std::move(name), std::move(version), std::move(deps)}
        , m_init_called(false)
        , m_start_called(false)
        , m_stop_called(false)
        , m_unload_called(false)
    {}
    
    const module_info& get_info() const override { return m_info; }
    
    bool init() override {
        m_init_called = true;
        return true;
    }
    
    bool start() override {
        m_start_called = true;
        return true;
    }
    
    bool stop() override {
        m_stop_called = true;
        return true;
    }
    
    bool unload() override {
        m_unload_called = true;
        return true;
    }
    
    bool is_init_called() const { return m_init_called; }
    bool is_start_called() const { return m_start_called; }
    bool is_stop_called() const { return m_stop_called; }
    bool is_unload_called() const { return m_unload_called; }
    
private:
    module_info m_info;
    bool m_init_called;
    bool m_start_called;
    bool m_stop_called;
    bool m_unload_called;
};

// 模块管理器基本功能测试
TEST(ModuleManagerTest, BasicFunctions) {
    // 创建模块管理器
    module_manager manager;
    
    // 测试设置和获取模块目录
    std::string module_dir = "/path/to/modules";
    manager.set_module_dir(module_dir);
    EXPECT_EQ(manager.module_dir(), module_dir);
    
    // 默认情况下应该没有模块
    EXPECT_TRUE(manager.find_module("any_module") == nullptr);
}

// 模块注册和查找测试
TEST(ModuleManagerTest, ModuleRegistration) {
    module_manager manager;
    
    // 创建测试模块
    auto module1 = std::make_shared<test_module>("module1", "1.0.0");
    auto module2 = std::make_shared<test_module>("module2", "1.0.0");
    
    // 注册模块
    EXPECT_TRUE(manager.register_module(module1));
    EXPECT_TRUE(manager.register_module(module2));
    
    // 验证可以找到注册的模块
    EXPECT_NE(manager.find_module("module1"), nullptr);
    EXPECT_NE(manager.find_module("module2"), nullptr);
    EXPECT_EQ(manager.find_module("module3"), nullptr);
}

// 模块生命周期测试
TEST(ModuleManagerTest, ModuleLifecycle) {
    module_manager manager;
    
    // 创建测试模块
    auto module1 = std::make_shared<test_module>("module1", "1.0.0");
    auto module2 = std::make_shared<test_module>("module2", "1.0.0");
    
    // 注册模块
    manager.register_module(module1);
    manager.register_module(module2);
    
    // 初始化所有模块
    bool init_result = manager.init_modules();
    EXPECT_TRUE(init_result);
    EXPECT_TRUE(module1->is_init_called());
    EXPECT_TRUE(module2->is_init_called());
    
    // 启动所有模块
    bool start_result = manager.start_modules();
    EXPECT_TRUE(start_result);
    EXPECT_TRUE(module1->is_start_called());
    EXPECT_TRUE(module2->is_start_called());
    
    // 停止所有模块
    bool stop_result = manager.stop_modules();
    EXPECT_TRUE(stop_result);
    EXPECT_TRUE(module1->is_stop_called());
    EXPECT_TRUE(module2->is_stop_called());
    
    // 卸载所有模块
    manager.unload_all_modules();
    EXPECT_TRUE(module1->is_unload_called());
    EXPECT_TRUE(module2->is_unload_called());
    
    // 应该找不到模块了
    EXPECT_EQ(manager.find_module("module1"), nullptr);
    EXPECT_EQ(manager.find_module("module2"), nullptr);
}

// 模块依赖关系测试
TEST(ModuleManagerTest, ModuleDependencies) {
    module_manager manager;
    
    // 创建带依赖关系的测试模块，使用显式构造向量而非初始化列表
    auto module1 = std::make_shared<test_module>("module1", "1.0.0");
    
    std::vector<std::string> module2_deps;
    module2_deps.push_back("module1");
    auto module2 = std::make_shared<test_module>("module2", "1.0.0", module2_deps);
    
    std::vector<std::string> module3_deps;
    module3_deps.push_back("module2");
    auto module3 = std::make_shared<test_module>("module3", "1.0.0", module3_deps);
    
    // 按照依赖顺序注册模块
    manager.register_module(module1);
    manager.register_module(module2);
    manager.register_module(module3);
    
    // 初始化，启动，停止和卸载模块应该按照依赖顺序进行
    EXPECT_TRUE(manager.init_modules());
    EXPECT_TRUE(manager.start_modules());
    EXPECT_TRUE(manager.stop_modules());
    manager.unload_all_modules();
}

// 模块加载测试
TEST(ModuleManagerTest, ModuleLoading) {
    module_manager manager;
    
    // 设置模块名称列表
    std::vector<std::string> module_names = {"module1", "module2"};
    
    // 注册模块路径中的模块
    // 注意：这里我们不实际加载模块，因为这需要实际的动态库
    // 在真实测试中，需要准备测试用的动态库模块
    
    // 但我们可以测试空列表的情况，这不应该导致错误
    manager.load_modules({});
    // 加载空列表后应该没有模块
    EXPECT_EQ(manager.find_module("any_module"), nullptr);
}

} // namespace test
} // namespace mc 