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
#include <mc/core/config_manager.h>
#include <mc/core/config_schema.h>
#include <mc/singleton.h>
#include <mc/variant.h>
#include <test_utilities/test_base.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <optional>
#include <sys/stat.h>
#include <unistd.h>
#include <string>
#include <vector>

using namespace mc;
using namespace mc::core;

namespace {

// 辅助函数：确保共享内存锁文件存在，避免依赖engine测试环境
void ensure_shm_lock_file() {
#ifdef __linux__
    constexpr const char* lock_path = "/dev/shm/init_shm.lock";
    std::ofstream         file(lock_path, std::ios::app);
    if (!file.is_open()) {
        return;
    }
    file.close();
    chmod(lock_path, 0666);
#endif
}

// 辅助函数：创建带后缀的临时文件
std::string create_temp_file_with_suffix(const std::string& content, std::string_view suffix) {
    auto tmp_dir = mc::filesystem::temp_directory_path();
    auto tmp_file = tmp_dir / "mc_config_test_XXXXXX";
    std::string pattern = tmp_file.string();
    pattern += suffix;

    std::vector<char> buffer(pattern.begin(), pattern.end());
    buffer.push_back('\0');

    int fd = mkstemps(buffer.data(), static_cast<int>(suffix.size()));
    if (fd == -1) {
        return "";
    }

    if (!content.empty()) {
        ssize_t written = write(fd, content.c_str(), content.size());
        if (written < 0) {
            close(fd);
            unlink(buffer.data());
            return "";
        }
    }

    close(fd);
    return std::string(buffer.data());
}

// 辅助函数：创建临时JSON文件
std::string create_temp_json_file(const std::string& content) {
    return create_temp_file_with_suffix(content, ".json");
}

} // namespace

class config_manager_test : public mc::test::TestBase {
protected:
    void SetUp() override {
        TestBase::SetUp();
        ensure_shm_lock_file();
        manager = std::make_unique<config_manager>();
        temp_files.clear();
    }

    void TearDown() override {
        // 清理临时文件
        for (const auto& file : temp_files) {
            unlink(file.c_str());
        }
        temp_files.clear();
        manager.reset();
        TestBase::TearDown();
    }

    std::unique_ptr<config_manager> manager;
    std::vector<std::string>        temp_files;
};

// 测试构造函数
TEST_F(config_manager_test, constructor) {
    EXPECT_EQ(manager->get_plugin_dir(), "./plugins");
    EXPECT_EQ(manager->get_thread_count(), 0);
}

// 测试解析命令行 - 基本选项
TEST_F(config_manager_test, parse_command_line_basic) {
    std::vector<std::string> args = {"test", "--help"};
    std::vector<char*>        argv;
    for (auto& arg : args) {
        argv.push_back(const_cast<char*>(arg.c_str()));
    }
    argv.push_back(nullptr);
    int argc = 2;

    EXPECT_TRUE(manager->parse_command_line(argc, argv.data()));
}

// 测试解析命令行 - 配置文件路径
TEST_F(config_manager_test, parse_command_line_config_file) {
    std::vector<std::string> args = {"test", "--config", "/path/to/config.json"};
    std::vector<char*>        argv;
    for (auto& arg : args) {
        argv.push_back(const_cast<char*>(arg.c_str()));
    }
    argv.push_back(nullptr);
    int argc = 3;

    EXPECT_TRUE(manager->parse_command_line(argc, argv.data()));
}

// 测试解析命令行 - 插件目录
TEST_F(config_manager_test, parse_command_line_plugin_dir) {
    auto tmp_dir = mc::filesystem::temp_directory_path();
    auto plugin_dir = (tmp_dir / "plugins").string();
    std::vector<std::string> args = {"test", "--plugin-dir", plugin_dir};
    std::vector<char*>        argv;
    for (auto& arg : args) {
        argv.push_back(const_cast<char*>(arg.c_str()));
    }
    argv.push_back(nullptr);
    int argc = 3;

    EXPECT_TRUE(manager->parse_command_line(argc, argv.data()));
}

// 测试解析命令行 - 线程数
TEST_F(config_manager_test, parse_command_line_threads) {
    std::vector<std::string> args = {"test", "--threads", "4"};
    std::vector<char*>        argv;
    for (auto& arg : args) {
        argv.push_back(const_cast<char*>(arg.c_str()));
    }
    argv.push_back(nullptr);
    int argc = 3;

    EXPECT_TRUE(manager->parse_command_line(argc, argv.data()));
}

