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
#include <memory>
#include <string>

using namespace mc::log;

class ConsoleAppenderTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建控制台追加器
        m_appender = std::make_shared<console_appender>();
    }

    void TearDown() override {
        m_appender.reset();
    }

    // 创建一个测试消息
    message create_test_message(level lvl, const std::string& msg) {
        context ctx("test_file.cpp", "test_function", 123);
        return message(lvl, msg, ctx);
    }

    // 创建一个格式化测试消息
    message create_format_message(level lvl, const std::string& fmt, const mc::mutable_dict& args) {
        context ctx("test_file.cpp", "test_function", 123);
        return message(lvl, ctx, fmt, args);
    }

    std::shared_ptr<console_appender> m_appender;
};

// 测试默认构造函数
TEST_F(ConsoleAppenderTest, DefaultConstructor) {
    ASSERT_NE(m_appender, nullptr);
}

// 测试配置构造函数
TEST_F(ConsoleAppenderTest, ConfigConstructor) {
    console_appender::config cfg;
    cfg.stream    = console_appender::stream_type::std_error;
    cfg.format    = "{level} - {message}";
    cfg.use_color = true;

    auto appender = std::make_shared<console_appender>();
    ASSERT_NE(appender, nullptr);
}

// 测试variant构造函数
TEST_F(ConsoleAppenderTest, VariantConstructor) {
    mc::mutable_dict dict;
    dict["stream"]    = "std_error";
    dict["format"]    = "{level} - {message}";
    dict["use_color"] = true;

    auto appender = std::make_shared<console_appender>();
    ASSERT_NE(appender, nullptr);
}

// 测试初始化函数
TEST_F(ConsoleAppenderTest, Init) {
    mc::mutable_dict dict;
    dict["stream"]    = "std_error";
    dict["format"]    = "{level} - {message}";
    dict["use_color"] = true;

    bool result = m_appender->init(dict);
    ASSERT_TRUE(result);
}

// 测试追加函数 - 基本消息
TEST_F(ConsoleAppenderTest, AppendBasicMessage) {
    // 这个测试只是确保append不会崩溃
    auto msg = create_test_message(level::info, "这是一条测试消息");
    ASSERT_NO_THROW(m_appender->append(msg));
}

// 测试追加函数 - 格式化消息
TEST_F(ConsoleAppenderTest, AppendFormattedMessage) {
    // 这个测试只是确保append不会崩溃
    mc::mutable_dict args;
    args["name"]  = "测试";
    args["value"] = 42;

    auto msg = create_format_message(level::info, "名称: ${name}, 值: ${value}", args);
    ASSERT_NO_THROW(m_appender->append(msg));
}

// 测试不同日志级别
TEST_F(ConsoleAppenderTest, DifferentLogLevels) {
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

// 测试自定义格式
TEST_F(ConsoleAppenderTest, CustomFormat) {
    console_appender::config cfg;
    cfg.format = "[{level}] {time} - {file}:{line} - {message}";

    auto appender = std::make_shared<console_appender>();
    auto msg      = create_test_message(level::info, "自定义格式测试");

    ASSERT_NO_THROW(appender->append(msg));
}

// 测试详细格式
TEST_F(ConsoleAppenderTest, DetailedFormat) {
    console_appender::config cfg;
    cfg.format = "{detailed}";

    auto appender = std::make_shared<console_appender>();
    auto msg      = create_test_message(level::info, "详细格式测试");

    ASSERT_NO_THROW(appender->append(msg));
}

// 测试工厂创建
TEST_F(ConsoleAppenderTest, FactoryCreate) {
    auto appender = appender_factory::instance().create<console_appender>("console");
    ASSERT_NE(appender, nullptr);
}

// 手动测试 - 这个测试会输出各种颜色的消息
TEST_F(ConsoleAppenderTest, DISABLED_ManualColorTest) {
    std::cout << "=== 手动颜色测试 (需要人工验证) ===" << std::endl;

    console_appender::config cfg;
    cfg.format = "{detailed}";

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
