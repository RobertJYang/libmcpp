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

#include <memory>
#include <string>

using namespace mc::log;

class console_appender_test : public mc::test::TestBase {
protected:
    void SetUp() override {
        mc::test::TestBase::SetUp();
        mc::log::default_logger().set_level(mc::log::level::off);

        // 创建控制台追加器
        m_appender = std::make_shared<console_appender>();
    }

    void TearDown() override {
        m_appender.reset();
        mc::test::TestBase::TearDown();
    }

    // 创建一个测试消息
    message create_test_message(level lvl, const std::string& msg) {
        mc::log::context ctx("test_file.cpp", "test_function", 123);
        return message(lvl, msg, ctx);
    }

    // 创建一个格式化测试消息
    message create_format_message(level lvl, const std::string& fmt, const mc::mutable_dict& args) {
        mc::log::context ctx("test_file.cpp", "test_function", 123);
        return message(lvl, ctx, fmt, args);
    }

    std::shared_ptr<console_appender> m_appender;
};

// 测试默认构造函数
TEST_F(console_appender_test, DefaultConstructor) {
    ASSERT_NE(m_appender, nullptr);
}

// 测试配置构造函数
TEST_F(console_appender_test, ConfigConstructor) {
    console_appender::config cfg;
    cfg.stream    = console_appender::stream_type::std_error;
    cfg.use_color = true;

    auto appender = std::make_shared<console_appender>();
    ASSERT_NE(appender, nullptr);
}

// 测试variant构造函数
TEST_F(console_appender_test, VariantConstructor) {
    mc::mutable_dict dict;
    dict["stream"]    = "std_error";
    dict["use_color"] = true;

    auto appender = std::make_shared<console_appender>();
    ASSERT_NE(appender, nullptr);
}

// 测试初始化函数
TEST_F(console_appender_test, Init) {
    mc::mutable_dict dict;
    dict["stream"]    = "std_error";
    dict["use_color"] = true;

    bool result = m_appender->init(dict);
    ASSERT_TRUE(result);
}

// 测试追加函数 - 基本消息
TEST_F(console_appender_test, AppendBasicMessage) {
    // 这个测试只是确保append不会崩溃
    auto msg = create_test_message(level::info, "这是一条测试消息");
    ASSERT_NO_THROW(m_appender->append(msg));
}

// 测试追加函数 - 格式化消息
TEST_F(console_appender_test, AppendFormattedMessage) {
    // 这个测试只是确保append不会崩溃
    mc::mutable_dict args;
    args["name"]  = "测试";
    args["value"] = 42;

    auto msg = create_test_message(level::info, "名称: 测试, 值: 42");
    ASSERT_NO_THROW(m_appender->append(msg));
}

// 测试不同日志级别
TEST_F(console_appender_test, DifferentLogLevels) {
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
TEST_F(console_appender_test, FactoryCreate) {
    auto appender = appender_factory::instance().create_by_type<console_appender>("console");
    ASSERT_NE(appender, nullptr);
}

// 测试创建并命名
TEST_F(console_appender_test, FactoryCreateNamed) {
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
TEST_F(console_appender_test, GetOrCreateAppender) {
    mc::dict config;

    // 首次调用应该创建新的appender
    auto appender1 =
        appender_factory::instance().get_or_create_appender("test_console2", "console", config);
    ASSERT_NE(appender1, nullptr);
    EXPECT_EQ(appender1->get_name(), "test_console2");

    // 再次调用应该返回已有的appender
    auto appender2 =
        appender_factory::instance().get_or_create_appender("test_console2", "console", config);
    ASSERT_NE(appender2, nullptr);
    EXPECT_EQ(appender2, appender1);
}

// 手动测试 - 这个测试会输出各种颜色的消息
TEST_F(console_appender_test, DISABLED_ManualColorTest) {
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