// 测试解析命令行 - 非法线程数
TEST_F(config_manager_test, parse_command_line_invalid_threads) {
    std::vector<std::string> args = {"test", "--threads", "invalid"};
    std::vector<char*>        argv;
    for (auto& arg : args) {
        argv.push_back(const_cast<char*>(arg.c_str()));
    }
    argv.push_back(nullptr);
    int argc = 3;

    EXPECT_FALSE(manager->parse_command_line(argc, argv.data()));
    EXPECT_EQ(manager->get_thread_count(), 0u);
}

// 测试解析命令行 - 插件列表
TEST_F(config_manager_test, parse_command_line_plugins) {
    std::vector<std::string> args = {"test", "--plugin", "plugin1", "--plugin", "plugin2"};
    std::vector<char*>        argv;
    for (auto& arg : args) {
        argv.push_back(const_cast<char*>(arg.c_str()));
    }
    argv.push_back(nullptr);
    int argc = 5;

    EXPECT_TRUE(manager->parse_command_line(argc, argv.data()));
    auto plugins = manager->get_plugin_names();
    EXPECT_EQ(plugins.size(), 2);
}

// 测试添加配置 - Application配置
TEST_F(config_manager_test, add_config_application) {
    config::app_config app;
    auto tmp_dir = mc::filesystem::temp_directory_path();
    auto plugin_dir = (tmp_dir / "plugins").string();

    app.api_version = "v1";
    app.kind        = "Application";
    app.meta.name   = "test_app";
    app.plugin_dir  = plugin_dir;
    app.plugins     = {"plugin1", "plugin2"};
    app.threads     = 4;

    variant v;
    to_variant(app, v);
    EXPECT_TRUE(manager->add_config(v));

    auto result = manager->get_config<config::app_config>("test_app");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->meta.name, "test_app");
    EXPECT_EQ(result->plugin_dir, plugin_dir);
}

// 测试添加配置 - Service配置
TEST_F(config_manager_test, add_config_service) {
    config::service_config service;
    service.api_version  = "v1";
    service.kind         = "Service";
    service.meta.name    = "test_service";
    service.type         = "test_type";
    service.dependencies = {"dep1", "dep2"};

    variant v;
    to_variant(service, v);
    EXPECT_TRUE(manager->add_config(v));

    auto result = manager->get_config<config::service_config>("test_service");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->meta.name, "test_service");
    EXPECT_EQ(result->type, "test_type");
}

// 测试添加配置 - Supervisor配置
TEST_F(config_manager_test, add_config_supervisor) {
    config::supervisor_config supervisor;
    supervisor.api_version  = "v1";
    supervisor.kind         = "Supervisor";
    supervisor.meta.name    = "test_supervisor";
    supervisor.strategy     = config::supervisor_strategy::one_for_one;
    supervisor.max_restarts = 3;
    supervisor.services     = {"service1", "service2"};

    variant v;
    to_variant(supervisor, v);
    EXPECT_TRUE(manager->add_config(v));

    auto result = manager->get_config<config::supervisor_config>("test_supervisor");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->meta.name, "test_supervisor");
}

// 测试添加配置 - Plugin配置
TEST_F(config_manager_test, add_config_plugin) {
    config::plugin_config plugin;
    plugin.api_version = "v1";
    plugin.kind        = "Plugin";
    plugin.meta.name   = "test_plugin";
    plugin.version     = "1.0.0";

    variant v;
    to_variant(plugin, v);
    EXPECT_TRUE(manager->add_config(v));

    auto result = manager->get_config<config::plugin_config>("test_plugin");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->meta.name, "test_plugin");
}

// 测试获取配置 - 不存在的配置
TEST_F(config_manager_test, get_config_not_found) {
    auto result = manager->get_config<config::app_config>("nonexistent");
    EXPECT_FALSE(result.has_value());
}

// 测试获取所有配置
TEST_F(config_manager_test, get_configs) {
    config::service_config service1;
    service1.api_version = "v1";
    service1.kind        = "Service";
    service1.meta.name   = "service1";
    service1.type        = "test";

    config::service_config service2;
    service2.api_version = "v1";
    service2.kind        = "Service";
    service2.meta.name   = "service2";
    service2.type        = "test";

    variant v1, v2;
    to_variant(service1, v1);
    to_variant(service2, v2);

    manager->add_config(v1);
    manager->add_config(v2);

    auto configs = manager->get_configs<config::service_config>();
    EXPECT_EQ(configs.size(), 2);
}

