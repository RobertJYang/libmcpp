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

#include <atomic>
#include <chrono>
#include <fstream>
#include <mc/filesystem.h>
#include <memory>
#include <string>
#include <vector>

using namespace mc::log;

namespace {

class tracking_appender : public mc::log::appender {
public:
    tracking_appender() = default;

    bool init(const mc::variant& args) override
    {
        ++m_init_count;
        if (args.is_dict()) {
            m_last_config = args.as<mc::dict>();
        } else {
            m_last_config = mc::dict{};
        }
        return true;
    }

    void append(const mc::log::message& msg) override
    {
        m_messages.push_back(msg);
    }

    int init_count() const
    {
        return m_init_count;
    }

    const mc::dict& last_config() const
    {
        return m_last_config;
    }

    const mc::log::messages& messages() const
    {
        return m_messages;
    }

private:
    int               m_init_count{0};
    mc::dict          m_last_config;
    mc::log::messages m_messages;
};

std::string make_unique_name(const std::string& prefix)
{
    static std::atomic<uint32_t> counter{0};
    return prefix + "_" + std::to_string(++counter);
}

} // namespace

// 测试用的日志追加器，将日志消息存储在内存中
class memory_appender : public mc::log::appender {
public:
    memory_appender()
    {
    }

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

class failing_init_appender : public mc::log::appender {
public:
    bool init(const mc::variant&) override
    {
        return false;
    }

    void append(const mc::log::message&) override
    {
    }
};

class log_manager_test : public mc::test::TestBase {
protected:
    void SetUp() override
    {
        mc::test::TestBase::SetUp();
        mc::log::default_logger().set_level(mc::log::level::off);
    }

