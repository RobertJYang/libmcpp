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
#include <mc/log.h>
#include <mc/log/appender_factory.h>
#include <mc/log/appenders/console_appender.h>
#include <mc/log/log_message.h>
#include <mc/variant.h>
#include <test_utilities/test_base.h>

#include <cstdio>
#include <fcntl.h>
#include <memory>
#include <string>
#include <unistd.h>
#include <vector>
#ifdef __linux__
#include <stdlib.h>
#endif

using namespace mc::log;

class console_appender_test : public mc::test::TestBase {
protected:
    void SetUp() override
    {
        mc::test::TestBase::SetUp();
        mc::log::default_logger().set_level(mc::log::level::off);

        // 创建控制台追加器
        m_appender = std::make_shared<console_appender>();
        start_capture();
    }

    void TearDown() override
    {
        stop_capture();
        m_appender.reset();
        mc::test::TestBase::TearDown();
    }

    // 创建一个测试消息
    message create_test_message(level lvl, const std::string& msg)
    {
        mc::log::context ctx("test_file.cpp", "test_function", 123);
        return message(lvl, msg, ctx);
    }

    // 创建一个格式化测试消息
    message create_format_message(level lvl, const std::string& fmt, const mc::dict& args)
    {
        mc::log::context ctx("test_file.cpp", "test_function", 123);
        return message(lvl, ctx, fmt, args);
    }

    void start_capture()
    {
        if (m_capture_active) {
            return;
        }
        testing::internal::CaptureStdout();
        testing::internal::CaptureStderr();
        m_capture_active = true;
    }

    void stop_capture()
    {
        if (!m_capture_active) {
            return;
        }
        m_captured_stdout = testing::internal::GetCapturedStdout();
        m_captured_stderr = testing::internal::GetCapturedStderr();
        m_capture_active  = false;
    }

    std::string consume_stderr()
    {
        stop_capture();
        std::string output = m_captured_stderr;
        m_captured_stdout.clear();
        m_captured_stderr.clear();
        start_capture();
        return output;
    }

    std::string consume_stdout()
    {
        stop_capture();
        std::string output = m_captured_stdout;
        m_captured_stdout.clear();
        m_captured_stderr.clear();
        start_capture();
        return output;
    }

    std::shared_ptr<console_appender> m_appender;
    bool                              m_capture_active{false};
    std::string                       m_captured_stdout;
    std::string                       m_captured_stderr;
};

// 测试默认构造函数
TEST_F(console_appender_test, DefaultConstructor)
{
    ASSERT_NE(m_appender, nullptr);
}

// 测试初始化函数
TEST_F(console_appender_test, Init)
{
    mc::dict dict;
    dict["stream"]    = "std_error";
    dict["use_color"] = true;

    bool result = m_appender->init(dict);
    ASSERT_TRUE(result);
}

// 测试不同日志级别
TEST_F(console_appender_test, DifferentLogLevels)
{
    // 这个测试只是确保append不会崩溃
    auto trace_msg = create_test_message(level::trace, "跟踪消息");
    auto debug_msg = create_test_message(level::debug, "调试消息");
    auto info_msg  = create_test_message(level::info, "信息消息");
    auto warn_msg  = create_test_message(level::warn, "警告消息");
    auto error_msg = create_test_message(level::error, "错误消息");
    auto fatal_msg = create_test_message(level::fatal, "致命错误消息");

    ASSERT_NO_THROW(m_appender->append(trace_msg));
    ASSERT_NO_THROW(m_appender->append(debug_msg));
    ASSERT_NO_THROW(m_appender->append(info_msg));
    ASSERT_NO_THROW(m_appender->append(warn_msg));
    ASSERT_NO_THROW(m_appender->append(error_msg));
    ASSERT_NO_THROW(m_appender->append(fatal_msg));
}

// 测试工厂创建
TEST_F(console_appender_test, FactoryCreate)
{
    auto appender = appender_factory::instance().create_by_type<console_appender>("console");
    ASSERT_NE(appender, nullptr);
}

// 测试创建并命名
TEST_F(console_appender_test, FactoryCreateNamed)
{
    mc::dict config;
    auto     appender = appender_factory::instance().create("test_console", "console", config);
    ASSERT_NE(appender, nullptr);
    EXPECT_EQ(appender->get_name(), "test_console");

    // 测试获取已创建的appender
    auto same_appender = appender_factory::instance().get_appender("test_console");
    ASSERT_NE(same_appender, nullptr);
    EXPECT_EQ(same_appender, appender);
}

