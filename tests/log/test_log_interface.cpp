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
#include <mc/log.h>
#include <mc/log/log_level.h>
#include <memory>
#include <string>
#include <vector>

// 测试用的日志追加器，将日志消息存储在内存中
class memory_appender : public mc::log::appender {
public:
    bool init(const mc::variant& args) override
    {
        return true;
    }

    void append(const mc::log::message& msg) override
    {
        m_messages.push_back(msg);
    }

    const std::vector<mc::log::message>& get_messages() const
    {
        return m_messages;
    }

    void clear()
    {
        m_messages.clear();
    }

private:
    std::vector<mc::log::message> m_messages;
};

// 日志接口测试类
class LogInterfaceTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        m_memory_appender = std::make_shared<memory_appender>();
        m_memory_appender->set_name("memory_appender");

        m_test_logger = mc::log::logger("test_interface_logger");
        m_test_logger.set_level(mc::log::level::all);
        m_test_logger.add_appender(m_memory_appender);
    }

    void TearDown() override
    {
        m_test_logger.clear_appenders();
        m_memory_appender->clear();
    }

    bool message_contains(const mc::log::message& msg, const std::string& text)
    {
        return msg.get_message().find(text) != std::string::npos;
    }

    std::shared_ptr<memory_appender> m_memory_appender;
    mc::log::logger                  m_test_logger;
};

// ==================== 调试日志（notice、warn 等）====================

TEST_F(LogInterfaceTest, debug_log_trace)
{
    mc_tlog(m_test_logger, "trace 调试日志");
    ASSERT_FALSE(m_memory_appender->get_messages().empty());
    const auto& msg = m_memory_appender->get_messages().back();
    EXPECT_EQ(msg.get_level(), mc::log::level::trace);
    EXPECT_EQ(msg.get_category(), mc::log::log_category::debug);
    EXPECT_TRUE(message_contains(msg, "trace 调试日志"));
    EXPECT_TRUE(msg.get_limit());
}

TEST_F(LogInterfaceTest, debug_log_debug)
{
    mc_dlog(m_test_logger, "debug 调试日志");
    ASSERT_FALSE(m_memory_appender->get_messages().empty());
    const auto& msg = m_memory_appender->get_messages().back();
    EXPECT_EQ(msg.get_level(), mc::log::level::debug);
    EXPECT_EQ(msg.get_category(), mc::log::log_category::debug);
    EXPECT_TRUE(message_contains(msg, "debug 调试日志"));
}

TEST_F(LogInterfaceTest, debug_log_info)
{
    mc_ilog(m_test_logger, "info 调试日志");
    ASSERT_FALSE(m_memory_appender->get_messages().empty());
    const auto& msg = m_memory_appender->get_messages().back();
    EXPECT_EQ(msg.get_level(), mc::log::level::info);
    EXPECT_TRUE(message_contains(msg, "info 调试日志"));
}

TEST_F(LogInterfaceTest, debug_log_notice)
{
    mc_nlog(m_test_logger, "notice 调试日志");
    ASSERT_FALSE(m_memory_appender->get_messages().empty());
    const auto& msg = m_memory_appender->get_messages().back();
    EXPECT_EQ(msg.get_level(), mc::log::level::notice);
    EXPECT_TRUE(message_contains(msg, "notice 调试日志"));
}

TEST_F(LogInterfaceTest, debug_log_warn)
{
    mc_wlog(m_test_logger, "warn 调试日志");
    ASSERT_FALSE(m_memory_appender->get_messages().empty());
    const auto& msg = m_memory_appender->get_messages().back();
    EXPECT_EQ(msg.get_level(), mc::log::level::warn);
    EXPECT_TRUE(message_contains(msg, "warn 调试日志"));
}

TEST_F(LogInterfaceTest, debug_log_error)
{
    mc_elog(m_test_logger, "error 调试日志");
    ASSERT_FALSE(m_memory_appender->get_messages().empty());
    const auto& msg = m_memory_appender->get_messages().back();
    EXPECT_EQ(msg.get_level(), mc::log::level::error);
    EXPECT_TRUE(message_contains(msg, "error 调试日志"));
}

