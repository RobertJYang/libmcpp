/*
* Copyright (c) 2025 Huawei Technologies Co., Ltd.
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
#include <mc/core/plugin_manager.h>
#include <mc/core/plugin.h>
#include <mc/core/service_factory.h>
#include <test_utilities/test_base.h>

using namespace mc;
using namespace mc::core;

// 测试用插件实现
class test_plugin : public plugin {
public:
    test_plugin(const std::string& name, const std::string& version)
        : m_info{name, version, {}} {
    }

    const plugin_info& get_info() const override {
        return m_info;
    }

    bool init(service_factory& factory) override {
        m_init_called = true;
        return true;
    }

    bool m_init_called = false;

private:
    plugin_info m_info;
};

// 测试用服务工厂
class test_service_factory : public service_factory {
public:
    test_service_factory() = default;
};

class plugin_manager_test : public mc::test::TestBase {
protected:
    void SetUp() override {
        TestBase::SetUp();
        manager = std::make_unique<plugin_manager>();
        factory  = std::make_unique<test_service_factory>();
    }

    void TearDown() override {
        manager->unload_all_plugins();
        manager.reset();
        factory.reset();
        TestBase::TearDown();
    }

    std::unique_ptr<plugin_manager>      manager;
    std::unique_ptr<test_service_factory> factory;
};

// 测试构造函数和析构函数
TEST_F(plugin_manager_test, constructor_destructor) {
    plugin_manager mgr;
    EXPECT_TRUE(mgr.plugin_dir().empty());
}

// 测试设置和获取插件目录
TEST_F(plugin_manager_test, set_get_plugin_dir) {
    EXPECT_TRUE(manager->plugin_dir().empty());

    manager->set_plugin_dir("/tmp/plugins");
    EXPECT_EQ(manager->plugin_dir(), "/tmp/plugins");

    manager->set_plugin_dir("/another/path");
    EXPECT_EQ(manager->plugin_dir(), "/another/path");
}

// 测试注册插件
TEST_F(plugin_manager_test, register_plugin) {
    auto plugin = std::make_shared<test_plugin>("test_plugin", "1.0.0");
    EXPECT_TRUE(manager->register_plugin(plugin));
    EXPECT_NE(manager->find_plugin("test_plugin"), nullptr);
}

// 测试注册空插件（应该失败）
TEST_F(plugin_manager_test, register_null_plugin) {
    EXPECT_FALSE(manager->register_plugin(nullptr));
}

// 测试注册重复插件
TEST_F(plugin_manager_test, register_duplicate_plugin) {
    auto plugin1 = std::make_shared<test_plugin>("test_plugin", "1.0.0");
    auto plugin2 = std::make_shared<test_plugin>("test_plugin", "2.0.0");

    EXPECT_TRUE(manager->register_plugin(plugin1));
    EXPECT_FALSE(manager->register_plugin(plugin2));
}

// 测试查找插件
TEST_F(plugin_manager_test, find_plugin) {
    auto plugin = std::make_shared<test_plugin>("test_plugin", "1.0.0");
    manager->register_plugin(plugin);

    auto* found = manager->find_plugin("test_plugin");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->get_info().name, "test_plugin");
    EXPECT_EQ(found->get_info().version, "1.0.0");
}

// 测试查找不存在的插件
TEST_F(plugin_manager_test, find_nonexistent_plugin) {
    EXPECT_EQ(manager->find_plugin("nonexistent"), nullptr);
}

// 测试获取加载的插件列表
TEST_F(plugin_manager_test, get_loaded_plugins) {
    auto plugin1 = std::make_shared<test_plugin>("plugin1", "1.0.0");
    auto plugin2 = std::make_shared<test_plugin>("plugin2", "2.0.0");

    manager->register_plugin(plugin1);
    manager->register_plugin(plugin2);

    auto plugins = manager->get_loaded_plugins();
    EXPECT_EQ(plugins.size(), 2);
    EXPECT_TRUE(std::find(plugins.begin(), plugins.end(), "plugin1") != plugins.end());
    EXPECT_TRUE(std::find(plugins.begin(), plugins.end(), "plugin2") != plugins.end());
}

// 测试卸载插件
TEST_F(plugin_manager_test, unload_plugin) {
    auto plugin = std::make_shared<test_plugin>("test_plugin", "1.0.0");
    manager->register_plugin(plugin);

    EXPECT_TRUE(manager->unload_plugin("test_plugin"));
    EXPECT_EQ(manager->find_plugin("test_plugin"), nullptr);
}

// 测试卸载不存在的插件
TEST_F(plugin_manager_test, unload_nonexistent_plugin) {
    EXPECT_FALSE(manager->unload_plugin("nonexistent"));
}

// 测试卸载所有插件
TEST_F(plugin_manager_test, unload_all_plugins) {
    auto plugin1 = std::make_shared<test_plugin>("plugin1", "1.0.0");
    auto plugin2 = std::make_shared<test_plugin>("plugin2", "2.0.0");

    manager->register_plugin(plugin1);
    manager->register_plugin(plugin2);

    manager->unload_all_plugins();

    EXPECT_EQ(manager->find_plugin("plugin1"), nullptr);
    EXPECT_EQ(manager->find_plugin("plugin2"), nullptr);
    EXPECT_TRUE(manager->get_loaded_plugins().empty());
}

// 测试初始化插件
TEST_F(plugin_manager_test, init_plugins) {
    auto plugin1 = std::make_shared<test_plugin>("plugin1", "1.0.0");
    auto plugin2 = std::make_shared<test_plugin>("plugin2", "2.0.0");

    manager->register_plugin(plugin1);
    manager->register_plugin(plugin2);

    EXPECT_TRUE(manager->init_plugins(*factory));
    EXPECT_TRUE(plugin1->m_init_called);
    EXPECT_TRUE(plugin2->m_init_called);
}

// 测试加载插件列表（空列表）
TEST_F(plugin_manager_test, load_plugins_empty_list) {
    // 设置插件目录
    manager->set_plugin_dir("/tmp/plugins");

    // 空列表应该返回false（因为目录可能不存在）
    EXPECT_FALSE(manager->load_plugins({}));
}

// 测试多个插件管理
TEST_F(plugin_manager_test, multiple_plugins) {
    auto plugin1 = std::make_shared<test_plugin>("plugin1", "1.0.0");
    auto plugin2 = std::make_shared<test_plugin>("plugin2", "2.0.0");
    auto plugin3 = std::make_shared<test_plugin>("plugin3", "3.0.0");

    manager->register_plugin(plugin1);
    manager->register_plugin(plugin2);
    manager->register_plugin(plugin3);

    EXPECT_EQ(manager->get_loaded_plugins().size(), 3);
    EXPECT_NE(manager->find_plugin("plugin1"), nullptr);
    EXPECT_NE(manager->find_plugin("plugin2"), nullptr);
    EXPECT_NE(manager->find_plugin("plugin3"), nullptr);

    // 卸载一个插件，其他应该还在
    manager->unload_plugin("plugin2");
    EXPECT_NE(manager->find_plugin("plugin1"), nullptr);
    EXPECT_EQ(manager->find_plugin("plugin2"), nullptr);
    EXPECT_NE(manager->find_plugin("plugin3"), nullptr);
}