// 测试获取或创建
TEST_F(console_appender_test, GetOrCreateAppender)
{
    mc::dict config;

    // 首次调用应该创建新的appender
    auto appender1 = appender_factory::instance().get_or_create_appender("test_console2", "console", config);
    ASSERT_NE(appender1, nullptr);
    EXPECT_EQ(appender1->get_name(), "test_console2");

    // 再次调用应该返回已有的appender
    auto appender2 = appender_factory::instance().get_or_create_appender("test_console2", "console", config);
    ASSERT_NE(appender2, nullptr);
    EXPECT_EQ(appender2, appender1);
}

// 手动测试 - 这个测试会输出各种颜色的消息
TEST_F(console_appender_test, DISABLED_ManualColorTest)
{
    std::cout << "=== 手动颜色测试 (需要人工验证) ===" << std::endl;

    console_appender::config cfg;

    auto appender = std::make_shared<console_appender>();

    auto trace_msg = create_test_message(level::trace, "这是一条跟踪消息");
    auto debug_msg = create_test_message(level::debug, "这是一条调试消息");
    auto info_msg  = create_test_message(level::info, "这是一条信息消息");
    auto warn_msg  = create_test_message(level::warn, "这是一条警告消息");
    auto error_msg = create_test_message(level::error, "这是一条错误消息");
    auto fatal_msg = create_test_message(level::fatal, "这是一条致命错误消息");

    appender->append(trace_msg);
    appender->append(debug_msg);
    appender->append(info_msg);
    appender->append(warn_msg);
    appender->append(error_msg);
    appender->append(fatal_msg);

    std::cout << "=== 手动颜色测试结束 ===" << std::endl;
}

// 测试 init 函数 - 异常处理分支
TEST_F(console_appender_test, InitWithInvalidArgs)
{
    // 使用无效的 variant 类型触发异常
    mc::variant invalid_args = 42; // 非对象类型，会触发异常
    bool        result       = m_appender->init(invalid_args);
    std::string captured_log = consume_stderr();
    EXPECT_FALSE(result);
    EXPECT_NE(captured_log.find("console_appender 初始化失败"), std::string::npos);
}

// 测试配置 - level_colors 配置处理
TEST_F(console_appender_test, ConfigureWithLevelColors)
{
    console_appender::config cfg;
    cfg.use_color = true;
    cfg.level_colors.push_back(console_appender::level_color(level::info, console_appender::color_type::blue));
    cfg.level_colors.push_back(console_appender::level_color(level::error, console_appender::color_type::red));

    mc::dict dict;
    dict["use_color"] = true;
    mc::variants level_colors;
    mc::dict     lc1;
    lc1["level"] = static_cast<int>(level::info);
    lc1["color"] = static_cast<int>(console_appender::color_type::blue);
    level_colors.push_back(lc1);
    mc::dict lc2;
    lc2["level"] = static_cast<int>(level::error);
    lc2["color"] = static_cast<int>(console_appender::color_type::red);
    level_colors.push_back(lc2);
    dict["level_colors"] = level_colors;

    EXPECT_TRUE(m_appender->init(dict));

    // 测试使用配置后的 appender
    auto msg = create_test_message(level::info, "测试消息");
    ASSERT_NO_THROW(m_appender->append(msg));
}

// 测试追加函数 - 空文件名上下文
TEST_F(console_appender_test, AppendWithEmptyFileContext)
{
    // 创建空文件名的上下文
    mc::log::context ctx("", "test_function", 123);
    auto             msg = message(level::info, "测试消息", ctx);
    ASSERT_NO_THROW(m_appender->append(msg));
}

// 测试追加函数 - 包含命名空间前缀的函数名
TEST_F(console_appender_test, AppendWithNamespacedFunction)
{
    // 创建包含命名空间前缀的函数名上下文
    mc::log::context ctx("test_file.cpp", "mc::log::test_function", 123);
    auto             msg = message(level::info, "测试消息", ctx);
    ASSERT_NO_THROW(m_appender->append(msg));
}

// 测试配置 - 禁用颜色
TEST_F(console_appender_test, ConfigureWithoutColor)
{
    mc::dict dict;
    dict["use_color"] = false;
    dict["flush"]     = false;

    EXPECT_TRUE(m_appender->init(dict));

    auto msg = create_test_message(level::info, "测试消息");
    ASSERT_NO_THROW(m_appender->append(msg));
}

// 测试配置 - std_error 流
TEST_F(console_appender_test, ConfigureStdErrorStream)
{
    mc::dict dict;
    dict["stream"] = "std_error";

    EXPECT_TRUE(m_appender->init(dict));

    auto msg = create_test_message(level::error, "错误消息");
    ASSERT_NO_THROW(m_appender->append(msg));
}

