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
#include <mc/log/appenders/file_appender.h>
#include <mc/log/log_message.h>
#include <mc/variant.h>
#include <test_utilities/test_base.h>

#include <cstdarg>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace mc::log;

// 函数指针类型声明，需与file_appender.cpp一致
// 注意：依赖库logging.h中已定义DLOG_LEVEL_E，我们直接使用它
// file_appender.cpp中的DLOG_LEVEL_E与依赖库中的DLOG_LEVEL_E在数值上是兼容的
// 因为两者都使用相同的枚举值：0=DLOG_ERROR, 1=DLOG_WARN, 2=DLOG_NOTICE, 3=DLOG_INFO, 4=DLOG_DEBUG

// 定义函数指针类型，使用依赖库中已定义的DLOG_LEVEL_E
// 由于file_appender.cpp通过dlsym获取函数指针，这里使用int类型以确保兼容性
typedef void (*debug_log_func_t)(int, const char*, int, const char*, ...);

// mock debug_log实现
// 使用不同的函数名避免与依赖库中的debug_log宏冲突
// 使用int类型作为第一个参数，因为枚举值在C/C++中可以隐式转换为int
extern "C" void test_debug_log(int level, const char* file, int line, const char* fmt, ...) {
    static const std::string log_path = std::string(TEST_LOG_DIR) + "/test_file_appender_mock.log";
    std::filesystem::create_directories(TEST_LOG_DIR);
    std::ofstream ofs(log_path, std::ios::app);
    if (!ofs.is_open()) {
        std::cout << "[mock debug_log] open file failed: " << log_path << std::endl;
        return;
    }
    va_list args;
    va_start(args, fmt);
    char buf[1024];
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    ofs << buf << std::endl;
}

class file_appender_test : public mc::test::TestBase {
protected:
    static std::shared_ptr<file_appender> m_appender;
    static std::filesystem::path          m_test_log_file;
    static void                           SetUpTestSuite() {
        m_appender      = std::make_shared<file_appender>();
        m_test_log_file = std::string(TEST_LOG_DIR) + "/test_file_appender_mock.log";
        // 初始化日志文件（直接清空文件）
        std::ofstream ofs(m_test_log_file, std::ios::trunc);
        ofs.close();
        mc::dict dict;
        dict["filename"]       = m_test_log_file.string();
        dict["truncate"]       = true;
        dict["flush_on_write"] = true;
        m_appender->init(dict);
        file_appender::set_debug_log_ptr(reinterpret_cast<void*>(static_cast<debug_log_func_t>(test_debug_log)));
    }
    static void TearDownTestSuite() {
        m_appender.reset();
        if (std::filesystem::exists(m_test_log_file)) {
            std::filesystem::remove(m_test_log_file);
        }
    }
    void SetUp() override {
        mc::test::TestBase::SetUp();
        mc::log::default_logger().set_level(mc::log::level::off);
    }

    void TearDown() override {
        // m_appender.reset(); // This line is removed as m_appender is now static
        // 无需close_log_file，直接删除
        if (std::filesystem::exists(m_test_log_file)) {
            std::filesystem::remove(m_test_log_file);
        }
        mc::test::TestBase::TearDown();
    }

    // 创建一个测试消息
    message create_test_message(level lvl, const std::string& msg) {
        mc::log::context ctx("test_file.cpp", "test_function", 123);
        return message(lvl, msg, ctx);
    }

    // 创建一个格式化测试消息
    message create_format_message(level lvl, const std::string& fmt, const mc::dict& args) {
        mc::log::context ctx("test_file.cpp", "test_function", 123);
        return message(lvl, ctx, fmt, args);
    }

    // 检查日志文件是否存在且不为空时，直接用m_test_log_file
    bool check_log_file_exists_and_not_empty() {
        if (!std::filesystem::exists(m_test_log_file)) {
            return false;
        }
        std::ifstream file(m_test_log_file);
        if (!file.is_open()) {
            return false;
        }
        file.seekg(0, std::ios::end);
        return file.tellg() > 0;
    }

    // 检查指定日志文件是否存在且不为空
    bool check_log_file_exists_and_not_empty(const std::string& filename) {
        if (!std::filesystem::exists(filename)) {
            return false;
        }

        std::ifstream file(filename);
        if (!file.is_open()) {
            return false;
        }

        // 检查文件是否为空
        file.seekg(0, std::ios::end);
        return file.tellg() > 0;
    }
};

// 静态成员定义
std::shared_ptr<file_appender> file_appender_test::m_appender;
std::filesystem::path          file_appender_test::m_test_log_file;

// 测试默认构造函数
TEST_F(file_appender_test, DefaultConstructor) {
    ASSERT_NE(m_appender, nullptr);
}

// 测试设置和获取文件名
TEST_F(file_appender_test, SetAndGetFilename) {
    std::string test_filename = "test_log.txt";
    m_appender->set_filename(test_filename);
    EXPECT_EQ(m_appender->get_filename(), test_filename);
}

