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

#include <chrono>
#include <cstdio>
#include <fstream>
#include <gtest/gtest.h>
#include <mc/dict.h>
#include <mc/filesystem.h>
#include <mc/log.h>
#include <memory>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

// 测试用的日志追加器，将日志消息存储在内存中
class memory_appender : public mc::log::appender {
public:
    memory_appender()
    {}

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

// 日志框架测试类
class LogTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // 创建内存日志追加器
        m_memory_appender = std::make_shared<memory_appender>();
        m_memory_appender->set_name("memory_appender");

        // 创建测试日志器
        m_test_logger = mc::log::logger("test_logger");
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

    bool message_matches(const mc::log::message& msg, const std::string& pattern)
    {
        std::regex regex(pattern);
        return std::regex_search(msg.get_message(), regex);
    }

    std::string get_last_message()
    {
        const auto& messages = m_memory_appender->get_messages();
        return messages.empty() ? std::string{} : messages.back().get_message();
    }

    std::shared_ptr<memory_appender> m_memory_appender;
    mc::log::logger                  m_test_logger;
};

// 测试基本日志功能
TEST_F(LogTest, BasicLogging)
{
    // 确保日志级别设置为允许所有级别的日志
    m_test_logger.set_level(mc::log::level::trace);

    // 使用结构化日志宏
    mc_ilog(m_test_logger, "这是一条信息日志");

    // 检查日志消息
    const auto& messages = m_memory_appender->get_messages();
    ASSERT_FALSE(messages.empty()) << "日志消息未被添加到内存追加器";
    auto msg = messages.back();
    EXPECT_EQ(mc::log::level::info, msg.get_level());
    EXPECT_TRUE(message_contains(msg, "这是一条信息日志"));

    // 清除之前的消息
    m_memory_appender->clear();

    // 使用不同级别的日志宏
    mc_tlog(m_test_logger, "跟踪日志");
    mc_dlog(m_test_logger, "调试日志");
    mc_wlog(m_test_logger, "警告日志");
    mc_nlog(m_test_logger, "通知日志");

    // 重新获取消息列表
    const auto& new_messages = m_memory_appender->get_messages();

    // 检查日志消息数量
    ASSERT_EQ(4, new_messages.size()) << "应该有4条日志消息被添加";

    // 检查最后一条消息
    msg = new_messages.back();
    EXPECT_EQ(mc::log::level::notice, msg.get_level());
    EXPECT_TRUE(message_contains(msg, "通知日志"));
}

// 测试结构化日志功能
TEST_F(LogTest, StructuredLogging)
{
    // 确保日志级别设置为允许所有级别的日志
    m_test_logger.set_level(mc::log::level::trace);

    // 使用结构化日志宏
    mc_ilog(m_test_logger, "用户 ${user} 登录成功，IP: ${ip}", ("user", "admin")("ip", "192.168.1.1"));

    // 检查日志消息
    const auto& messages = m_memory_appender->get_messages();
    ASSERT_FALSE(messages.empty()) << "日志消息未被添加到内存追加器";
    auto msg = messages.back();
    EXPECT_EQ(mc::log::level::info, msg.get_level());
    EXPECT_TRUE(message_contains(msg, "用户 admin 登录成功，IP: 192.168.1.1"));

    // 检查参数字典
    const auto& args = msg.get_args();
    EXPECT_TRUE(args.contains("user"));
    EXPECT_TRUE(args.contains("ip"));
    EXPECT_EQ("admin", args["user"].as_string());
    EXPECT_EQ("192.168.1.1", args["ip"].as_string());

    // 清除之前的消息
    m_memory_appender->clear();

    // 使用多个参数
    mc_dlog(m_test_logger, "请求 ${method} ${url} 返回 ${status}",
            ("method", "GET")("url", "/api/users")("status", 200));

    // 检查日志消息
    const auto& new_messages = m_memory_appender->get_messages();
    ASSERT_EQ(1, new_messages.size()) << "应该有1条日志消息被添加";
    msg = new_messages.back();
    EXPECT_EQ(mc::log::level::debug, msg.get_level());
    EXPECT_TRUE(message_contains(msg, "请求 GET /api/users 返回 200"));

    // 检查参数字典
    const auto& args2 = msg.get_args();
    EXPECT_TRUE(args2.contains("method"));
    EXPECT_TRUE(args2.contains("url"));
    EXPECT_TRUE(args2.contains("status"));
}