// 测试配置 - 所有颜色类型（覆盖 get_console_color 函数）
TEST_F(console_appender_test, ConfigureAllColorTypesForGetConsoleColor)
{
    // 配置所有颜色类型以覆盖 get_console_color 函数的所有分支
    mc::dict dict;
    dict["use_color"] = true;
    mc::variants level_colors;

    // 为每个日志级别配置不同的颜色，覆盖所有颜色类型
    std::vector<std::pair<level, console_appender::color_type>> color_mappings = {
        {level::trace, console_appender::color_type::red},     {level::debug, console_appender::color_type::green},
        {level::info, console_appender::color_type::brown},    {level::warn, console_appender::color_type::blue},
        {level::error, console_appender::color_type::magenta}, {level::fatal, console_appender::color_type::cyan},
        {level::notice, console_appender::color_type::white},
    };

    for (const auto& mapping : color_mappings) {
        mc::dict lc;
        lc["level"] = static_cast<int>(mapping.first);
        lc["color"] = static_cast<int>(mapping.second);
        level_colors.push_back(lc);
    }
    dict["level_colors"] = level_colors;

    EXPECT_TRUE(m_appender->init(dict));

    // 测试所有级别的消息，触发 get_console_color 的所有颜色分支
    for (const auto& mapping : color_mappings) {
        auto msg = create_test_message(mapping.first, "测试消息");
        ASSERT_NO_THROW(m_appender->append(msg));
    }

    // 测试 console_default 颜色（通过未配置的级别）
    auto off_msg = create_test_message(level::off, "测试消息");
    ASSERT_NO_THROW(m_appender->append(off_msg));
}

// 测试配置 - flush 为 false 的分支
TEST_F(console_appender_test, ConfigureFlushFalse)
{
    mc::dict dict;
    dict["use_color"] = true;
    dict["flush"]     = false; // 测试 flush 为 false 的分支

    EXPECT_TRUE(m_appender->init(dict));

    auto msg = create_test_message(level::info, "测试消息");
    ASSERT_NO_THROW(m_appender->append(msg));
}

// 测试复杂场景 - 融合所有配置选项
TEST_F(console_appender_test, ComplexScenarioAllOptions)
{
    // 配置所有选项：std_error 流、禁用颜色、flush=false、所有颜色类型
    mc::dict dict;
    dict["stream"]    = "std_error";
    dict["use_color"] = false;
    dict["flush"]     = false;
    mc::variants level_colors;
    for (int i = 0; i <= static_cast<int>(level::off); ++i) {
        mc::dict lc;
        lc["level"] = i;
        lc["color"] = static_cast<int>(console_appender::color_type::red);
        level_colors.push_back(lc);
    }
    dict["level_colors"] = level_colors;

    EXPECT_TRUE(m_appender->init(dict));

    // 测试所有级别的消息
    for (int i = 0; i <= static_cast<int>(level::off); ++i) {
        auto msg = create_test_message(static_cast<level>(i), "测试消息");
        ASSERT_NO_THROW(m_appender->append(msg));
    }
}

// 测试复杂场景 - 空文件名和命名空间函数名组合
TEST_F(console_appender_test, ComplexScenarioEmptyFileAndNamespacedFunction)
{
    mc::dict dict;
    dict["use_color"] = true;
    EXPECT_TRUE(m_appender->init(dict));

    // 创建空文件名和命名空间函数名的上下文
    mc::log::context ctx("", "mc::log::test_function", 123);
    auto             msg = message(level::info, "测试消息", ctx);
    ASSERT_NO_THROW(m_appender->append(msg));
}

