#include <gtest/gtest.h>
#include <mc/log.h>
#include <mc/log/appender_factory.h>
#include <mc/log/appenders/file_appender.h>
#include <mc/log/appenders/socket_appender.h>

#include "log/builtin_appenders.h"
#include "log/logging_internal.h"

#include <filesystem>
#include <fstream>
#include <string>

using namespace mc::log;

namespace {

size_t count_appender_by_name(const mc::log::logger& log, std::string_view name)
{
    size_t count = 0;
    for (const auto& appender : log.get_appenders()) {
        if (appender && appender->get_name() == name) {
            ++count;
        }
    }
    return count;
}

std::string read_file_content(const std::filesystem::path& path)
{
    std::ifstream ifs(path);
    return std::string((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
}

} // namespace

TEST(builtin_appender_registration_test, register_builtin_appenders_registers_file_and_socket_without_side_effects)
{
    auto default_logger = mc::log::default_logger();
    default_logger.set_level(mc::log::level::info);
    const auto default_file_count_before = count_appender_by_name(default_logger, "default_file");

    register_builtin_appenders();

    EXPECT_EQ(default_logger.get_level(), mc::log::level::info);
    EXPECT_EQ(count_appender_by_name(default_logger, "default_file"), default_file_count_before);

    auto file = appender_factory::instance().create_by_type<file_appender>("file");
    auto socket = appender_factory::instance().create_by_type<socket_appender>("socket");

    ASSERT_NE(file, nullptr);
    ASSERT_NE(socket, nullptr);
}

TEST(builtin_appender_registration_test, bootstrap_default_logging_is_idempotent)
{
    auto default_logger = mc::log::default_logger();
    default_logger.set_level(mc::log::level::info);

    bootstrap_default_logging();
    bootstrap_default_logging();

    EXPECT_EQ(default_logger.get_level(), mc::log::level::info);
    EXPECT_EQ(count_appender_by_name(default_logger, "default_file"), 1u);
}

TEST(builtin_appender_registration_test, bootstrap_default_logging_makes_file_appender_effective)
{
    auto default_logger = mc::log::default_logger();
    default_logger.set_level(mc::log::level::info);

    auto log_path = std::filesystem::path(TEST_LOG_DIR) / "bootstrap_default_file.log";
    std::filesystem::create_directories(log_path.parent_path());
    std::filesystem::remove(log_path);
    logging::set_stub_log_path(log_path.string());

    bootstrap_default_logging();

    mc::log::message msg(mc::log::level::info, "bootstrap default file appender works",
                         mc::log::context("builtin_appenders_test.cpp", "bootstrap_test", 42));
    default_logger.log(msg);

    ASSERT_TRUE(std::filesystem::exists(log_path));
    EXPECT_NE(read_file_content(log_path).find("bootstrap default file appender works"), std::string::npos);
}