// 测试可选 attrs：最后一个参数是 dict 时自动作为 attrs
TEST_F(LogTest, log_with_optional_attrs)
{
    m_test_logger.set_level(mc::log::level::info);

    mc::dict attrs;
    attrs["trace_id"] = std::string("abc-123");
    attrs["span_id"]  = std::string("span-1");
    attrs["count"]    = 42;

    mc_ilog(m_test_logger, "count=${i}", ("i", 1), attrs);

    auto& messages = m_memory_appender->get_messages();
    ASSERT_FALSE(messages.empty());
    auto& last_msg = messages.back();
    EXPECT_EQ(mc::log::level::info, last_msg.get_level());
    EXPECT_TRUE(message_contains(last_msg, "count=1"));
    EXPECT_NE(last_msg.get_message().find("trace_id"), std::string::npos);
    EXPECT_NE(last_msg.get_message().find("abc-123"), std::string::npos);
    EXPECT_NE(last_msg.get_message().find("span_id"), std::string::npos);
    EXPECT_NE(last_msg.get_message().find("span-1"), std::string::npos);
    EXPECT_NE(last_msg.get_message().find("count"), std::string::npos);
    EXPECT_NE(last_msg.get_message().find("42"), std::string::npos);
    EXPECT_FALSE(last_msg.get_attrs().empty());
    EXPECT_EQ(last_msg.get_attrs()["trace_id"].as_string(), "abc-123");
    EXPECT_EQ(last_msg.get_attrs()["count"].as_int64(), 42);
}

// 测试可选 attrs：最后一个参数不是 dict 时，行为不变
TEST_F(LogTest, log_without_optional_attrs)
{
    m_test_logger.set_level(mc::log::level::info);

    mc_ilog(m_test_logger, "count=${i}", ("i", 1));

    auto& messages = m_memory_appender->get_messages();
    ASSERT_FALSE(messages.empty());
    auto& last_msg = messages.back();
    EXPECT_EQ(mc::log::level::info, last_msg.get_level());
    EXPECT_TRUE(message_contains(last_msg, "count=1"));
    EXPECT_TRUE(last_msg.get_attrs().empty());
}

// 测试多个格式参数 + attrs
TEST_F(LogTest, log_with_multiple_format_args_and_attrs)
{
    m_test_logger.set_level(mc::log::level::info);

    mc::dict attrs;
    attrs["trace_id"] = std::string("abc-123");
    attrs["span_id"]  = std::string("span-1");

    // 多个格式参数 + attrs
    mc_ilog(m_test_logger, "count=${i}, value=${a}", ("i", 1)("a", 2), attrs);

    auto& messages = m_memory_appender->get_messages();
    ASSERT_FALSE(messages.empty());
    auto& last_msg = messages.back();
    EXPECT_EQ(mc::log::level::info, last_msg.get_level());
    EXPECT_TRUE(message_contains(last_msg, "count=1"));
    EXPECT_TRUE(message_contains(last_msg, "value=2"));
    EXPECT_NE(last_msg.get_message().find("trace_id"), std::string::npos);
    EXPECT_NE(last_msg.get_message().find("abc-123"), std::string::npos);
    EXPECT_FALSE(last_msg.get_attrs().empty());
    EXPECT_EQ(last_msg.get_attrs()["trace_id"].as_string(), "abc-123");
}

// 测试多个格式参数，没有 attrs
TEST_F(LogTest, log_with_multiple_format_args_no_attrs)
{
    m_test_logger.set_level(mc::log::level::info);

    // 多个格式参数，没有 attrs
    mc_ilog(m_test_logger, "count=${i}, value=${a}", ("i", 1)("a", 2));

    auto& messages = m_memory_appender->get_messages();
    ASSERT_FALSE(messages.empty());
    auto& last_msg = messages.back();
    EXPECT_EQ(mc::log::level::info, last_msg.get_level());
    EXPECT_TRUE(message_contains(last_msg, "count=1"));
    EXPECT_TRUE(message_contains(last_msg, "value=2"));
    EXPECT_TRUE(last_msg.get_attrs().empty());
}