TEST_F(LogInterfaceTest, debug_log_fatal)
{
    mc_flog(m_test_logger, "fatal 调试日志");
    ASSERT_FALSE(m_memory_appender->get_messages().empty());
    const auto& msg = m_memory_appender->get_messages().back();
    EXPECT_EQ(msg.get_level(), mc::log::level::fatal);
    EXPECT_TRUE(message_contains(msg, "fatal 调试日志"));
}

TEST_F(LogInterfaceTest, debug_log_structured)
{
    mc_ilog(m_test_logger, "用户 ${user} 值 ${val}", ("user", "test")("val", 42));
    ASSERT_FALSE(m_memory_appender->get_messages().empty());
    EXPECT_TRUE(message_contains(m_memory_appender->get_messages().back(), "用户 test 值 42"));
}

// ==================== 不限流调试日志（notice_easy、warn_easy 等）====================

TEST_F(LogInterfaceTest, debug_log_easy_notice)
{
    mc_nlog_easy(m_test_logger, "notice_easy 不限流");
    ASSERT_FALSE(m_memory_appender->get_messages().empty());
    const auto& msg = m_memory_appender->get_messages().back();
    EXPECT_EQ(msg.get_level(), mc::log::level::notice);
    EXPECT_FALSE(msg.get_limit());
    EXPECT_TRUE(message_contains(msg, "notice_easy 不限流"));
}

TEST_F(LogInterfaceTest, debug_log_easy_warn)
{
    mc_wlog_easy(m_test_logger, "warn_easy 不限流");
    ASSERT_FALSE(m_memory_appender->get_messages().empty());
    const auto& msg = m_memory_appender->get_messages().back();
    EXPECT_EQ(msg.get_level(), mc::log::level::warn);
    EXPECT_FALSE(msg.get_limit());
    EXPECT_TRUE(message_contains(msg, "warn_easy 不限流"));
}

TEST_F(LogInterfaceTest, debug_log_easy_all_levels)
{
    mc_tlog_easy(m_test_logger, "trace_easy");
    mc_dlog_easy(m_test_logger, "debug_easy");
    mc_ilog_easy(m_test_logger, "info_easy");
    mc_nlog_easy(m_test_logger, "notice_easy");
    mc_wlog_easy(m_test_logger, "warn_easy");
    mc_elog_easy(m_test_logger, "error_easy");
    mc_flog_easy(m_test_logger, "fatal_easy");

    const auto& messages = m_memory_appender->get_messages();
    ASSERT_EQ(messages.size(), 7);

    const mc::log::level levels[] = {mc::log::level::trace,  mc::log::level::debug, mc::log::level::info,
                                     mc::log::level::notice, mc::log::level::warn,  mc::log::level::error,
                                     mc::log::level::fatal};

    for (size_t i = 0; i < 7; ++i) {
        EXPECT_EQ(messages[i].get_level(), levels[i]);
        EXPECT_FALSE(messages[i].get_limit());
    }
}

// ==================== 南向硬件流日志（hw_stream）====================

TEST_F(LogInterfaceTest, hw_stream_info)
{
    mc_hw_stream_info(m_test_logger, "南向硬件流 info");
    ASSERT_FALSE(m_memory_appender->get_messages().empty());
    const auto& msg = m_memory_appender->get_messages().back();
    EXPECT_EQ(msg.get_category(), mc::log::log_category::hw_stream);
    EXPECT_EQ(msg.get_level(), mc::log::level::info);
    EXPECT_TRUE(message_contains(msg, "南向硬件流 info"));
}

TEST_F(LogInterfaceTest, hw_stream_warn)
{
    mc_hw_stream_warn(m_test_logger, "南向硬件流 warn");
    ASSERT_FALSE(m_memory_appender->get_messages().empty());
    const auto& msg = m_memory_appender->get_messages().back();
    EXPECT_EQ(msg.get_category(), mc::log::log_category::hw_stream);
    EXPECT_EQ(msg.get_level(), mc::log::level::warn);
    EXPECT_TRUE(message_contains(msg, "南向硬件流 warn"));
}

TEST_F(LogInterfaceTest, hw_stream_notice)
{
    mc_hw_stream_notice(m_test_logger, "南向硬件流 notice");
    ASSERT_FALSE(m_memory_appender->get_messages().empty());
    const auto& msg = m_memory_appender->get_messages().back();
    EXPECT_EQ(msg.get_category(), mc::log::log_category::hw_stream);
    EXPECT_EQ(msg.get_level(), mc::log::level::notice);
    EXPECT_TRUE(message_contains(msg, "南向硬件流 notice"));
}