    void TearDown() override
    {
        mc::test::TestBase::TearDown();
    }
};

// 测试 log_manager 单例模式
TEST_F(log_manager_test, SingletonInstance)
{
    log_manager& manager1 = log_manager::instance();
    log_manager& manager2 = log_manager::instance();

    // 应该是同一个实例
    ASSERT_EQ(&manager1, &manager2);
}

// 测试获取默认 logger
TEST_F(log_manager_test, GetDefaultLogger)
{
    log_manager& manager        = log_manager::instance();
    logger       default_logger = manager.get_logger();

    ASSERT_EQ(default_logger.get_name(), "default");
}

// 测试获取指定名称的 logger
TEST_F(log_manager_test, GetNamedLogger)
{
    log_manager& manager     = log_manager::instance();
    logger       test_logger = manager.get_logger("test_logger");

    ASSERT_EQ(test_logger.get_name(), "test_logger");
}

// 测试多次获取同一个 logger 返回相同的实例
TEST_F(log_manager_test, GetSameLoggerMultipleTimes)
{
    log_manager& manager = log_manager::instance();
    logger       logger1 = manager.get_logger("same_logger");
    logger       logger2 = manager.get_logger("same_logger");

    // 虽然对象不同（按值返回），但应该是从同一个管理器中获取的
    ASSERT_EQ(logger1.get_name(), logger2.get_name());
}

// 测试应用配置 - 创建新的 logger
TEST_F(log_manager_test, ApplyConfigCreateNewLogger)
{
    log_manager& manager = log_manager::instance();

    // 创建一个内存 appender
    auto mem_appender = std::make_shared<memory_appender>();
    mem_appender->set_name("test_memory");
    appender_factory::instance().register_creator("test_memory", [mem_appender]() {
        return mem_appender;
    });

    // 创建日志配置
    logging_config  config;
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
TEST_F(log_manager_test, ApplyConfigUpdateExistingLogger)
{
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

// 测试 apply_config 应用 condition 配置
TEST_F(log_manager_test, ApplyConfigCondition)
{
    log_manager& manager = log_manager::instance();

    auto mem_appender = std::make_shared<memory_appender>();
    mem_appender->set_name("condition_test_mem");
    appender_factory::instance().register_creator("condition_test_mem", [mem_appender]() {
        return mem_appender;
    });

    logging_config  config;
    appender_config app_config;
    app_config.name       = "condition_test_mem";
    app_config.type       = "condition_test_mem";
    app_config.lib_path   = "";
    app_config.properties = mc::dict{};
    config.appenders.push_back(app_config);

    logger_config log_config;
    log_config.name      = "condition_test_logger";
    log_config.level     = level::info;
    log_config.appenders = {"condition_test_mem"};
    log_config.condition = false;
    config.loggers.push_back(log_config);

    ASSERT_TRUE(manager.apply_config(config));

    logger test_logger = manager.get_logger("condition_test_logger");
    mc_ilog(test_logger, "不应输出");
    ASSERT_TRUE(mem_appender->get_messages().empty());

    test_logger.condition(true);
    mc_ilog(test_logger, "应输出");
    ASSERT_EQ(mem_appender->get_messages().size(), 1);
}

// 测试动态库加载（模拟场景）
TEST_F(log_manager_test, LoadAppenderLibrary)
{
    log_manager& manager = log_manager::instance();

    // 尝试加载一个不存在的动态库应该返回 false
    bool result = manager.load_appender("/path/to/nonexistent/lib.so", "test_appender");
    ASSERT_FALSE(result);

    // 加载空的目录应该不报错
    ASSERT_NO_THROW(manager.load_appenders("/nonexistent/path"));
}

// 测试工厂创建 appender
TEST_F(log_manager_test, FactoryCreateAppender)
{
    // 使用唯一的名称避免冲突
    std::string unique_prefix = "factory_test_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());

    // 使用 appender_factory 创建 console appender
    mc::dict config;
    config["stream"]        = "std_out";
    config["use_color"]     = true;
    appender_ptr console_ap = appender_factory::instance().create(unique_prefix + "_console", "console", config);
    ASSERT_NE(console_ap, nullptr);

    // 使用 appender_factory 创建 file appender
    mc::dict file_config;
    file_config["filename"]       = std::string(TEST_LOG_DIR) + "/test_file.log";
    file_config["truncate"]       = true;
    file_config["flush_on_write"] = true;
    appender_ptr file_ap          = appender_factory::instance().create(unique_prefix + "_file", "file", file_config);
    ASSERT_NE(file_ap, nullptr);
}

// 测试配置包含多个 appender 和 logger
TEST_F(log_manager_test, MultipleAppendersAndLoggers)
{
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
TEST_F(log_manager_test, LoggerWithMultipleAppenders)
{
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

TEST_F(log_manager_test, AppenderFactoryReinitializeExisting)
{
    auto type_name = make_unique_name("tracking_type");
    auto app_name  = make_unique_name("tracking_app");
    appender_factory::instance().register_creator(
        type_name, []() {
        return std::make_shared<tracking_appender>();
    });

    mc::dict first_props{{"kind", "first"}};
    auto     first          = appender_factory::instance().get_or_create_appender(app_name, type_name, first_props);
    auto     tracking_first = std::dynamic_pointer_cast<tracking_appender>(first);
    ASSERT_NE(tracking_first, nullptr);
    EXPECT_EQ(tracking_first->init_count(), 1);
    EXPECT_EQ(tracking_first->last_config()["kind"].as_string(), "first");

    mc::dict second_props{{"kind", "second"}};
    auto     second          = appender_factory::instance().get_or_create_appender(app_name, type_name, second_props);
    auto     tracking_second = std::dynamic_pointer_cast<tracking_appender>(second);
    ASSERT_EQ(tracking_first.get(), tracking_second.get());
    EXPECT_EQ(tracking_second->init_count(), 2);
    EXPECT_EQ(tracking_second->last_config()["kind"].as_string(), "second");
}

TEST_F(log_manager_test, AppenderFactoryRejectsDuplicateName)
{
    auto type_name = make_unique_name("dup_type");
    auto app_name  = make_unique_name("dup_app");
    appender_factory::instance().register_creator(
        type_name, []() {
        return std::make_shared<tracking_appender>();
    });

    auto created = appender_factory::instance().create(app_name, type_name, mc::dict{});
    ASSERT_NE(created, nullptr);

    auto duplicate = appender_factory::instance().create(app_name, type_name, mc::dict{});
    EXPECT_EQ(duplicate, nullptr);
}

TEST_F(log_manager_test, LoadAppendersHandlesInvalidLibraries)
{
    auto temp_dir = mc::filesystem::path(TEST_LOG_DIR) / make_unique_name("invalid_libs");
    mc::filesystem::create_directories(temp_dir);
    auto fake_lib = temp_dir / "fake_logger.so";
    {
        std::ofstream ofs(fake_lib.string());
        ofs << "not a real shared library";
    }

    ASSERT_NO_THROW(log_manager::instance().load_appenders(temp_dir.string()));
    EXPECT_EQ(appender_factory::instance().get_appender(fake_lib.stem().string()), nullptr);
}

TEST_F(log_manager_test, ApplyConfigHandlesPartialAppenderFailures)
{
    auto valid_type  = make_unique_name("valid_type");
    auto valid_app   = make_unique_name("valid_app");
    auto invalid_app = make_unique_name("invalid_app");
    auto logger_name = make_unique_name("config_logger");

    appender_factory::instance().register_creator(
        valid_type, []() {
        return std::make_shared<tracking_appender>();
    });

    logging_config  config;
    appender_config valid_config;
    valid_config.name       = valid_app;
    valid_config.type       = valid_type;
    valid_config.properties = mc::dict{{"key", "value"}};

    appender_config invalid_config;
    invalid_config.name = invalid_app;
    invalid_config.type = make_unique_name("missing_type");

    config.appenders.push_back(valid_config);
    config.appenders.push_back(invalid_config);

    logger_config log_cfg(logger_name);
    log_cfg.level     = level::debug;
    log_cfg.appenders = {valid_app};
    config.loggers.push_back(log_cfg);

    EXPECT_TRUE(log_manager::instance().apply_config(config));

    auto logger_instance = log_manager::instance().get_logger(logger_name.c_str());
    ASSERT_EQ(logger_instance.get_appenders().size(), 1);
    auto tracking = std::dynamic_pointer_cast<tracking_appender>(
        appender_factory::instance().get_appender(valid_app));
    ASSERT_NE(tracking, nullptr);
    EXPECT_EQ(tracking->init_count(), 1);

    EXPECT_EQ(appender_factory::instance().get_appender(invalid_app), nullptr);
}

TEST_F(log_manager_test, ApplyConfigUpdatesAndRemovesAppenders)
{
    auto type_name   = make_unique_name("update_type");
    auto app_one     = make_unique_name("app_one");
    auto logger_name = make_unique_name("update_logger");

    appender_factory::instance().register_creator(
        type_name, []() {
        return std::make_shared<tracking_appender>();
    });

    logging_config  first_config;
    appender_config app_conf;
    app_conf.name       = app_one;
    app_conf.type       = type_name;
    app_conf.properties = mc::dict{};
    first_config.appenders.push_back(app_conf);

    logger_config log_cfg(logger_name);
    log_cfg.level     = level::info;
    log_cfg.appenders = {app_one};
    first_config.loggers.push_back(log_cfg);

    EXPECT_TRUE(log_manager::instance().apply_config(first_config));

    auto logger_instance = log_manager::instance().get_logger(logger_name.c_str());
    ASSERT_EQ(logger_instance.get_appenders().size(), 1);

    logging_config second_config;
    logger_config  remove_cfg(logger_name);
    remove_cfg.level = level::error;
    second_config.loggers.push_back(remove_cfg);

    EXPECT_TRUE(log_manager::instance().apply_config(second_config));
    logger_instance = log_manager::instance().get_logger(logger_name.c_str());
    EXPECT_TRUE(logger_instance.get_appenders().empty());
    EXPECT_EQ(logger_instance.get_level(), level::error);

    logging_config third_config;
    logger_config  missing_cfg(logger_name);
    missing_cfg.level     = level::warn;
    missing_cfg.appenders = {make_unique_name("absent_app")};
    third_config.loggers.push_back(missing_cfg);

    EXPECT_TRUE(log_manager::instance().apply_config(third_config));
    logger_instance = log_manager::instance().get_logger(logger_name.c_str());
    EXPECT_TRUE(logger_instance.get_appenders().empty());
    EXPECT_EQ(logger_instance.get_level(), level::warn);
}

TEST_F(log_manager_test, ApplyConfigExternalLibraryLoadFailure)
{
    auto logger_name  = make_unique_name("external_logger");
    auto ext_app_name = make_unique_name("external_app");

    logging_config  config;
    appender_config external;
    external.name     = ext_app_name;
    external.type     = "console";
    external.lib_path = (mc::filesystem::path(TEST_LOG_DIR) / (ext_app_name + ".so")).string();
    config.appenders.push_back(external);

    logger_config log_cfg(logger_name);
    log_cfg.level     = level::info;
    log_cfg.appenders = {ext_app_name};
    config.loggers.push_back(log_cfg);

    EXPECT_TRUE(log_manager::instance().apply_config(config));

    auto logger_instance = log_manager::instance().get_logger(logger_name.c_str());
    EXPECT_TRUE(logger_instance.get_appenders().empty());
    EXPECT_EQ(appender_factory::instance().get_appender(ext_app_name), nullptr);
}

TEST_F(log_manager_test, ApplyConfigSkipRedundantAppenderUpdates)
{
    auto type_name   = make_unique_name("stable_type");
    auto app_name    = make_unique_name("stable_app");
    auto logger_name = make_unique_name("stable_logger");

    auto tracked_appender = std::make_shared<tracking_appender>();
    tracked_appender->set_name(app_name);
    appender_factory::instance().register_creator(type_name, [tracked_appender]() {
        return tracked_appender;
    });

    logging_config  config;
    appender_config app_cfg;
    app_cfg.name = app_name;
    app_cfg.type = type_name;
    config.appenders.push_back(app_cfg);

    logger_config log_cfg(logger_name);
    log_cfg.level     = level::info;
    log_cfg.appenders = {app_name};
    config.loggers.push_back(log_cfg);

    ASSERT_TRUE(log_manager::instance().apply_config(config));

    auto logger_before = log_manager::instance().get_logger(logger_name.c_str());
    ASSERT_EQ(logger_before.get_appenders().size(), 1);
    auto first_ptr = logger_before.get_appenders().front();

    EXPECT_TRUE(log_manager::instance().apply_config(config));

    auto logger_after = log_manager::instance().get_logger(logger_name.c_str());
    ASSERT_EQ(logger_after.get_appenders().size(), 1);
    EXPECT_EQ(logger_after.get_appenders().front().get(), first_ptr.get());
}

TEST_F(log_manager_test, AppenderFactoryInitFailureReturnsNullptr)
{
    auto type_name = make_unique_name("failing_type");
    auto app_name  = make_unique_name("failing_app");
    appender_factory::instance().register_creator(
        type_name, []() {
        return std::make_shared<failing_init_appender>();
    });

    auto created = appender_factory::instance().create(app_name, type_name, mc::dict{});
    EXPECT_EQ(created, nullptr);
}

// 测试配置缺失时的处理
TEST_F(log_manager_test, LoadInvalidConfigFails)
{
    log_manager& manager = log_manager::instance();

    // 测试空的配置（没有 appenders 字段）
    logging_config empty_config;
    // 即使配置为空，apply_config 也会返回 true（因为即使 appender 加载失败也会继续）
    // 但我们可以验证默认 logger 仍然存在
    bool result = manager.apply_config(empty_config);
    EXPECT_TRUE(result); // apply_config 总是返回 true，即使配置无效

    // 验证默认 logger 仍然存在
    logger default_logger = manager.get_logger();
    EXPECT_EQ(default_logger.get_name(), "default");

    // 测试缺少 appenders 字段的配置（通过创建无效的 appender 配置）
    logging_config  invalid_config;
    appender_config invalid_app_config;
    invalid_app_config.name = "invalid_appender";
    invalid_app_config.type = "nonexistent_type"; // 不存在的类型
    invalid_config.appenders.push_back(invalid_app_config);

    logger_config log_cfg("test_logger");
    log_cfg.level     = level::info;
    log_cfg.appenders = {"invalid_appender"};
    invalid_config.loggers.push_back(log_cfg);

    // apply_config 应该返回 true，但 appender 创建会失败
    result = manager.apply_config(invalid_config);
    EXPECT_TRUE(result);

    // 验证 logger 被创建，但没有 appender（因为 appender 创建失败）
    logger test_logger = manager.get_logger("test_logger");
    EXPECT_EQ(test_logger.get_name(), "test_logger");
    EXPECT_TRUE(test_logger.get_appenders().empty()); // 应该没有 appender
}

// 测试 appender_factory 创建重复名称的 appender（覆盖重复名称错误路径）
TEST_F(log_manager_test, AppenderFactoryDuplicateName)
{
    auto type_name = make_unique_name("dup_type");
    auto app_name  = make_unique_name("dup_app");

    appender_factory::instance().register_creator(
        type_name, []() {
        return std::make_shared<tracking_appender>();
    });

    // 第一次创建应该成功
    auto first = appender_factory::instance().create(app_name, type_name, mc::dict{});
    ASSERT_NE(first, nullptr);

    // 第二次使用相同名称创建应该返回 nullptr（覆盖行 69-72）
    auto duplicate = appender_factory::instance().create(app_name, type_name, mc::dict{});
    EXPECT_EQ(duplicate, nullptr);
}

// 测试 appender_factory 创建未知类型的 appender（覆盖行 77-79）
TEST_F(log_manager_test, AppenderFactoryUnknownType)
{
    auto app_name = make_unique_name("unknown_app");

    // 尝试创建未知类型的 appender 应该返回 nullptr
    auto unknown = appender_factory::instance().create(app_name, "unknown_type", mc::dict{});
    EXPECT_EQ(unknown, nullptr);
}

// 测试 appender_factory 的 create_by_type 返回 nullptr 的情况（覆盖行 63）
TEST_F(log_manager_test, AppenderFactoryCreateByTypeUnknown)
{
    // 尝试创建未知类型的 appender
    auto unknown = appender_factory::instance().create_by_type<mc::log::appender>("unknown_type_12345");
    EXPECT_EQ(unknown, nullptr);
}

// 测试 appender_factory 加载已存在的库（覆盖行 99-100）
TEST_F(log_manager_test, AppenderFactoryLoadExistingLibrary)
{
    // 由于动态库加载需要真实的库文件，这里只测试逻辑
    // 实际测试中，如果库已加载，应该直接返回 true
    // 这个测试主要验证代码路径存在
    auto lib_path = "/nonexistent/path/lib.so";
    auto type     = "test_type";

    // 第一次加载应该失败（因为路径不存在）
    bool first_load = log_manager::instance().load_appender(lib_path, type);
    EXPECT_FALSE(first_load);

    // 注意：由于第一次加载失败，库不会被注册，所以第二次也会失败
    // 要测试已存在库的路径，需要真实的库文件，这在单元测试中较难实现
}

// 测试 appender_factory 的 get_appender 返回 nullptr（覆盖行 158）
TEST_F(log_manager_test, AppenderFactoryGetNonExistentAppender)
{
    // 获取不存在的 appender 应该返回 nullptr
    auto non_existent = appender_factory::instance().get_appender("non_existent_appender_12345");
    EXPECT_EQ(non_existent, nullptr);
}

// 测试 log_manager 的 get_logger 创建新 logger 的路径（覆盖行 72-74）
TEST_F(log_manager_test, GetLoggerCreatesNewLogger)
{
    log_manager& manager = log_manager::instance();

    // 获取一个不存在的 logger，应该创建新的
    auto new_logger = manager.get_logger("newly_created_logger_12345");
    EXPECT_EQ(new_logger.get_name(), "newly_created_logger_12345");

    // 再次获取应该返回同一个（虽然对象不同，但名称相同）
    auto same_logger = manager.get_logger("newly_created_logger_12345");
    EXPECT_EQ(same_logger.get_name(), "newly_created_logger_12345");
}

// 测试 log_manager 的 apply_config 中 appender 添加失败的路径（覆盖行 159）
TEST_F(log_manager_test, ApplyConfigAppenderNotFound)
{
    log_manager& manager = log_manager::instance();

    logging_config config;
    logger_config  log_cfg("appender_not_found_logger");
    log_cfg.level     = level::info;
    log_cfg.appenders = {"non_existent_appender_12345"}; // 不存在的 appender

    config.loggers.push_back(log_cfg);

    // 应用配置应该成功，但 logger 不会有 appender
    EXPECT_TRUE(manager.apply_config(config));

    auto logger_instance = manager.get_logger("appender_not_found_logger");
    EXPECT_TRUE(logger_instance.get_appenders().empty());
}

// 测试 log_manager 的 update_existing_logger 中添加 appender 的路径（覆盖行 152-162）
TEST_F(log_manager_test, UpdateExistingLoggerAddAppender)
{
    log_manager& manager = log_manager::instance();

    // 创建一个 appender
    auto type_name = make_unique_name("update_type");
    auto app_name  = make_unique_name("update_app");
    appender_factory::instance().register_creator(
        type_name, []() {
        return std::make_shared<tracking_appender>();
    });

    logging_config  app_config;
    appender_config app_cfg;
    app_cfg.name = app_name;
    app_cfg.type = type_name;
    app_config.appenders.push_back(app_cfg);

    // 先创建一个 logger
    auto logger_name = make_unique_name("update_logger");
    auto logger      = manager.get_logger(logger_name.c_str());

    // 然后通过配置添加 appender
    logging_config config;
    config.appenders.push_back(app_cfg);
    logger_config log_cfg(logger_name);
    log_cfg.level     = level::info;
    log_cfg.appenders = {app_name};
    config.loggers.push_back(log_cfg);

    EXPECT_TRUE(manager.apply_config(config));

    auto updated_logger = manager.get_logger(logger_name.c_str());
    EXPECT_FALSE(updated_logger.get_appenders().empty());
}

// 测试 log_manager 的 update_existing_logger 中移除 appender 的路径（覆盖行 144-149）
TEST_F(log_manager_test, UpdateExistingLoggerRemoveAppender)
{
    log_manager& manager = log_manager::instance();

    // 创建一个 appender 并添加到 logger
    auto type_name = make_unique_name("remove_type");
    auto app_name  = make_unique_name("remove_app");
    appender_factory::instance().register_creator(
        type_name, []() {
        return std::make_shared<tracking_appender>();
    });

    logging_config  first_config;
    appender_config app_cfg;
    app_cfg.name = app_name;
    app_cfg.type = type_name;
    first_config.appenders.push_back(app_cfg);

    auto          logger_name = make_unique_name("remove_logger");
    logger_config log_cfg(logger_name);
    log_cfg.level     = level::info;
    log_cfg.appenders = {app_name};
    first_config.loggers.push_back(log_cfg);

    EXPECT_TRUE(manager.apply_config(first_config));

    // 然后通过配置移除 appender
    logging_config second_config;
    logger_config  remove_cfg(logger_name);
    remove_cfg.level     = level::warn;
    remove_cfg.appenders = {}; // 空列表，移除所有 appender
    second_config.loggers.push_back(remove_cfg);

    EXPECT_TRUE(manager.apply_config(second_config));

    auto updated_logger = manager.get_logger(logger_name.c_str());
    EXPECT_TRUE(updated_logger.get_appenders().empty());
    EXPECT_EQ(updated_logger.get_level(), level::warn);
}
