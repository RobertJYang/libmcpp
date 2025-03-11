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
#include <mc/log/log_manager.h>
#include <mc/log/log_macros.h>
#include <mc/log/console_appender.h>
#include <mc/log/file_log_backend.h>
#include <mc/dict.h>
#include <fstream>
#include <sstream>
#include <string>
#include <memory>
#include <vector>
#include <regex>

// 测试用的日志追加器，将日志消息存储在内存中
class memory_appender : public mc::log::appender {
public:
    memory_appender() : mc::log::appender(mc::log::appender_config("memory")) {}
    
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

// 测试用的日志后端，将日志消息存储在内存中
class memory_backend : public mc::log::log_backend {
public:
    memory_backend() = default;
    
    bool init(const std::string& config) override {
        m_config = config;
        return true;
    }
    
    void write(const mc::log::message& msg) override {
        m_messages.push_back(msg);
    }
    
    void flush() override {}
    
    void close() override {
        m_messages.clear();
    }
    
    std::string name() const override {
        return "memory_backend";
    }
    
    const std::vector<mc::log::message>& get_messages() const {
        return m_messages;
    }
    
    void clear() {
        m_messages.clear();
    }
    
private:
    std::string m_config;
    std::vector<mc::log::message> m_messages;
};

// 日志框架测试类
class LogTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建内存追加器
        m_memory_appender = std::make_shared<memory_appender>();
        
        // 创建内存后端
        m_memory_backend = std::make_shared<memory_backend>();
        m_memory_backend->init("test_config");
        
        // 创建后端适配器
        m_backend_adapter = mc::log::log_manager::instance().create_backend_adapter(m_memory_backend);
        
        // 创建测试日志记录器
        m_test_logger = mc::log::log_manager::instance().get_logger("test_logger");
        m_test_logger.add_appender(m_memory_appender);
        m_test_logger.add_appender(m_backend_adapter);
        m_test_logger.set_level(mc::log::level::trace);
    }
    
    void TearDown() override {
        // 清理资源
        m_test_logger.remove_appender("memory");
        if (m_backend_adapter) {
            m_test_logger.remove_appender(m_memory_backend->name());
        }
        
        // 清空消息
        if (m_memory_appender) {
            m_memory_appender->clear();
        }
        if (m_memory_backend) {
            m_memory_backend->clear();
        }
        
        // 重置智能指针
        m_backend_adapter.reset();
        m_memory_appender.reset();
        m_memory_backend.reset();
    }
    
    // 辅助方法：检查日志消息是否包含指定文本
    bool message_contains(const mc::log::message& msg, const std::string& text) {
        return msg.m_message.find(text) != std::string::npos;
    }
    
    // 辅助方法：检查日志消息是否匹配正则表达式
    bool message_matches(const mc::log::message& msg, const std::string& pattern) {
        std::regex regex(pattern);
        return std::regex_search(msg.m_message, regex);
    }
    
    // 辅助方法：获取最后一条日志消息
    mc::log::message get_last_message() {
        auto& messages = m_memory_appender->get_messages();
        EXPECT_FALSE(messages.empty());
        return messages.back();
    }
    
    std::shared_ptr<memory_appender> m_memory_appender;
    std::shared_ptr<memory_backend> m_memory_backend;
    std::shared_ptr<mc::log::appender> m_backend_adapter;
    mc::log::logger m_test_logger;
};

// 测试基本日志功能
TEST_F(LogTest, BasicLogging) {
    // 使用结构化日志宏
    MC_WLOG_INFO(m_test_logger, "这是一条信息日志");
    
    // 检查日志消息
    auto& messages = m_memory_appender->get_messages();
    ASSERT_FALSE(messages.empty());
    auto msg = messages.back();
    EXPECT_EQ(mc::log::level::info, msg.m_level);
    EXPECT_TRUE(message_contains(msg, "这是一条信息日志"));
    
    // 使用不同级别的日志宏
    MC_WLOG_TRACE(m_test_logger, "跟踪日志");
    MC_WLOG_DEBUG(m_test_logger, "调试日志");
    MC_WLOG_WARN(m_test_logger, "警告日志");
    
    // 检查日志消息数量
    ASSERT_EQ(4, messages.size());
    
    // 检查最后一条消息
    msg = messages.back();
    EXPECT_EQ(mc::log::level::warn, msg.m_level);
    EXPECT_TRUE(message_contains(msg, "警告日志"));
}

// 测试结构化日志功能
TEST_F(LogTest, StructuredLogging) {
    // 使用结构化日志宏
    MC_WLOG_INFO(m_test_logger, "用户 ${user} 登录成功，IP: ${ip}", 
        stream("user", "admin")("ip", "192.168.1.1"));
    
    // 检查日志消息
    auto& messages = m_memory_appender->get_messages();
    ASSERT_FALSE(messages.empty());
    auto msg = messages.back();
    EXPECT_EQ(mc::log::level::info, msg.m_level);
    EXPECT_TRUE(message_contains(msg, "用户 admin 登录成功，IP: 192.168.1.1"));
    
    // 检查参数字典
    EXPECT_TRUE(msg.m_args.contains("user"));
    EXPECT_TRUE(msg.m_args.contains("ip"));
    EXPECT_EQ("admin", msg.m_args["user"].as_string());
    EXPECT_EQ("192.168.1.1", msg.m_args["ip"].as_string());
    
    // 使用多个参数
    MC_WLOG_DEBUG(m_test_logger, "请求 ${method} ${url} 返回 ${status}", 
        stream("method", "GET")("url", "/api/users")("status", 200));
    
    // 检查日志消息
    ASSERT_EQ(2, messages.size());
    msg = messages.back();
    EXPECT_EQ(mc::log::level::debug, msg.m_level);
    EXPECT_TRUE(message_contains(msg, "请求 GET /api/users 返回 200"));
    
    // 检查参数字典
    EXPECT_TRUE(msg.m_args.contains("method"));
    EXPECT_TRUE(msg.m_args.contains("url"));
    EXPECT_TRUE(msg.m_args.contains("status"));
}

