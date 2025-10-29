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
#include <mc/dict.h>
#include <mc/log.h>
#include <mc/log/appender.h>
#include <mc/log/appender_factory.h>
#include <mc/log/appenders/console_appender.h>
#include <mc/log/appenders/file_appender.h>
#include <mc/log/log_manager.h>
#include <mc/log/log_message.h>
#include <test_utilities/test_base.h>

#include <chrono>
#include <memory>
#include <string>
#include <vector>

using namespace mc::log;

// 测试用的日志追加器，将日志消息存储在内存中
class memory_appender : public mc::log::appender {
public:
    memory_appender() {
    }

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

class log_manager_test : public mc::test::TestBase {
protected:
    void SetUp() override {
        mc::test::TestBase::SetUp();
        mc::log::default_logger().set_level(mc::log::level::off);
    }

    void TearDown() override {
        mc::test::TestBase::TearDown();
    }
};

// 测试 log_manager 单例模式
TEST_F(log_manager_test, SingletonInstance) {
    log_manager& manager1 = log_manager::instance();
    log_manager& manager2 = log_manager::instance();

    // 应该是同一个实例
    ASSERT_EQ(&manager1, &manager2);
}

// 测试获取默认 logger
TEST_F(log_manager_test, GetDefaultLogger) {
    log_manager& manager = log_manager::instance();
    logger       default_logger = manager.get_logger();

    ASSERT_EQ(default_logger.get_name(), "default");
}

// 测试获取指定名称的 logger
TEST_F(log_manager_test, GetNamedLogger) {
    log_manager& manager = log_manager::instance();
    logger       test_logger = manager.get_logger("test_logger");

    ASSERT_EQ(test_logger.get_name(), "test_logger");
}

// 测试多次获取同一个 logger 返回相同的实例
TEST_F(log_manager_test, GetSameLoggerMultipleTimes) {
    log_manager& manager = log_manager::instance();
    logger       logger1 = manager.get_logger("same_logger");
    logger       logger2 = manager.get_logger("same_logger");

    // 虽然对象不同（按值返回），但应该是从同一个管理器中获取的
    ASSERT_EQ(logger1.get_name(), logger2.get_name());
}

// 测试应用配置 - 创建新的 logger
TEST_F(log_manager_test, ApplyConfigCreateNewLogger) {
    log_manager& manager = log_manager::instance();

    // 创建一个内存 appender
    auto mem_appender = std::make_shared<memory_appender>();
    mem_appender->set_name("test_memory");
    appender_factory::instance().register_creator("test_memory", [mem_appender]() {
        return mem_appender;
    });

    // 创建日志配置
    logging_config config;
    appender_config app_config;
    app_config.name       = "test_memory";
    app_config.type       = "test_memory";
    app_config.lib_path   = "";
    app_config.properties = mc::dict{};

    config.appenders.push_back(app_config);

    logger_config log_config;
    log_config.name      = "test_configured_logger";
    log_config.level     = level::info;
    log_config.appenders = {"test_memory"};

    config.loggers.push_back(log_config);

    // 应用配置
    bool result = manager.apply_config(config);
    ASSERT_TRUE(result);

    // 获取配置后的 logger
    logger configured_logger = manager.get_logger("test_configured_logger");
    ASSERT_EQ(configured_logger.get_name(), "test_configured_logger");
    ASSERT_EQ(configured_logger.get_level(), level::info);
    ASSERT_FALSE(configured_logger.get_appenders().empty());
}

// 测试应用配置 - 更新现有 logger
TEST_F(log_manager_test, ApplyConfigUpdateExistingLogger) {
    log_manager& manager = log_manager::instance();

    // 创建一个内存 appender
    auto mem_appender = std::make_shared<memory_appender>();
    mem_appender->set_name("test_memory2");

    // 先获取一个 logger
    logger existing_logger = manager.get_logger("existing_logger");
    existing_logger.set_level(level::debug);
    existing_logger.add_appender(mem_appender);

    // 创建日志配置用于更新
    logging_config config;

    logger_config log_config;
    log_config.name      = "existing_logger";
    log_config.level     = level::warn; // 不同的级别
    log_config.appenders = {};

    config.loggers.push_back(log_config);

    // 应用配置
    bool result = manager.apply_config(config);
    ASSERT_TRUE(result);

    // 验证 logger 已被更新
    logger updated_logger = manager.get_logger("existing_logger");
    ASSERT_EQ(updated_logger.get_level(), level::warn);
}

// 测试动态库加载（模拟场景）
TEST_F(log_manager_test, LoadAppenderLibrary) {
    log_manager& manager = log_manager::instance();

    // 尝试加载一个不存在的动态库应该返回 false
    bool result = manager.load_appender("/path/to/nonexistent/lib.so", "test_appender");
    ASSERT_FALSE(result);

    // 加载空的目录应该不报错
    ASSERT_NO_THROW(manager.load_appenders("/nonexistent/path"));
}

// 测试工厂创建 appender
TEST_F(log_manager_test, FactoryCreateAppender) {
    // 使用唯一的名称避免冲突
    std::string unique_prefix = "factory_test_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    
    // 使用 appender_factory 创建 console appender
    mc::dict config;
    config["stream"]    = "std_out";
    config["use_color"] = true;
    appender_ptr console_ap = appender_factory::instance().create(unique_prefix + "_console", "console", config);
    ASSERT_NE(console_ap, nullptr);

    // 使用 appender_factory 创建 file appender
    mc::dict file_config;
    file_config["filename"]       = std::string(TEST_LOG_DIR) + "/test_file.log";
    file_config["truncate"]       = true;
    file_config["flush_on_write"] = true;
    appender_ptr file_ap = appender_factory::instance().create(unique_prefix + "_file", "file", file_config);
    ASSERT_NE(file_ap, nullptr);
}

// 测试配置包含多个 appender 和 logger
TEST_F(log_manager_test, MultipleAppendersAndLoggers) {
    log_manager& manager = log_manager::instance();

    logging_config config;

    // 创建两个 appender
    appender_config app_config1;
    app_config1.name       = "console1";
    app_config1.type       = "console";
    app_config1.lib_path   = "";
    app_config1.properties = mc::dict{{"stream", "std_out"}};

    appender_config app_config2;
    app_config2.name       = "console2";
    app_config2.type       = "console";
    app_config2.lib_path   = "";
    app_config2.properties = mc::dict{{"stream", "std_error"}};

    config.appenders.push_back(app_config1);
    config.appenders.push_back(app_config2);

    // 创建两个 logger，每个都使用不同的 appender
    logger_config log_config1;
    log_config1.name      = "logger1";
    log_config1.level     = level::info;
    log_config1.appenders = {"console1"};

    logger_config log_config2;
    log_config2.name      = "logger2";
    log_config2.level     = level::debug;
    log_config2.appenders = {"console2"};

    config.loggers.push_back(log_config1);
    config.loggers.push_back(log_config2);

    // 应用配置
    bool result = manager.apply_config(config);
    ASSERT_TRUE(result);

    // 验证两个 logger 都已创建
    logger logger1 = manager.get_logger("logger1");
    logger logger2 = manager.get_logger("logger2");

    ASSERT_EQ(logger1.get_name(), "logger1");
    ASSERT_EQ(logger1.get_level(), level::info);
    ASSERT_EQ(logger2.get_name(), "logger2");
    ASSERT_EQ(logger2.get_level(), level::debug);
}

// 测试 logger 使用多个 appender
TEST_F(log_manager_test, LoggerWithMultipleAppenders) {
    log_manager& manager = log_manager::instance();

    logging_config config;

    // 创建两个 appender
    appender_config app_config1;
    app_config1.name       = "console_multi1";
    app_config1.type       = "console";
    app_config1.lib_path   = "";
    app_config1.properties = mc::dict{};

    appender_config app_config2;
    app_config2.name       = "console_multi2";
    app_config2.type       = "console";
    app_config2.lib_path   = "";
    app_config2.properties = mc::dict{};

    config.appenders.push_back(app_config1);
    config.appenders.push_back(app_config2);

    // 创建一个 logger 使用两个 appender
    logger_config log_config;
    log_config.name      = "multi_appender_logger";
    log_config.level     = level::info;
    log_config.appenders = {"console_multi1", "console_multi2"};

    config.loggers.push_back(log_config);

    // 应用配置
    bool result = manager.apply_config(config);
    ASSERT_TRUE(result);

    // 验证 logger 有两个 appender
    logger multi_logger = manager.get_logger("multi_appender_logger");
    ASSERT_EQ(multi_logger.get_appenders().size(), 2);
}