// 测试加载配置文件 - JSON格式
TEST_F(config_manager_test, load_config_file_json) {
    auto tmp_dir = mc::filesystem::temp_directory_path();
    auto plugin_dir = (tmp_dir / "plugins").string();
    std::string json_content = R"({
        "api_version": "v1",
        "kind": "Application",
        "meta": {
            "name": "test_app"
        },
        "plugin_dir": ")" + plugin_dir + R"(",
        "plugins": ["plugin1"],
        "threads": 4
    })";

    std::string file_path = create_temp_json_file(json_content);
    ASSERT_FALSE(file_path.empty());
    temp_files.push_back(file_path);

    EXPECT_TRUE(manager->load_config_file(file_path));

    auto result = manager->get_config<config::app_config>("test_app");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->meta.name, "test_app");
}

// 测试加载配置文件 - 数组格式
TEST_F(config_manager_test, load_config_file_array) {
    std::string json_content = R"([
        {
            "api_version": "v1",
            "kind": "Service",
            "meta": {"name": "service1"},
            "type": "test"
        },
        {
            "api_version": "v1",
            "kind": "Service",
            "meta": {"name": "service2"},
            "type": "test"
        }
    ])";

    std::string file_path = create_temp_json_file(json_content);
    ASSERT_FALSE(file_path.empty());
    temp_files.push_back(file_path);

    EXPECT_TRUE(manager->load_config_file(file_path));

    auto configs = manager->get_configs<config::service_config>();
    EXPECT_EQ(configs.size(), 2);
}

// 测试加载配置文件 - 文件不存在
TEST_F(config_manager_test, load_config_file_not_found) {
    EXPECT_FALSE(manager->load_config_file("/nonexistent/file.json"));
}

// 测试加载配置文件 - 无效JSON
TEST_F(config_manager_test, load_config_file_invalid_json) {
    std::string invalid_json = "{ invalid json }";
    std::string file_path    = create_temp_json_file(invalid_json);
    ASSERT_FALSE(file_path.empty());
    temp_files.push_back(file_path);

    EXPECT_FALSE(manager->load_config_file(file_path));
}

// 测试加载配置文件 - 根节点为非法类型
TEST_F(config_manager_test, load_config_file_invalid_root_type) {
    std::string invalid_root = "123";
    std::string file_path    = create_temp_json_file(invalid_root);
    ASSERT_FALSE(file_path.empty());
    temp_files.push_back(file_path);

    EXPECT_FALSE(manager->load_config_file(file_path));
}

// 测试加载配置文件 - 同时包含有效与无效条目
TEST_F(config_manager_test, load_config_file_mixed_entries) {
    std::string json_content = R"([
        {
            "api_version": "v1",
            "kind": "Service",
            "meta": {"name": "service1"},
            "type": "demo"
        },
        "invalid_entry"
    ])";

    std::string file_path = create_temp_json_file(json_content);
    ASSERT_FALSE(file_path.empty());
    temp_files.push_back(file_path);

    EXPECT_TRUE(manager->load_config_file(file_path));
    auto configs = manager->get_configs<config::service_config>();
    ASSERT_EQ(configs.size(), 1);
    EXPECT_EQ(configs.front().meta.name, "service1");
}

// 测试加载配置文件 - Logging 配置
TEST_F(config_manager_test, load_config_file_logging_section) {
    std::string json_content = R"({
        "api_version": "v1",
        "kind": "Logging",
        "meta": {"name": "logging"},
        "appenders": [],
        "loggers": []
    })";

    std::string file_path = create_temp_json_file(json_content);
    ASSERT_FALSE(file_path.empty());
    temp_files.push_back(file_path);

    EXPECT_TRUE(manager->load_config_file(file_path));
    EXPECT_TRUE(manager->get_configs<config::service_config>().empty());
}

// 测试加载配置文件 - TOML 文件仍未实现
TEST_F(config_manager_test, load_config_file_toml_not_supported) {
    std::string file_path = create_temp_file_with_suffix("kind = \"Application\"\n", ".toml");
    ASSERT_FALSE(file_path.empty());
    temp_files.push_back(file_path);

    EXPECT_FALSE(manager->load_config_file(file_path));
}

// 测试添加无效配置：无效配置会被跳过，add_config 返回 true（设计如此）
TEST_F(config_manager_test, add_config_invalid) {
    variant invalid_config = dict{{"invalid", "config"}};
    // 注意：当前实现会跳过无效配置但返回 true
    // 这可能是设计上的选择，让我们验证配置没有被添加
    bool result = manager->add_config(invalid_config);
    // 验证无效配置没有被添加到配置列表
    auto configs = manager->get_configs<config::app_config>();
    EXPECT_EQ(configs.size(), 0);
}