TEST_F(LogInterfaceTest, hw_stream_error)
{
    mc_hw_stream_error(m_test_logger, "南向硬件流 error");
    ASSERT_FALSE(m_memory_appender->get_messages().empty());
    const auto& msg = m_memory_appender->get_messages().back();
    EXPECT_EQ(msg.get_category(), mc::log::log_category::hw_stream);
    EXPECT_EQ(msg.get_level(), mc::log::level::error);
    EXPECT_TRUE(message_contains(msg, "南向硬件流 error"));
}

TEST_F(LogInterfaceTest, hw_stream_structured)
{
    mc_hw_stream_info(m_test_logger, "设备 ${dev} 温度 ${temp}", ("dev", "sensor_1")("temp", 42));
    ASSERT_FALSE(m_memory_appender->get_messages().empty());
    EXPECT_TRUE(message_contains(m_memory_appender->get_messages().back(), "设备 sensor_1 温度 42"));
}

// ==================== 应用框架流日志（mc_stream）====================

TEST_F(LogInterfaceTest, mc_stream_info)
{
    mc_mc_stream_info(m_test_logger, "应用框架流 info");
    ASSERT_FALSE(m_memory_appender->get_messages().empty());
    const auto& msg = m_memory_appender->get_messages().back();
    EXPECT_EQ(msg.get_category(), mc::log::log_category::mc_stream);
    EXPECT_EQ(msg.get_level(), mc::log::level::info);
    EXPECT_TRUE(message_contains(msg, "应用框架流 info"));
}

TEST_F(LogInterfaceTest, mc_stream_warn)
{
    mc_mc_stream_warn(m_test_logger, "应用框架流 warn");
    ASSERT_FALSE(m_memory_appender->get_messages().empty());
    const auto& msg = m_memory_appender->get_messages().back();
    EXPECT_EQ(msg.get_category(), mc::log::log_category::mc_stream);
    EXPECT_EQ(msg.get_level(), mc::log::level::warn);
    EXPECT_TRUE(message_contains(msg, "应用框架流 warn"));
}

TEST_F(LogInterfaceTest, mc_stream_notice)
{
    mc_mc_stream_notice(m_test_logger, "应用框架流 notice");
    ASSERT_FALSE(m_memory_appender->get_messages().empty());
    const auto& msg = m_memory_appender->get_messages().back();
    EXPECT_EQ(msg.get_category(), mc::log::log_category::mc_stream);
    EXPECT_EQ(msg.get_level(), mc::log::level::notice);
    EXPECT_TRUE(message_contains(msg, "应用框架流 notice"));
}

TEST_F(LogInterfaceTest, mc_stream_error)
{
    mc_mc_stream_error(m_test_logger, "应用框架流 error");
    ASSERT_FALSE(m_memory_appender->get_messages().empty());
    const auto& msg = m_memory_appender->get_messages().back();
    EXPECT_EQ(msg.get_category(), mc::log::log_category::mc_stream);
    EXPECT_EQ(msg.get_level(), mc::log::level::error);
    EXPECT_TRUE(message_contains(msg, "应用框架流 error"));
}

TEST_F(LogInterfaceTest, mc_stream_structured)
{
    mc_mc_stream_info(m_test_logger, "服务 ${svc} 状态 ${status}", ("svc", "task_service")("status", "running"));
    ASSERT_FALSE(m_memory_appender->get_messages().empty());
    EXPECT_TRUE(message_contains(m_memory_appender->get_messages().back(), "服务 task_service 状态 running"));
}

// ==================== operation_log ====================

TEST_F(LogInterfaceTest, operation_log_basic)
{
    mc_operation_log(m_test_logger, "操作日志");
    ASSERT_FALSE(m_memory_appender->get_messages().empty());
    const auto& msg = m_memory_appender->get_messages().back();
    EXPECT_EQ(msg.get_category(), mc::log::log_category::operation);
    EXPECT_TRUE(message_contains(msg, "操作日志"));
}

TEST_F(LogInterfaceTest, operation_log_structured)
{
    mc_operation_log(m_test_logger, "用户 ${user} 执行 ${action}", ("user", "admin")("action", "登录"));
    ASSERT_FALSE(m_memory_appender->get_messages().empty());
    EXPECT_TRUE(message_contains(m_memory_appender->get_messages().back(), "用户 admin 执行 登录"));
}