// 测试无 attrs 时行为不变
TEST_F(LogTest, message_without_attrs_unchanged)
{
    m_test_logger.set_level(mc::log::level::info);

    mc::log::message log_msg(mc::log::level::info, mc::log::context(__FILE__, __FUNCTION__, __LINE__), "no attrs",
                             mc::dict{});
    m_test_logger.log(log_msg);

    const auto& messages = m_memory_appender->get_messages();
    ASSERT_FALSE(messages.empty());
    EXPECT_EQ(messages.back().get_message(), "no attrs");
    EXPECT_TRUE(messages.back().get_attrs().empty());
}

// 测试日志级别过滤
TEST_F(LogTest, LevelFiltering)
{
    // 设置日志级别为警告
    m_test_logger.set_level(mc::log::level::warn);

    // 发送不同级别的日志
    mc_tlog(m_test_logger, "跟踪日志"); // 不应该记录
    mc_dlog(m_test_logger, "调试日志"); // 不应该记录
    mc_ilog(m_test_logger, "信息日志"); // 不应该记录
    mc_wlog(m_test_logger, "警告日志"); // 应该记录
    mc_elog(m_test_logger, "错误日志"); // 应该记录

    // 检查日志消息
    auto& messages = m_memory_appender->get_messages();
    ASSERT_EQ(2, messages.size());

    // 检查第一条消息
    auto& msg1 = messages[0];
    EXPECT_EQ(mc::log::level::warn, msg1.get_level());
    EXPECT_TRUE(message_contains(msg1, "警告日志"));

    // 检查第二条消息
    auto& msg2 = messages[1];
    EXPECT_EQ(mc::log::level::error, msg2.get_level());
    EXPECT_TRUE(message_contains(msg2, "错误日志"));

    // 恢复日志级别
    m_test_logger.set_level(mc::log::level::trace);
}

// 测试日志上下文信息
TEST_F(LogTest, ContextInfo)
{
    // 确保日志级别设置正确
    m_test_logger.set_level(mc::log::level::info);

    // 使用带上下文的日志宏
    mc_ilog(m_test_logger, "带有上下文的日志消息");

    // 检查日志消息
    auto& messages = m_memory_appender->get_messages();

    // 确保至少有一条消息被记录
    ASSERT_FALSE(messages.empty()) << "未捕获到日志消息，请检查日志级别设置";

    auto& msg = messages.back();

    // 检查上下文信息是否正确捕获
    const auto& ctx = msg.get_context();
    EXPECT_FALSE(ctx.m_file.empty());
    EXPECT_FALSE(ctx.m_function.empty());
    EXPECT_GT(ctx.m_line, 0);

    // 检查原始消息不包含上下文信息（因为使用延迟合并）
    EXPECT_EQ(msg.get_message().find(ctx.m_file), std::string::npos);
    EXPECT_EQ(msg.get_message().find(ctx.m_function), std::string::npos);
}

// 测试全局日志宏
TEST_F(LogTest, GlobalLogMacros)
{
    // 创建一个新的日志记录器，而不是使用默认日志记录器
    auto test_global_logger = mc::log::log_manager::instance().get_logger("test_global");

    // 添加内存追加器
    auto mem_appender = std::make_shared<memory_appender>();
    test_global_logger.add_appender(mem_appender);

    // 设置日志级别
    test_global_logger.set_level(mc::log::level::trace);

    // 使用指定日志记录器的日志宏
    mc_ilog(test_global_logger, "这是一条全局信息日志");
    mc_dlog(test_global_logger, "这是一条全局调试日志");
    mc_wlog(test_global_logger, "这是一条全局警告日志");

    // 使用结构化日志宏
    mc_ilog(test_global_logger, "用户 ${user} 登录", ("user", "guest"));

    // 检查日志消息
    auto& messages = mem_appender->get_messages();
    ASSERT_EQ(4, messages.size());

    // 检查最后一条消息
    auto& last_msg = messages.back();
    EXPECT_EQ(mc::log::level::info, last_msg.get_level());
    EXPECT_TRUE(message_contains(last_msg, "用户 guest 登录"));
}