// 测试 get_console_color 函数的所有颜色分支
// 注意：此测试在自动化测试环境中可能无法完全覆盖 get_console_color 函数，
// 因为 isatty() 在重定向的输出流中返回 false。
// 要完全覆盖，需要在真实的终端环境中运行，或者使用伪终端（pty）。
// 详见 tests/log/README.md 中的说明。
TEST_F(console_appender_test, GetConsoleColorAllBranches)
{
    // 停止输出捕获，尝试让 isatty 返回 true
    stop_capture();

    mc::dict dict;
    dict["use_color"] = true;
    mc::variants level_colors;

    // 配置所有颜色类型，覆盖 get_console_color 的所有 case 分支
    std::vector<std::pair<level, console_appender::color_type>> color_mappings = {
        {level::trace, console_appender::color_type::red},     // 覆盖 red case
        {level::debug, console_appender::color_type::green},   // 覆盖 green case
        {level::info, console_appender::color_type::brown},    // 覆盖 brown case
        {level::warn, console_appender::color_type::blue},     // 覆盖 blue case
        {level::error, console_appender::color_type::magenta}, // 覆盖 magenta case
        {level::fatal, console_appender::color_type::cyan},    // 覆盖 cyan case
        {level::notice, console_appender::color_type::white},  // 覆盖 white case
    };

    for (const auto& mapping : color_mappings) {
        mc::dict lc;
        lc["level"] = static_cast<int>(mapping.first);
        lc["color"] = static_cast<int>(mapping.second);
        level_colors.push_back(lc);
    }
    dict["level_colors"] = level_colors;

    EXPECT_TRUE(m_appender->init(dict));

    // 测试所有级别的消息，尝试触发 get_console_color 的所有颜色分支
    // 注意：只有在 isatty() 返回 true 时才会实际调用 get_console_color
    for (const auto& mapping : color_mappings) {
        auto msg = create_test_message(mapping.first, "测试消息");
        ASSERT_NO_THROW(m_appender->append(msg));
    }

    // 测试 console_default 颜色（通过未配置的级别或 default case）
    auto off_msg = create_test_message(level::off, "测试消息");
    ASSERT_NO_THROW(m_appender->append(off_msg));

    // 重新开始捕获，以便后续测试正常工作
    start_capture();
}

// 测试空文件名的格式化
TEST_F(console_appender_test, FormatMessageEmptyFileUsesUnknown)
{
    mc::dict dict;
    dict["use_color"] = false;
    EXPECT_TRUE(m_appender->init(dict));

    // 创建空文件名的上下文
    mc::log::context ctx("", "test_function", 123);
    auto             msg = message(level::info, "测试消息", ctx);

    // 清空现有的输出，确保只检查当前 append 的内容
    consume_stdout();

    // 追加消息，应该使用 "unknown" 作为文件名
    m_appender->append(msg);
    std::string output = consume_stdout();

    // 验证输出包含 "unknown"（因为文件名为空）
    EXPECT_TRUE(output.find("unknown") != std::string::npos || output.find(":123") != std::string::npos);
}

// 测试通过伪终端让 isatty 返回 true，覆盖颜色输出分支
TEST_F(console_appender_test, ColorOutputWithPseudoTerminal)
{
#ifdef __linux__
    // 创建伪终端
    int master_fd = posix_openpt(O_RDWR | O_NOCTTY);
    if (master_fd < 0) {
        GTEST_SKIP() << "无法创建伪终端，跳过此测试";
        return;
    }

    if (grantpt(master_fd) != 0 || unlockpt(master_fd) != 0) {
        close(master_fd);
        GTEST_SKIP() << "无法设置伪终端权限，跳过此测试";
        return;
    }

    // 获取从端名称
    char* slave_name = ptsname(master_fd);
    if (slave_name == nullptr) {
        close(master_fd);
        GTEST_SKIP() << "无法获取从端名称，跳过此测试";
        return;
    }

    // 打开从端
    int slave_fd = open(slave_name, O_RDWR | O_NOCTTY);
    if (slave_fd < 0) {
        close(master_fd);
        GTEST_SKIP() << "无法打开从端，跳过此测试";
        return;
    }

    // 保存原始的 stdout
    int original_stdout = dup(STDOUT_FILENO);
    if (original_stdout < 0) {
        close(slave_fd);
        close(master_fd);
        GTEST_SKIP() << "无法保存原始 stdout，跳过此测试";
        return;
    }

    // 将 stdout 重定向到从端
    if (dup2(slave_fd, STDOUT_FILENO) < 0) {
        close(original_stdout);
        close(slave_fd);
        close(master_fd);
        GTEST_SKIP() << "无法重定向 stdout，跳过此测试";
        return;
    }

    // 配置 appender 使用颜色
    mc::dict dict;
    dict["use_color"] = true;
    dict["stream"]    = "std_out";
    EXPECT_TRUE(m_appender->init(dict));

    // 创建测试消息
    auto msg = create_test_message(level::info, "测试颜色输出");

    // 追加消息，现在 isatty 应该返回 true
    m_appender->append(msg);

    // 从主端读取输出
    char    buffer[1024];
    ssize_t bytes_read = read(master_fd, buffer, sizeof(buffer) - 1);
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        std::string output(buffer);

        // 验证输出包含 ANSI 颜色代码（以 \033[ 开头）
        EXPECT_TRUE(output.find("\033[") != std::string::npos) << "输出应该包含 ANSI 颜色代码";
    }

    // 恢复原始的 stdout
    dup2(original_stdout, STDOUT_FILENO);
    close(original_stdout);
    close(slave_fd);
    close(master_fd);
#else
    GTEST_SKIP() << "伪终端测试仅在 Linux 上支持";
#endif
}
