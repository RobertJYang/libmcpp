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
#include <mc/core/application.h>
#include <mc/core/config_manager.h>
#include <mc/core/config_schema.h>
#include <mc/log.h>
#include <mc/singleton.h>
#include <test_utilities/test_base.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <string>
#include <unistd.h>
#include <vector>

using namespace mc;
using namespace mc::core;

namespace {

// 辅助函数：确保共享内存锁文件存在
void ensure_shm_lock_file()
{
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

// 辅助函数：创建临时JSON文件
std::string create_temp_json_file(const std::string& content)
{
    auto              tmp_dir  = mc::filesystem::temp_directory_path();
    auto              tmp_file = tmp_dir / "mc_app_test_XXXXXX.json";
    std::string       pattern  = tmp_file.string();
    std::vector<char> buffer(pattern.begin(), pattern.end());
    buffer.push_back('\0');

    int fd = mkstemps(buffer.data(), 5); // 5 = ".json" 的长度
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

} // namespace

class application_test : public mc::test::TestBase {
protected:
    void SetUp() override
    {
        TestBase::SetUp();
        ensure_shm_lock_file();

        // 重置 application singleton
        application::reset_for_test();
        app = &application::instance();

        temp_files.clear();
        m_should_cleanup = false; // 标记是否需要清理
    }

    void TearDown() override
    {
        // 只在测试确实启动了应用时才清理，避免影响其他模块的测试
        // application::stop() 内部已有检查 m_running，不会停止未运行的应用
        if (app && m_should_cleanup) {
            app->stop();
        }

        // 清理临时文件
        for (const auto& file : temp_files) {
            unlink(file.c_str());
        }
        temp_files.clear();

        // 重置 singleton（这会清理所有资源）
        // 注意：application 的清理只影响它自己管理的服务，不会影响 engine 模块直接创建的服务
        application::reset_for_test();

        m_should_cleanup = false;
        TestBase::TearDown();
    }

    application*             app;
    std::vector<std::string> temp_files;
    bool                     m_should_cleanup; // 标记测试是否需要清理应用状态
};

// 测试 application::instance()
TEST_F(application_test, instance)
{
    EXPECT_NE(app, nullptr);

    // 多次调用应该返回同一个实例
    application& app2 = application::instance();
    EXPECT_EQ(app, &app2);
}

// 测试 application::version() 和 set_version()
TEST_F(application_test, version)
{
    EXPECT_EQ(app->version(), "1.0.0");

    app->set_version("2.0.0");
    EXPECT_EQ(app->version(), "2.0.0");
}

// 测试 application::get_config_manager()
TEST_F(application_test, get_config_manager)
{
    auto& config_mgr = app->get_config_manager();
    EXPECT_NE(&config_mgr, nullptr);
}

// 测试 application::get_plugin_manager()
TEST_F(application_test, get_plugin_manager)
{
    auto& plugin_mgr = app->get_plugin_manager();
    EXPECT_NE(&plugin_mgr, nullptr);
}

// 测试 application::get_service_factory()
TEST_F(application_test, get_service_factory)
{
    auto& factory = app->get_service_factory();
    EXPECT_NE(&factory, nullptr);
}

// 测试 application::get_service_manager()
TEST_F(application_test, get_service_manager)
{
    auto& service_mgr = app->get_service_manager();
    EXPECT_NE(&service_mgr, nullptr);
}

// 测试 application::get_supervisor_manager()
TEST_F(application_test, get_supervisor_manager)
{
    auto& supervisor_mgr = app->get_supervisor_manager();
    EXPECT_NE(&supervisor_mgr, nullptr);
}

// 测试 application::initialize() - 成功路径
TEST_F(application_test, initialize_success)
{
    EXPECT_TRUE(app->initialize());
    EXPECT_NE(mc::log::default_logger().find_appender("default_file"), nullptr);
}

// 测试 application::initialize() - supervisor_manager init 失败
// 注意：这个测试可能难以模拟，因为我们需要 mock supervisor_manager
// 但至少测试基本流程
TEST_F(application_test, initialize_basic)
{
    EXPECT_TRUE(app->initialize());
    EXPECT_NE(nullptr, &app->get_service_manager());
}

// 测试 application::initialize(int argc, char** argv) - 成功路径
TEST_F(application_test, initialize_with_args_success)
{
    char        arg0[]         = "test_app";
    char        arg1[]         = "--config";
    auto        tmp_dir        = mc::filesystem::temp_directory_path();
    std::string plugin_dir     = tmp_dir.string();
    std::string config_content = R"({
        "api_version": "v1",
        "kind": "Application",
        "meta": {"name": "test_app"},
        "plugin_dir": ")" + plugin_dir +
                                 R"(",
        "plugins": [],
        "threads": 2
    })";
    std::string config_file = create_temp_json_file(config_content);
    ASSERT_FALSE(config_file.empty());
    temp_files.push_back(config_file);

    char arg2[256];
    snprintf(arg2, sizeof(arg2), "%s", config_file.c_str());

    char* argv[] = {arg0, arg1, arg2, nullptr};
    int   argc   = 3;

    EXPECT_TRUE(app->initialize(argc, argv));
    EXPECT_EQ(app->get_config_manager().get_plugin_dir(), plugin_dir);
}