// 测试添加配置 - 非对象节点被忽略
TEST_F(config_manager_test, add_config_non_object_variant) {
    variant invalid_value = 123;
    bool    result        = manager->add_config(invalid_value);
    EXPECT_TRUE(result);
    auto configs = manager->get_configs<config::service_config>();
    EXPECT_TRUE(configs.empty());
}

// 测试添加配置 - 未知类型
TEST_F(config_manager_test, add_config_unknown_kind) {
    variant config_variant = dict{{"api_version", "v1"},
                                   {"kind", "Unknown"},
                                   {"meta", dict{{"name", "mystery"}}}};
    bool result = manager->add_config(config_variant);
    EXPECT_TRUE(result);
    auto configs = manager->get_configs<config::service_config>();
    EXPECT_TRUE(configs.empty());
}

// 测试获取插件目录
TEST_F(config_manager_test, get_plugin_dir) {
    EXPECT_EQ(manager->get_plugin_dir(), "./plugins");
}

// 测试获取线程数
TEST_F(config_manager_test, get_thread_count) {
    EXPECT_EQ(manager->get_thread_count(), 0);
}

// 测试获取插件名称列表 - 空列表
TEST_F(config_manager_test, get_plugin_names_empty) {
    auto plugins = manager->get_plugin_names();
    EXPECT_TRUE(plugins.empty());
}

// 测试JSON配置加载器
TEST_F(config_manager_test, json_config_loader) {
    json_config_loader loader;
    auto               extensions = loader.supported_extensions();
    EXPECT_EQ(extensions.size(), 1);
    EXPECT_EQ(extensions[0], ".json");
}

// 测试TOML配置加载器
TEST_F(config_manager_test, toml_config_loader) {
    toml_config_loader loader;
    auto               extensions = loader.supported_extensions();
    EXPECT_EQ(extensions.size(), 1);
    EXPECT_EQ(extensions[0], ".toml");

    // TOML加载器应该抛出not_implemented异常
    auto tmp_dir = mc::filesystem::temp_directory_path();
    auto test_file = (tmp_dir / "test.toml").string();
    EXPECT_THROW(loader.load(test_file), mc::not_implemented_exception);
}

// 注意：validate_config 是私有方法，不能直接测试。
// 这些测试用例通过 add_config 来间接测试验证逻辑。
// validate_config 的功能会在 add_config 过程中被调用，相关测试已在上面覆盖。

// 测试 json_config_loader - 文件不存在
TEST_F(config_manager_test, json_config_loader_file_not_found) {
    json_config_loader loader;

    EXPECT_THROW(loader.load("/nonexistent/file.json"), mc::file_not_found_exception);
}

// 测试 json_config_loader - 文件打开失败
TEST_F(config_manager_test, json_config_loader_file_open_fails) {
    json_config_loader loader;

    // 尝试加载一个权限不足的文件（如果可能）
    // 或者创建一个无法打开的文件路径
    // 由于难以模拟文件打开失败，我们至少测试文件不存在的情况
    EXPECT_THROW(loader.load("/proc/self/mem/invalid_path"), mc::file_not_found_exception);
}

// 测试 json_config_loader - JSON 解析错误
TEST_F(config_manager_test, json_config_loader_parse_error) {
    json_config_loader loader;

    // 创建一个包含无效 JSON 的文件
    std::string invalid_json = "{ invalid json syntax }";
    std::string file_path    = create_temp_json_file(invalid_json);
    ASSERT_FALSE(file_path.empty());
    temp_files.push_back(file_path);

    EXPECT_THROW(loader.load(file_path), mc::parse_error_exception);
}

// 测试 json_config_loader - 有效的 JSON
TEST_F(config_manager_test, json_config_loader_valid_json) {
    json_config_loader loader;

    std::string valid_json = R"({"key": "value", "number": 123})";
    std::string file_path  = create_temp_json_file(valid_json);
    ASSERT_FALSE(file_path.empty());
    temp_files.push_back(file_path);

    variant result = loader.load(file_path);
    EXPECT_TRUE(result.is_dict());

    dict d = result.as<dict>();
    EXPECT_EQ(d["key"].as<std::string>(), "value");
    EXPECT_EQ(d["number"].as<int>(), 123);
}
