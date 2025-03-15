#include "mc/core/application.h"
#include "mc/core/module.h"
#include "mc/core/service.h"
#include "mc/core/supervisor.h"
#include <filesystem>
#include <gtest/gtest.h>
#include <string>
#include <vector>

using namespace mc;

// 测试模块类
class test_module : public module_base<test_module> {
public:
    test_module(const char* name = "test_module") {
        module_info info;
        info.name = name;
        set_info(info);
    }

    bool load() override {
        m_loaded = true;
        return true;
    }

    void post_load() override {
        m_post_loaded = true;
    }

    void pre_unload() override {
        m_pre_unloaded = true;
    }

    bool unload() override {
        m_loaded = false;
        return true;
    }

    void cleanup_resources() override {
        m_resources_cleaned = true;
    }

    std::vector<std::string> get_active_services() const override {
        return m_active_services;
    }

    bool is_loaded() const {
        return m_loaded;
    }

    bool is_post_loaded() const {
        return m_post_loaded;
    }

    bool is_pre_unloaded() const {
        return m_pre_unloaded;
    }

    bool is_resources_cleaned() const {
        return m_resources_cleaned;
    }

    // 为了测试目的，提供公共方法来访问 get_info 和 set_info
    module_info get_module_info() const {
        return get_info();
    }

    void set_module_info(const module_info& info) {
        set_info(info);
    }

private:
    bool m_loaded = false;
    bool m_post_loaded = false;
    bool m_pre_unloaded = false;
    bool m_resources_cleaned = false;
    std::vector<std::string> m_active_services;
};

// 测试服务类
class test_service : public service_base<test_service> {
public:
    test_service() = default;

    bool init(const service_config& config) override {
        set_config(config);
        set_state(service_state::stopped);
        m_initialized = true;
        return true;
    }

    bool start() override {
        if (!m_initialized) {
            return false;
        }
        set_state(service_state::starting);
        m_started = true;
        set_state(service_state::running);
        return true;
    }

    bool stop() override {
        if (!m_started) {
            return false;
        }
        set_state(service_state::stopping);
        m_started = false;
        set_state(service_state::stopped);
        return true;
    }

    void cleanup() override {
        m_initialized = false;
        m_resources_cleaned = true;
    }

    bool is_healthy() const override {
        return m_initialized && (get_state() == service_state::running);
    }

    bool is_initialized() const {
        return m_initialized;
    }

    bool is_started() const {
        return m_started;
    }

    bool is_resources_cleaned() const {
        return m_resources_cleaned;
    }

private:
    bool m_initialized = false;
    bool m_started = false;
    bool m_resources_cleaned = false;
};

// 测试应用程序单例
TEST(ApplicationTest, Singleton) {
    application& app1 = application::instance();
    application& app2 = application::instance();

    // 验证是同一个实例
    EXPECT_EQ(&app1, &app2);
}

// 测试版本号管理
TEST(ApplicationTest, Version) {
    application& app = application::instance();

    // 设置版本号
    app.set_version("1.0.0");

    // 验证版本号
    EXPECT_EQ("1.0.0", app.version());
}

// 测试配置目录管理
TEST(ApplicationTest, ConfigDir) {
    application& app = application::instance();

    // 设置配置目录
    fs::path config_dir = "/tmp/config";
    app.set_config_dir(config_dir);

    // 验证配置目录
    EXPECT_EQ(config_dir, app.config_dir());
}

// 测试模块管理
TEST(ApplicationTest, ModuleManagement) {
    application& app = application::instance();

    // 注册模块
    auto module1 = std::make_shared<test_module>("module1");
    test_module* m1 = module1.get();

    app.register_module(module1);

    // 验证模块查找
    EXPECT_EQ(m1, app.find_module("module1"));
    EXPECT_EQ(nullptr, app.find_module("nonexistent"));

    // 加载模块
    EXPECT_TRUE(app.load_module("module1"));
    EXPECT_TRUE(m1->is_loaded());

    // 初始化应用程序
    app.initialize();

    // 验证模块post_load被调用
    EXPECT_TRUE(m1->is_post_loaded());

    // 卸载模块
    EXPECT_TRUE(app.unload_module("module1"));
    EXPECT_TRUE(m1->is_pre_unloaded());
    EXPECT_FALSE(m1->is_loaded());
}

// 测试服务管理
TEST(ApplicationTest, ServiceManagement) {
    application& app = application::instance();

    // 注册服务工厂
    app.register_service("test_service", []() -> service_ptr {
        return std::make_shared<test_service>();
    });

    // 创建服务
    service_config config;
    config.name = "service1";
    config.type = "test_service";

    auto service = app.create_service("test_service", config);
    ASSERT_NE(nullptr, service);

    // 获取根监督器
    auto root_supervisor = app.get_root_supervisor();
    ASSERT_NE(nullptr, root_supervisor);

    // 添加服务到根监督器
    EXPECT_TRUE(root_supervisor->add_service(service));

    // 验证服务查找
    EXPECT_EQ(service, app.get_service("service1"));
    EXPECT_EQ(nullptr, app.get_service("nonexistent"));

    // 启动应用程序
    app.start();
    
    // 手动启动服务
    service->start();

    // 验证服务状态
    auto test_svc = std::dynamic_pointer_cast<test_service>(service);
    ASSERT_NE(nullptr, test_svc);
    EXPECT_TRUE(test_svc->is_initialized());
    EXPECT_TRUE(test_svc->is_started());
    EXPECT_TRUE(test_svc->is_healthy());

    // 停止应用程序
    app.stop();
    app.cleanup();

    // 验证应用程序已停止
    EXPECT_TRUE(app.is_stopped());
}

// 测试监督器管理
TEST(ApplicationTest, SupervisorManagement) {
    application& app = application::instance();

    // 创建子监督器
    supervisor_config config;
    config.name = "child_supervisor";
    config.strategy = supervisor_strategy::one_for_one;

    auto child = app.create_supervisor(config);
    ASSERT_NE(nullptr, child);

    // 获取根监督器
    auto root = app.get_root_supervisor();
    ASSERT_NE(nullptr, root);

    // 添加子监督器到根监督器
    EXPECT_TRUE(root->add_child(child));

    // 验证子监督器查找
    auto found_child = root->get_child(child->get_config().name);
    ASSERT_NE(nullptr, found_child);
    EXPECT_EQ(child, found_child);
    EXPECT_EQ(nullptr, root->get_child("nonexistent"));

    // 启动应用程序
    app.start();

    // 停止应用程序
    app.stop();
    app.cleanup();

    // 验证应用程序已停止
    EXPECT_TRUE(app.is_stopped());
}

// 测试模块依赖关系
TEST(ApplicationTest, ModuleDependencies) {
    application& app = application::instance();

    // 创建依赖模块
    auto dependency = std::make_shared<test_module>("dependency");
    app.register_module(dependency);

    // 创建依赖于dependency的模块
    auto dependent = std::make_shared<test_module>("dependent");
    module_info info = dependent->get_module_info();
    info.dependencies.push_back("dependency");
    dependent->set_module_info(info);
    app.register_module(dependent);

    // 加载依赖模块
    EXPECT_TRUE(app.load_module("dependency"));

    // 加载依赖于dependency的模块
    EXPECT_TRUE(app.load_module("dependent"));

    // 尝试卸载被依赖的模块，应该失败
    EXPECT_FALSE(app.unload_module("dependency"));

    // 先卸载依赖于dependency的模块
    EXPECT_TRUE(app.unload_module("dependent"));

    // 现在可以卸载dependency模块
    EXPECT_TRUE(app.unload_module("dependency"));
}