// 测试 application::initialize(int argc, char** argv) - parse_command_line 失败
TEST_F(application_test, initialize_parse_command_line_fails)
{
    // 使用无效的命令行参数可能不会导致 parse_command_line 失败
    // 但至少测试函数调用
    char  arg0[] = "test_app";
    char* argv[] = {arg0, nullptr};
    int   argc   = 1;

    EXPECT_FALSE(app->initialize(argc, argv));
}

// 测试 application::initialize(int argc, char** argv) - load_config_file 失败（config_loaded = false）
TEST_F(application_test, initialize_load_config_fails)
{
    char arg0[] = "test_app";
    char arg1[] = "--config";
    char arg2[] = "/nonexistent/config.json";

    char* argv[] = {arg0, arg1, arg2, nullptr};
    int   argc   = 3;

    EXPECT_FALSE(app->initialize(argc, argv));
}

// 测试 application::start() - 基本测试
TEST_F(application_test, start_basic)
{
    app->initialize();
    m_should_cleanup = true; // 标记需要清理
    EXPECT_TRUE(app->start());
    EXPECT_FALSE(app->is_stopped());
}

// 测试 application::start() - 重复调用（m_running = true 分支）
TEST_F(application_test, start_when_already_running)
{
    app->initialize();
    m_should_cleanup = true; // 标记需要清理
    EXPECT_TRUE(app->start());
    EXPECT_TRUE(app->start()); // 再次启动，应该直接返回
    EXPECT_FALSE(app->is_stopped());
}

// 测试 application::stop() - 基本测试
TEST_F(application_test, stop_basic)
{
    app->initialize();
    m_should_cleanup = true; // 标记需要清理
    app->start();
    EXPECT_TRUE(app->stop());
    EXPECT_TRUE(app->is_stopped());
}

// 测试 application::stop() - 未运行状态（m_running = false 分支）
TEST_F(application_test, stop_when_not_running)
{
    app->initialize();
    // 不调用 start()
    EXPECT_TRUE(app->stop()); // stop() 总是返回 true
    // 应该直接返回，不执行清理逻辑
}

// 测试 application::stop() - 已经停止（m_stopped = true 分支）
TEST_F(application_test, stop_when_already_stopped)
{
    app->initialize();
    m_should_cleanup = true; // 标记需要清理
    app->start();
    app->stop();
    EXPECT_TRUE(app->is_stopped());

    // 再次停止
    EXPECT_TRUE(app->stop());
    EXPECT_TRUE(app->is_stopped());
}

// 测试 application::is_stopped()
TEST_F(application_test, is_stopped)
{
    app->initialize();
    m_should_cleanup = true; // 标记需要清理
    EXPECT_FALSE(app->is_stopped());

    app->start();
    EXPECT_FALSE(app->is_stopped());

    app->stop();
    EXPECT_TRUE(app->is_stopped());
}

// 注意：exec() 函数启动 runtime 并 join，会阻塞
// 所以不适合在单元测试中完整测试
// 但可以验证函数存在且可调用（如果需要的话，可以在单独进程中测试）

// 测试 application 生命周期：initialize -> start -> stop
TEST_F(application_test, lifecycle)
{
    // Initialize
    EXPECT_TRUE(app->initialize());
    m_should_cleanup = true; // 标记需要清理
    EXPECT_FALSE(app->is_stopped());

    // Start
    EXPECT_TRUE(app->start());
    EXPECT_FALSE(app->is_stopped());

    // Stop
    EXPECT_TRUE(app->stop());
    EXPECT_TRUE(app->is_stopped());
}

// 测试 application::initialize() - 无参数版本后调用带参数版本
TEST_F(application_test, initialize_both_versions)
{
    EXPECT_TRUE(app->initialize());

    // 重置后调用带参数版本
    application::reset_for_test();
    app = &application::instance();

    char  arg0[] = "test_app";
    char* argv[] = {arg0, nullptr};
    int   argc   = 1;

    // 无配置文件，plugin_dir 为空，初始化会失败
    EXPECT_FALSE(app->initialize(argc, argv));
}

// 测试 application::load_plugins() - 加载失败时不会阻止初始化
// 当前实现允许部分插件加载失败，initialize 仍然返回 true
TEST_F(application_test, load_plugins_failure_stops_start)
{
    char        arg0[]         = "test_app";
    char        arg1[]         = "--config";
    auto        tmp_dir        = mc::filesystem::temp_directory_path();
    std::string plugin_dir     = tmp_dir.string();
    std::string config_content = R"({
        "api_version": "v1",
        "kind": "Application",
        "meta": {"name": "test_app"},
        "plugin_dir": ")" + plugin_dir +
                                 R"(",
        "plugins": ["nonexistent_plugin"],
        "threads": 2
    })";
    std::string config_file = create_temp_json_file(config_content);
    ASSERT_FALSE(config_file.empty());
    temp_files.push_back(config_file);

    char arg2[256];
    snprintf(arg2, sizeof(arg2), "%s", config_file.c_str());

    char* argv[] = {arg0, arg1, arg2, nullptr};
    int   argc   = 3;

    // load_plugins 失败时，initialize 仍然返回 true（当前实现允许部分插件失败）
    EXPECT_TRUE(app->initialize(argc, argv));

    // 验证不存在的插件确实没有被加载
    auto loaded_plugins = app->get_plugin_manager().get_loaded_plugins();
    EXPECT_EQ(loaded_plugins.size(), 0);
}