// ==================== 流日志 / operation 不受 logger 级别过滤 ====================

TEST_F(LogInterfaceTest, hw_stream_bypass_level_filter)
{
    m_test_logger.set_level(mc::log::level::error);

    mc_hw_stream_info(m_test_logger, "hw info 应输出");

    ASSERT_EQ(m_memory_appender->get_messages().size(), 1);
    EXPECT_EQ(m_memory_appender->get_messages().back().get_level(), mc::log::level::info);
}

TEST_F(LogInterfaceTest, mc_stream_bypass_level_filter)
{
    m_test_logger.set_level(mc::log::level::error);

    mc_mc_stream_info(m_test_logger, "mc info 应输出");

    ASSERT_EQ(m_memory_appender->get_messages().size(), 1);
    EXPECT_EQ(m_memory_appender->get_messages().back().get_level(), mc::log::level::info);
}

TEST_F(LogInterfaceTest, operation_log_bypass_level_filter)
{
    m_test_logger.set_level(mc::log::level::off);

    mc_operation_log(m_test_logger, "operation 应输出");

    ASSERT_FALSE(m_memory_appender->get_messages().empty());
    EXPECT_EQ(m_memory_appender->get_messages().back().get_category(), mc::log::log_category::operation);
}

// ==================== running_log ====================

TEST_F(LogInterfaceTest, running_log_basic)
{
    mc_running_wlog(m_test_logger, "test running log");
    ASSERT_FALSE(m_memory_appender->get_messages().empty());
    const auto& msg = m_memory_appender->get_messages().back();
    EXPECT_EQ(msg.get_category(), mc::log::log_category::running);
    EXPECT_EQ(msg.get_level(), mc::log::level::warn);
    EXPECT_TRUE(message_contains(msg, "test running log"));
}

TEST_F(LogInterfaceTest, running_log_bypass_level_filter)
{
    m_test_logger.set_level(mc::log::level::off);

    mc_running_wlog(m_test_logger, "running 应输出");

    ASSERT_FALSE(m_memory_appender->get_messages().empty());
    EXPECT_EQ(m_memory_appender->get_messages().back().get_category(), mc::log::log_category::running);
}

// ==================== maintenance_log ====================

TEST_F(LogInterfaceTest, maintenance_log_basic)
{
    mc_maintenance_elog(m_test_logger, "SVR-0001111", "test maintenance log");
    ASSERT_FALSE(m_memory_appender->get_messages().empty());
    const auto& msg = m_memory_appender->get_messages().back();
    EXPECT_EQ(msg.get_category(), mc::log::log_category::maintenance);
    EXPECT_EQ(msg.get_level(), mc::log::level::error);
    EXPECT_TRUE(msg.get_args().contains("error_code"));
    EXPECT_EQ(msg.get_args()["error_code"].as<std::string>(), "SVR-0001111");
    EXPECT_TRUE(message_contains(msg, "test maintenance log"));
}

TEST_F(LogInterfaceTest, maintenance_log_bypass_level_filter)
{
    m_test_logger.set_level(mc::log::level::off);

    mc_maintenance_elog(m_test_logger, "", "maintenance 应输出");

    ASSERT_FALSE(m_memory_appender->get_messages().empty());
    EXPECT_EQ(m_memory_appender->get_messages().back().get_category(), mc::log::log_category::maintenance);
}

// ==================== security_log ====================

TEST_F(LogInterfaceTest, security_log_basic)
{
    mc_security_log(m_test_logger, "test security log");
    ASSERT_FALSE(m_memory_appender->get_messages().empty());
    const auto& msg = m_memory_appender->get_messages().back();
    EXPECT_EQ(msg.get_category(), mc::log::log_category::security);
    EXPECT_EQ(msg.get_level(), mc::log::level::info);
    EXPECT_TRUE(message_contains(msg, "test security log"));
}

TEST_F(LogInterfaceTest, security_log_bypass_level_filter)
{
    m_test_logger.set_level(mc::log::level::off);

    mc_security_log(m_test_logger, "security 应输出");

    ASSERT_FALSE(m_memory_appender->get_messages().empty());
    EXPECT_EQ(m_memory_appender->get_messages().back().get_category(), mc::log::log_category::security);
}