// 测试复杂数据类型日志
TEST_F(LogTest, ComplexDataLogging)
{
    // 创建复杂数据
    mc::dict user_info =
        mc::dict({{"id", 12345}, {"name", "张三"}, {"roles", mc::variants{"admin", "user"}}, {"active", true}});

    // 记录包含复杂数据的日志
    mc_ilog(m_test_logger, "用户信息: ${info}", ("info", user_info));

    // 检查日志消息
    auto& messages = m_memory_appender->get_messages();
    ASSERT_FALSE(messages.empty());
    auto& msg = messages.back();
    EXPECT_EQ(mc::log::level::info, msg.get_level());
    EXPECT_TRUE(message_contains(msg, "用户信息:"));

    // 检查参数字典
    const auto& args = msg.get_args();
    EXPECT_TRUE(args.contains("info"));
}

/*
早期日志只支持 ${} 命名参数，为了使用简单我们将日志格式化与 sformat 保持一致，
即支持 {} 占位也支持 ${} 命名占位符，其中 {} 既可以索引占位也可以命名占位，${} 占位
只是兼容旧的写法，建议的日志写法全部是 {} 占位符
*/
TEST_F(LogTest, basic_smart_log)
{
    // 测试自增索引 {}
    mc_ilog(m_test_logger, "{} {}", "Hello", "World");
    EXPECT_EQ(get_last_message(), "Hello World");

    // 测试显式索引 {0}, {1}
    mc_ilog(m_test_logger, "{1} {0}", "World", "Hello");
    EXPECT_EQ(get_last_message(), "Hello World");

    // 测试命名参数 {name}
    mc_ilog(m_test_logger, "{name} {value}", ("name", "Answer"), ("value", 42));
    EXPECT_EQ(get_last_message(), "Answer 42");
}

// 测试混合使用不同类型的占位符
TEST_F(LogTest, mixed_placeholder_types)
{
    // 混合自增索引和命名参数
    mc_ilog(m_test_logger, "{} is {age} years old", "Alice", ("age", 25));
    EXPECT_EQ(get_last_message(), "Alice is 25 years old");

    // 混合显式索引和命名参数
    mc_ilog(m_test_logger, "{0} has {balance:.2f} dollars", "Bob", ("balance", 123.456));
    EXPECT_EQ(get_last_message(), "Bob has 123.46 dollars");

    // 混合所有类型
    mc_ilog(m_test_logger, "{} {name} {2}", "Hello", ("name", "Test"), "World"); // 命名参数也会增加索引
    EXPECT_EQ(get_last_message(), "Hello Test World");
}

// 测试带格式说明符的智能占位符
TEST_F(LogTest, format_specifications)
{
    // 自增索引带格式
    mc_ilog(m_test_logger, "{:.2f} {}", 3.14159, "pi");
    EXPECT_EQ(get_last_message(), "3.14 pi");

    // 显式索引带格式
    mc_ilog(m_test_logger, "{0:.2f} {1:>10}", 3.14159, "right");
    EXPECT_EQ(get_last_message(), "3.14      right");

    // 命名参数带格式
    mc_ilog(m_test_logger, "{name:<10} {value:04d}", ("name", "Number"), ("value", 42));
    EXPECT_EQ(get_last_message(), "Number     0042");
}

// 测试动态格式参数
TEST_F(LogTest, dynamic_format_parameters)
{
    // 使用命名参数作为动态宽度和精度
    mc_ilog(m_test_logger, "{value:{width}.{precision}f}", ("value", 3.14159), ("width", 10), ("precision", 3));
    EXPECT_EQ(get_last_message(), "     3.142");

    // 混合索引和命名参数的动态格式
    mc_ilog(m_test_logger, "{0:{width}.2f}", 3.14159, ("width", 8));
    EXPECT_EQ(get_last_message(), "    3.14");
}