// 测试日志级别过滤
TEST_F(LogTest, LevelFiltering) {
    // 设置日志级别为警告
    m_test_logger.set_level(mc::log::level::warn);
    
    // 发送不同级别的日志
    MC_WLOG_TRACE(m_test_logger, "跟踪日志");  // 不应该记录
    MC_WLOG_DEBUG(m_test_logger, "调试日志");  // 不应该记录
    MC_WLOG_INFO(m_test_logger, "信息日志");   // 不应该记录
    MC_WLOG_WARN(m_test_logger, "警告日志");   // 应该记录
    MC_WLOG_ERROR(m_test_logger, "错误日志");  // 应该记录
    
    // 检查日志消息
    auto& messages = m_memory_appender->get_messages();
    ASSERT_EQ(2, messages.size());
    
    // 检查第一条消息
    auto& msg1 = messages[0];
    EXPECT_EQ(mc::log::level::warn, msg1.m_level);
    EXPECT_TRUE(message_contains(msg1, "警告日志"));
    
    // 检查第二条消息
    auto& msg2 = messages[1];
    EXPECT_EQ(mc::log::level::error, msg2.m_level);
    EXPECT_TRUE(message_contains(msg2, "错误日志"));
    
    // 恢复日志级别
    m_test_logger.set_level(mc::log::level::trace);
}

// 测试日志上下文信息
TEST_F(LogTest, ContextInfo) {
    // 使用带上下文的日志宏
    MC_WLOG_INFO(m_test_logger, "带有上下文的日志消息");
    
    // 检查日志消息
    auto& messages = m_memory_appender->get_messages();
    ASSERT_FALSE(messages.empty());
    auto& msg = messages.back();
    
    // 检查上下文信息
    EXPECT_FALSE(msg.m_context.m_file.empty());
    EXPECT_FALSE(msg.m_context.m_function.empty());
    EXPECT_GT(msg.m_context.m_line, 0);
    
    // 检查上下文信息是否包含在消息中
    auto& backend_messages = m_memory_backend->get_messages();
    ASSERT_FALSE(backend_messages.empty());
    auto& backend_msg = backend_messages.back();
    EXPECT_TRUE(message_contains(backend_msg, msg.m_context.m_file));
    EXPECT_TRUE(message_contains(backend_msg, msg.m_context.m_function));
}

// 测试日志后端
TEST_F(LogTest, LogBackend) {
    // 清空现有消息
    m_memory_backend->clear();
    
    // 发送日志消息
    MC_WLOG_INFO(m_test_logger, "测试后端的日志消息");
    
    // 检查后端是否收到消息
    auto& backend_messages = m_memory_backend->get_messages();
    ASSERT_FALSE(backend_messages.empty());
    
    // 检查消息内容
    auto& msg = backend_messages.back();
    EXPECT_EQ(mc::log::level::info, msg.m_level);
    EXPECT_TRUE(message_contains(msg, "测试后端的日志消息"));
}

// 测试全局日志宏
TEST_F(LogTest, GlobalLogMacros) {
    // 创建一个新的日志记录器，而不是使用默认日志记录器
    auto test_global_logger = mc::log::log_manager::instance().get_logger("test_global");
    
    // 添加内存追加器
    auto mem_appender = std::make_shared<memory_appender>();
    test_global_logger.add_appender(mem_appender);
    
    // 设置日志级别
    test_global_logger.set_level(mc::log::level::trace);
    
    // 使用指定日志记录器的日志宏
    MC_WLOG_INFO(test_global_logger, "这是一条全局信息日志");
    MC_WLOG_DEBUG(test_global_logger, "这是一条全局调试日志");
    MC_WLOG_WARN(test_global_logger, "这是一条全局警告日志");
    
    // 使用结构化日志宏
    MC_WLOG_INFO(test_global_logger, "用户 ${user} 登录", 
        stream("user", "guest"));
    
    // 检查日志消息
    auto& messages = mem_appender->get_messages();
    ASSERT_EQ(4, messages.size());
    
    // 检查最后一条消息
    auto& last_msg = messages.back();
    EXPECT_EQ(mc::log::level::info, last_msg.m_level);
    EXPECT_TRUE(message_contains(last_msg, "用户 guest 登录"));
}

// 测试复杂数据类型日志
TEST_F(LogTest, ComplexDataLogging) {
    // 创建复杂数据
    mc::dict user_info = mc::dict({
        {"id", 12345},
        {"name", "张三"},
        {"roles", mc::variants{"admin", "user"}},
        {"active", true}
    });
    
    // 记录包含复杂数据的日志
    MC_WLOG_INFO(m_test_logger, "用户信息: ${info}", 
        stream("info", user_info));
    
    // 检查日志消息
    auto& messages = m_memory_appender->get_messages();
    ASSERT_FALSE(messages.empty());
    auto& msg = messages.back();
    EXPECT_EQ(mc::log::level::info, msg.m_level);
    EXPECT_TRUE(message_contains(msg, "用户信息:"));
    
    // 检查参数字典
    EXPECT_TRUE(msg.m_args.contains("info"));
} 