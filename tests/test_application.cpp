#include <gtest/gtest.h>
#include "appbase/application.h"
#include <filesystem>

using namespace appbase;

// 测试插件类
class test_plugin : public plugin {
public:
    test_plugin(const std::string& name) : name_(name) {}
    
    std::string name() const override { return name_; }
    
    bool initialize() override {
        initialized_ = true;
        return true;
    }
    
    void startup() override {
        started_ = true;
    }
    
    void shutdown() override {
        started_ = false;
    }
    
    bool is_initialized() const { return initialized_; }
    bool is_started() const { return started_; }
    
private:
    std::string name_;
    bool initialized_ = false;
    bool started_ = false;
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

// 测试插件管理
TEST(ApplicationTest, PluginManagement) {
    application& app = application::instance();
    
    // 注册插件
    auto plugin1 = std::make_unique<test_plugin>("plugin1");
    auto plugin2 = std::make_unique<test_plugin>("plugin2");
    
    test_plugin* p1 = plugin1.get();
    test_plugin* p2 = plugin2.get();
    
    app.register_plugin(std::move(plugin1));
    app.register_plugin(std::move(plugin2));
    
    // 验证插件查找
    EXPECT_EQ(p1, app.find_plugin("plugin1"));
    EXPECT_EQ(p2, app.find_plugin("plugin2"));
    EXPECT_EQ(nullptr, app.find_plugin("nonexistent"));
    
    // 初始化插件
    app.initialize();
    
    // 验证插件初始化状态
    EXPECT_TRUE(p1->is_initialized());
    EXPECT_TRUE(p2->is_initialized());
    
    // 启动插件
    app.startup();
    
    // 验证插件启动状态
    EXPECT_TRUE(p1->is_started());
    EXPECT_TRUE(p2->is_started());
    
    // 关闭插件
    app.shutdown();
    
    // 验证插件关闭状态
    EXPECT_FALSE(p1->is_started());
    EXPECT_FALSE(p2->is_started());
}

// 测试重复注册插件
TEST(ApplicationTest, DuplicatePlugin) {
    application& app = application::instance();
    
    // 注册插件
    auto plugin1 = std::make_unique<test_plugin>("unique");
    auto plugin2 = std::make_unique<test_plugin>("unique"); // 相同名称
    
    test_plugin* p1 = plugin1.get();
    test_plugin* p2 = plugin2.get();
    
    app.register_plugin(std::move(plugin1));
    app.register_plugin(std::move(plugin2)); // 应该被忽略
    
    // 验证只有第一个插件被注册
    EXPECT_EQ(p1, app.find_plugin("unique"));
    EXPECT_NE(p2, app.find_plugin("unique"));
}