// 测试边界情况
TEST_F(LogTest, edge_cases)
{
    // 单个字符的命名参数
    mc_ilog(m_test_logger, "{x} {y}", ("x", 10), ("y", 20));
    EXPECT_EQ(get_last_message(), "10 20");

    // 长命名参数
    mc_ilog(m_test_logger, "{very_long_parameter_name}", ("very_long_parameter_name", "value"));
    EXPECT_EQ(get_last_message(), "value");

    // 提供了3个参数但格式化字符串只用了一部分，正常编译期报错，这里使用不安全版本
    mc_ilog_unsafe(m_test_logger, "{2}", "first", "second", "target_value");
    EXPECT_EQ(get_last_message(), "target_value");

    // 格式化字符串不是字面值常量而是变量，只能使用不安全版本
    const char* fmt = "{x} {y}";
    mc_ilog_unsafe(m_test_logger, fmt, ("x", 10), ("y", 20));
    EXPECT_EQ(get_last_message(), "10 20");
}

// 测试嵌套大括号
TEST_F(LogTest, nested_braces)
{
    // 命名参数中的嵌套大括号格式
    mc_ilog(m_test_logger, "{value:{width}.{precision}f}", ("value", 123.456), ("width", 12), ("precision", 2));
    EXPECT_EQ(get_last_message(), "      123.46");

    // 索引参数中的嵌套大括号格式
    mc_ilog(m_test_logger, "{0:{1}.{2}f}", 123.456, 12, 2);
    EXPECT_EQ(get_last_message(), "      123.46");
}

// 测试转义字符
TEST_F(LogTest, escaped_braces)
{
    // 转义的大括号与智能占位符混合
    mc_ilog(m_test_logger, "{{}} {name} {{}}", ("name", "test"));
    EXPECT_EQ(get_last_message(), "{} test {}");

    mc_ilog(m_test_logger, "{{0}} {0} {{name}}", "value");
    EXPECT_EQ(get_last_message(), "{0} value {name}");
}

// 测试 logger 的追加器管理功能
TEST_F(LogTest, manage_appenders_lifecycle)
{
    auto secondary = std::make_shared<memory_appender>();
    secondary->set_name("second_appender");
    m_test_logger.add_appender(secondary);

    EXPECT_NE(m_test_logger.find_appender("memory_appender"), nullptr);
    EXPECT_NE(m_test_logger.find_appender("second_appender"), nullptr);

    EXPECT_TRUE(m_test_logger.remove_appender("second_appender"));
    EXPECT_EQ(m_test_logger.find_appender("second_appender"), nullptr);
    EXPECT_FALSE(m_test_logger.remove_appender("second_appender"));

    m_test_logger.clear_appenders();
    EXPECT_TRUE(m_test_logger.get_appenders().empty());
}

// 测试 condition 配置：condition 为 false 时不输出日志
TEST_F(LogTest, condition_false_suppresses_logging)
{
    m_test_logger.set_level(mc::log::level::trace);
    m_test_logger.condition(false);

    mc_ilog(m_test_logger, "不应输出的日志");
    mc_wlog(m_test_logger, "不应输出的警告");

    EXPECT_TRUE(m_memory_appender->get_messages().empty());
}

// 测试 condition 配置：condition 为 true 时正常输出日志
TEST_F(LogTest, condition_true_allows_logging)
{
    m_test_logger.set_level(mc::log::level::trace);
    m_test_logger.condition(true);

    mc_ilog(m_test_logger, "应输出的日志");

    const auto& messages = m_memory_appender->get_messages();
    ASSERT_EQ(1, messages.size());
    EXPECT_TRUE(message_contains(messages.back(), "应输出的日志"));
}

// 测试 condition(bool) 链式接口，仿照 period/system
TEST_F(LogTest, condition_chainable)
{
    m_test_logger.set_level(mc::log::level::trace);

    // 链式调用：condition(false) 后不应输出
    m_test_logger.condition(false).log(MC_LOG_MESSAGE(info, "链式 condition(false) 不应输出"));
    EXPECT_TRUE(m_memory_appender->get_messages().empty());

    m_memory_appender->clear();

    // 链式调用：condition(true) 后应输出
    m_test_logger.condition(true).log(MC_LOG_MESSAGE(info, "链式 condition(true) 应输出"));
    ASSERT_EQ(m_memory_appender->get_messages().size(), 1);
    EXPECT_TRUE(message_contains(m_memory_appender->get_messages().back(), "链式 condition(true) 应输出"));
}