// 测试设置和获取flush_on_write
TEST_F(file_appender_test, SetAndGetFlushOnWrite) {
    m_appender->set_flush_on_write(true);
    EXPECT_TRUE(m_appender->get_flush_on_write());

    m_appender->set_flush_on_write(false);
    EXPECT_FALSE(m_appender->get_flush_on_write());
}

// 测试追加函数 - 简单文本消息
TEST_F(file_appender_test, AppendSimpleTextMessage) {
    // 设置文件名
    m_appender->set_filename(m_test_log_file.string());

    // 创建简单文本测试消息
    auto msg = create_test_message(level::info, "这是一条测试消息");

    // 追加消息
    ASSERT_NO_THROW(m_appender->append(msg));

    // 刷新文件
    m_appender->flush();

    // 验证文件是否被创建且不为空
    EXPECT_TRUE(check_log_file_exists_and_not_empty());
}

// 测试追加函数 - 包含格式化占位符的消息
TEST_F(file_appender_test, AppendMessageWithFormatPlaceholders) {
    // 设置文件名
    m_appender->set_filename(m_test_log_file.string());

    // 创建包含格式化占位符的测试消息
    auto msg = create_test_message(level::info, "这是一条%s测试消息%d, %p");

    // 追加消息
    ASSERT_NO_THROW(m_appender->append(msg));

    // 刷新文件
    m_appender->flush();

    // 验证文件是否被创建且不为空
    EXPECT_TRUE(check_log_file_exists_and_not_empty());
}

// 测试追加函数 - 字典参数格式化消息
TEST_F(file_appender_test, AppendDictFormattedMessage) {
    // 设置文件名
    m_appender->set_filename(m_test_log_file.string());

    // 创建字典参数格式化消息
    mc::dict args;
    args["name"]  = "测试";
    args["value"] = 42;

    auto msg = create_format_message(level::info, "名称: ${name}, 值: ${value}", args);

    // 追加消息
    ASSERT_NO_THROW(m_appender->append(msg));

    // 刷新文件
    m_appender->flush();

    // 验证文件是否被创建且不为空
    EXPECT_TRUE(check_log_file_exists_and_not_empty());
}

// 测试追加函数 - 多参数字典格式化消息
TEST_F(file_appender_test, AppendMultiParamDictFormattedMessage) {
    // 设置文件名
    m_appender->set_filename(m_test_log_file.string());

    // 创建多参数字典格式化消息
    mc::dict args;
    args["user"]  = "张三";
    args["age"]   = 25;
    args["city"]  = "北京";
    args["score"] = 95.5;

    auto msg = create_format_message(level::info,
                                     "用户: ${user}, 年龄: ${age}, 城市: ${city}, 分数: ${score}", args);

    // 追加消息
    ASSERT_NO_THROW(m_appender->append(msg));

    // 刷新文件
    m_appender->flush();

    // 验证文件是否被创建且不为空
    EXPECT_TRUE(check_log_file_exists_and_not_empty());
}

// 测试不同日志级别的消息追加
TEST_F(file_appender_test, AppendDifferentLogLevels) {
    // 设置文件名
    m_appender->set_filename(m_test_log_file.string());

    // 创建不同级别的消息
    auto debug_msg  = create_test_message(level::debug, "调试消息");
    auto info_msg   = create_test_message(level::info, "信息消息");
    auto warn_msg   = create_test_message(level::warn, "警告消息");
    auto error_msg  = create_test_message(level::error, "错误消息");
    auto notice_msg = create_test_message(level::notice, "通知消息");

    // 追加所有消息
    ASSERT_NO_THROW(m_appender->append(debug_msg));
    ASSERT_NO_THROW(m_appender->append(info_msg));
    ASSERT_NO_THROW(m_appender->append(warn_msg));
    ASSERT_NO_THROW(m_appender->append(error_msg));
    ASSERT_NO_THROW(m_appender->append(notice_msg));

    // 刷新文件
    m_appender->flush();

    // 验证文件是否被创建且不为空
    EXPECT_TRUE(check_log_file_exists_and_not_empty());
}

// 测试多线程并发追加消息
TEST_F(file_appender_test, AppendMessagesConcurrently) {
    m_appender->set_filename(m_test_log_file.string());

    // 创建多个线程同时写入日志
    std::vector<std::thread> threads;
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([this, i]() {
            for (int j = 0; j < 10; ++j) {
                auto msg = create_test_message(level::info,
                                               "线程 " + std::to_string(i) + " 消息 " + std::to_string(j));
                m_appender->append(msg);
            }
        });
    }

    // 等待所有线程完成
    for (auto& thread : threads) {
        thread.join();
    }

    // 刷新文件
    m_appender->flush();

    // 验证文件是否被创建且不为空
    EXPECT_TRUE(check_log_file_exists_and_not_empty());
}