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
#include <mc/log/log.h>
#include <mc/log/log_level.h>
#include <memory>
#include <string>
#include <vector>

// 测试用的日志追加器，将日志消息存储在内存中
class memory_appender : public mc::log::appender {
public:
    bool init(const mc::variant& args) override {
        return true;
    }

    void append(const mc::log::message& msg) override {
        m_messages.push_back(msg);
    }

    const std::vector<mc::log::message>& get_messages() const {
        return m_messages;
    }

    void clear() {
        m_messages.clear();
    }

private:
    std::vector<mc::log::message> m_messages;
};

// 日志接口测试类
class LogInterfaceTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_memory_appender = std::make_shared<memory_appender>();
        m_memory_appender->set_name("memory_appender");

        m_test_logger = mc::log::logger("test_interface_logger");
        m_test_logger.set_level(mc::log::level::all);
        m_test_logger.add_appender(m_memory_appender);
    }

    void TearDown() override {
        m_test_logger.clear_appenders();
        m_memory_appender->clear();
    }

    bool message_contains(const mc::log::message& msg, const mc::string& text) {
        return mc::string_view(msg.get_message()).find(text) != mc::string_view::npos;
    }

    std::shared_ptr<memory_appender> m_memory_appender;
    mc::log::logger                  m_test_logger;
};

// ==================== 调试日志（notice、warn 等）====================

TEST_F(LogInterfaceTest, debug_log_trace) {
    mc_tlog(m_test_logger, "trace 调试日志");
    ASSERT_FALSE(m_memory_appender->get_messages().empty());
    const auto& msg = m_memory_appender->get_messages().back();
    EXPECT_EQ(msg.get_level(), mc::log::level::trace);
    EXPECT_EQ(msg.get_category(), mc::log::log_category::debug);
    EXPECT_TRUE(message_contains(msg, "trace 调试日志"));
    EXPECT_TRUE(msg.get_limit());
}

TEST_F(LogInterfaceTest, debug_log_debug) {
    mc_dlog(m_test_logger, "debug 调试日志");
    ASSERT_FALSE(m_memory_appender->get_messages().empty());
    const auto& msg = m_memory_appender->get_messages().back();
    EXPECT_EQ(msg.get_level(), mc::log::level::debug);
    EXPECT_EQ(msg.get_category(), mc::log::log_category::debug);
    EXPECT_TRUE(message_contains(msg, "debug 调试日志"));
}

TEST_F(LogInterfaceTest, debug_log_info) {
    mc_ilog(m_test_logger, "info 调试日志");
    ASSERT_FALSE(m_memory_appender->get_messages().empty());
    const auto& msg = m_memory_appender->get_messages().back();
    EXPECT_EQ(msg.get_level(), mc::log::level::info);
    EXPECT_TRUE(message_contains(msg, "info 调试日志"));
}

TEST_F(LogInterfaceTest, debug_log_notice) {
    mc_nlog(m_test_logger, "notice 调试日志");
    ASSERT_FALSE(m_memory_appender->get_messages().empty());
    const auto& msg = m_memory_appender->get_messages().back();
    EXPECT_EQ(msg.get_level(), mc::log::level::notice);
    EXPECT_TRUE(message_contains(msg, "notice 调试日志"));
}

TEST_F(LogInterfaceTest, debug_log_warn) {
    mc_wlog(m_test_logger, "warn 调试日志");
    ASSERT_FALSE(m_memory_appender->get_messages().empty());
    const auto& msg = m_memory_appender->get_messages().back();
    EXPECT_EQ(msg.get_level(), mc::log::level::warn);
    EXPECT_TRUE(message_contains(msg, "warn 调试日志"));
}

TEST_F(LogInterfaceTest, debug_log_error) {
    mc_elog(m_test_logger, "error 调试日志");
    ASSERT_FALSE(m_memory_appender->get_messages().empty());
    const auto& msg = m_memory_appender->get_messages().back();
    EXPECT_EQ(msg.get_level(), mc::log::level::error);
    EXPECT_TRUE(message_contains(msg, "error 调试日志"));
}

TEST_F(LogInterfaceTest, debug_log_fatal) {
    mc_flog(m_test_logger, "fatal 调试日志");
    ASSERT_FALSE(m_memory_appender->get_messages().empty());
    const auto& msg = m_memory_appender->get_messages().back();
    EXPECT_EQ(msg.get_level(), mc::log::level::fatal);
    EXPECT_TRUE(message_contains(msg, "fatal 调试日志"));
}

TEST_F(LogInterfaceTest, debug_log_structured) {
    mc_ilog(m_test_logger, "用户 ${user} 值 ${val}", ("user", "test")("val", 42));
    ASSERT_FALSE(m_memory_appender->get_messages().empty());
    EXPECT_TRUE(message_contains(m_memory_appender->get_messages().back(), "用户 test 值 42"));
}