// 测试 condition 为 false 时 log() 直接调用也不输出
TEST_F(LogTest, condition_false_log_direct_call)
{
    m_test_logger.set_level(mc::log::level::trace);
    m_test_logger.condition(false);

    mc::log::context ctx(__FILE__, __FUNCTION__, static_cast<uint32_t>(__LINE__));
    mc::log::message msg(mc::log::level::info, ctx, "直接调用", mc::dict{});
    m_test_logger.log(msg);

    EXPECT_TRUE(m_memory_appender->get_messages().empty());
}

// 测试 logger 的命名与级别判定行为
TEST_F(LogTest, set_name_and_enabled_boundary)
{
    m_test_logger.set_name("custom_logger");
    m_test_logger.set_level(mc::log::level::info);

    EXPECT_EQ(m_test_logger.get_name(), "custom_logger");
    EXPECT_FALSE(m_test_logger.is_enabled(mc::log::level::debug));
    EXPECT_TRUE(m_test_logger.is_enabled(mc::log::level::info));
    EXPECT_TRUE(m_test_logger.is_enabled(mc::log::level::error));
}

// 覆盖 logger::log 内部的快速返回路径
TEST_F(LogTest, log_respects_is_enabled_fast_path)
{
    m_test_logger.set_level(mc::log::level::error);
    mc::log::context ctx(__FILE__, __FUNCTION__, static_cast<uint32_t>(__LINE__));

    mc::log::message low(mc::log::level::debug, "调试信息", ctx, {});
    m_test_logger.log(low);
    EXPECT_TRUE(m_memory_appender->get_messages().empty());

    mc::log::message high(mc::log::level::fatal, ctx, "致命 ${code}", mc::dict{{"code", 500}});
    m_test_logger.log(high);
    ASSERT_EQ(m_memory_appender->get_messages().size(), 1);
    EXPECT_TRUE(message_contains(m_memory_appender->get_messages().back(), "致命"));
}

// 测试 message 的结构化数据与惰性格式化
TEST(LogMessageTest, structured_data_and_lazy_formatting)
{
    mc::log::context ctx{"file.cpp", "Func", 123};
    mc::dict         args{{"user", "alice"}, {"value", 42}};
    mc::log::message msg(mc::log::level::warn, ctx, "用户 ${user} 值 ${value}", args);

    const auto start = std::chrono::system_clock::now();
    auto       data  = msg.to_structured_data();
    const auto end   = std::chrono::system_clock::now();

    EXPECT_TRUE(end >= msg.get_timestamp());
    EXPECT_TRUE(msg.get_timestamp() >= start - std::chrono::seconds(1));

    EXPECT_EQ(data["level"].as_int32(), static_cast<int>(mc::log::level::warn));

    const auto& ctx_dict = data["context"].as_dict();
    EXPECT_EQ(ctx_dict["file"].as_string(), "file.cpp");
    EXPECT_EQ(ctx_dict["function"].as_string(), "Func");
    EXPECT_EQ(ctx_dict["line"].as_uint32(), 123U);
    std::ostringstream thread_stream;
    thread_stream << msg.get_thread_id();
    EXPECT_EQ(ctx_dict["thread_id"].as_string(), thread_stream.str());

    EXPECT_EQ(data["message_template"].as_string(), "用户 ${user} 值 ${value}");
    EXPECT_EQ(data["message"].as_string(), "用户 alice 值 42");

    const auto& args_dict = data["args"].as_dict();
    EXPECT_EQ(args_dict.at("user").as_string(), "alice");
    EXPECT_EQ(args_dict.at("value").as_int32(), 42);

    // 再次获取消息应返回相同的格式化结果
    EXPECT_EQ(msg.get_message(), "用户 alice 值 42");
}

TEST(LogMessageTest, structured_data_without_format_template)
{
    mc::log::context ctx{"simple.cpp", "Func", 9};
    mc::log::message msg(mc::log::level::info, "纯文本消息", ctx, {});

    auto data = msg.to_structured_data();
    EXPECT_FALSE(data.contains("message_template"));
    EXPECT_FALSE(data.contains("args"));
    EXPECT_EQ(data["message"].as_string(), "纯文本消息");
}

TEST(LoggerStaticGetTest, static_get_returns_named_logger)
{
    auto retrieved = mc::log::logger::get("static_logger_entry");
    EXPECT_EQ(retrieved.get_name(), "static_logger_entry");
}

// 测试 logger 的拷贝构造函数和赋值运算符
TEST(LoggerTest, CopyConstructorAndAssignment)
{
    mc::log::logger logger1("test_logger");
    logger1.set_level(mc::log::level::info);

    // 测试拷贝构造函数
    mc::log::logger logger2(logger1);
    EXPECT_EQ(logger2.get_name(), "test_logger");
    EXPECT_EQ(logger2.get_level(), mc::log::level::info);

    // 测试赋值运算符
    mc::log::logger logger3("another_logger");
    logger3 = logger1;
    EXPECT_EQ(logger3.get_name(), "test_logger");
    EXPECT_EQ(logger3.get_level(), mc::log::level::info);

    // 测试自赋值（覆盖 this == &other 分支，抑制编译器自赋值告警）
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wself-assign-overloaded"
#endif
    logger3 = logger3;
#ifdef __clang__
#pragma clang diagnostic pop
#endif
    EXPECT_EQ(logger3.get_name(), "test_logger");
}

// 测试 logger 的移动构造函数和移动赋值运算符
TEST(LoggerTest, MoveConstructorAndAssignment)
{
    mc::log::logger logger1("move_test");
    logger1.set_level(mc::log::level::warn);

    // 测试移动构造函数
    mc::log::logger logger2(std::move(logger1));
    EXPECT_EQ(logger2.get_name(), "move_test");
    EXPECT_EQ(logger2.get_level(), mc::log::level::warn);

    // 测试移动赋值运算符
    mc::log::logger logger3("temp");
    logger3 = std::move(logger2);
    EXPECT_EQ(logger3.get_name(), "move_test");
    EXPECT_EQ(logger3.get_level(), mc::log::level::warn);

    // 测试自移动赋值（覆盖 this == &other 分支，抑制编译器自移动告警）
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wself-move"
#endif
    logger3 = std::move(logger3);
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
    EXPECT_EQ(logger3.get_name(), "move_test");
}

// 测试添加 null appender（覆盖 add_appender 中 a 为空的分支）
TEST(LoggerTest, AddNullAppender)
{
    mc::log::logger       logger("null_test");
    mc::log::appender_ptr null_appender = nullptr;

    // 添加 null appender 应该不会崩溃，也不会添加
    logger.add_appender(null_appender);
    EXPECT_TRUE(logger.get_appenders().empty());
}

// 测试 remove_appender 找不到的情况（覆盖返回 false 的分支）
TEST(LoggerTest, RemoveNonExistentAppender)
{
    mc::log::logger logger("remove_test");
    auto            appender = std::make_shared<memory_appender>();
    appender->set_name("test_appender");
    logger.add_appender(appender);

    // 移除存在的 appender
    EXPECT_TRUE(logger.remove_appender("test_appender"));

    // 再次移除应该返回 false
    EXPECT_FALSE(logger.remove_appender("test_appender"));
    EXPECT_FALSE(logger.remove_appender("non_existent"));
}

// 测试 find_appender 找不到的情况（覆盖返回 nullptr 的分支）
TEST(LoggerTest, FindNonExistentAppender)
{
    mc::log::logger logger("find_test");

    // 查找不存在的 appender 应该返回 nullptr
    EXPECT_EQ(logger.find_appender("non_existent"), nullptr);

    // 添加 appender 后再查找
    auto appender = std::make_shared<memory_appender>();
    appender->set_name("found_appender");
    logger.add_appender(appender);

    auto found = logger.find_appender("found_appender");
    EXPECT_NE(found, nullptr);
    EXPECT_EQ(found->get_name(), "found_appender");
}
