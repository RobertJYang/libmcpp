/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
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
#include <mc/app/app_proto.h>
#include <mc/app/application.h>
#include <mc/app/micro_component.h>
#include <mc/app/service.h>
#include <mc/array.h>
#include <mc/dbus/connection.h>
#include <mc/dbus/message.h>
#include <mc/engine.h>
#include <mc/engine/payload.h>
#include <mc/engine/proxy.h>
#include <mc/engine/service.h>
#include <mc/engine/service_proto.h>
#include <mc/filesystem.h>
#include <mc/json.h>
#include <mc/log/appender.h>
#include <mc/log/appender_factory.h>
#include <mc/log/log_manager.h>
#include <mc/reflect.h>
#include <mc/runtime/runtime_context.h>
#if MCENGINE_USE_SHM
#include <mc/shm/default_runtime.h>
#include <mc/shm/detail/shared_memory_backend.h>
#endif
#include <mc/signal/connection.h>
#include <test_utilities/base.h>
#include <test_utilities/test_base.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <tuple>
#include <unistd.h>
#include <vector>

namespace {

#if defined(__APPLE__)
constexpr mc::string_view k_builtin_file_appender_libname{"file_appender.dylib"};
constexpr mc::string_view k_builtin_socket_appender_libname{"socket_appender.dylib"};
constexpr mc::string_view k_test_file_appender_plugin_libname{"mcapp_test_file_appender_plugin.dylib"};
#else
constexpr mc::string_view k_builtin_file_appender_libname{"file_appender.so"};
constexpr mc::string_view k_builtin_socket_appender_libname{"socket_appender.so"};
constexpr mc::string_view k_test_file_appender_plugin_libname{"mcapp_test_file_appender_plugin.so"};
#endif

class memory_capture_appender : public mc::log::appender {
public:
    bool init(const mc::variant& /*args*/) override
    {
        return true;
    }

    void append(const mc::log::message& msg) override
    {
        m_lines.push_back(mc::string(msg.get_message()));
    }

    void clear_lines()
    {
        m_lines.clear();
    }

    bool any_line_contains_both(mc::string_view needle_a, mc::string_view needle_b) const
    {
        for (const mc::string& line : m_lines) {
            if (line.find(needle_a) != mc::string::npos && line.find(needle_b) != mc::string::npos) {
                return true;
            }
        }
        return false;
    }

private:
    std::vector<mc::string> m_lines;
};

mc::filesystem::path make_scratch_appenders_directory_token(mc::string_view unique_marker)
{
    using namespace std::chrono;
    const auto  ms   = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    std::string name = "mcapp_ut_ad_";
    name.append(unique_marker.data(), unique_marker.size());
    name.push_back('_');
    name += std::to_string(::getpid());
    name.push_back('_');
    name += std::to_string(static_cast<long long>(ms));
    return mc::filesystem::temp_directory_path() / name;
}

struct scratch_appenders_directory_guard {
    explicit scratch_appenders_directory_guard(mc::filesystem::path d) : dir(std::move(d))
    {}

    ~scratch_appenders_directory_guard()
    {
        (void)mc::filesystem::remove_all(dir);
    }

    mc::filesystem::path dir;
};

const char* resolve_global_build_root_with_candidate(const std::string& relative_candidate)
{
    const char* global_root = std::getenv("MC_GLOBAL_BUILD_ROOT");
    if (global_root != nullptr) {
        mc::filesystem::path candidate = mc::filesystem::path(global_root) / relative_candidate;
        if (mc::filesystem::exists(candidate)) {
            return global_root;
        }
    }

    const char* project_root = std::getenv("MC_BUILD_ROOT");
    if (project_root != nullptr) {
        mc::filesystem::path candidate = mc::filesystem::path(project_root) / relative_candidate;
        if (mc::filesystem::exists(candidate)) {
            return project_root;
        }
    }

    return nullptr;
}

bool populate_appenders_scratch_dir_with_file_plugin(mc::filesystem::path dir)
{
    const char* build_root = resolve_global_build_root_with_candidate("libraries/log_appenders/" +
                                                                      std::string(k_builtin_file_appender_libname));
    if (build_root == nullptr) {
        ADD_FAILURE() << "cannot resolve libraries/log_appenders plugin (MC_GLOBAL_BUILD_ROOT / MC_BUILD_ROOT)";
        return false;
    }
    mc::filesystem::path plugin =
        mc::filesystem::path(build_root) / "libraries" / "log_appenders" / std::string(k_builtin_file_appender_libname);
    if (!mc::filesystem::exists(plugin)) {
        ADD_FAILURE() << "missing plugin " << plugin.string();
        return false;
    }
    if (!mc::filesystem::exists(dir) && !mc::filesystem::create_directories(dir)) {
        return false;
    }
    mc::filesystem::path link = dir / std::string(k_builtin_file_appender_libname);
    return mc::filesystem::create_symlink(plugin, link);
}

bool populate_appenders_scratch_dir_with_test_file_plugin(mc::filesystem::path dir)
{
    const char* build_root = resolve_global_build_root_with_candidate("libraries/mcapp/tests/" +
                                                                      std::string(k_test_file_appender_plugin_libname));
    if (build_root == nullptr) {
        ADD_FAILURE() << "cannot resolve mcapp test file appender plugin (MC_GLOBAL_BUILD_ROOT / MC_BUILD_ROOT)";
        return false;
    }

    mc::filesystem::path plugin = mc::filesystem::path(build_root) / "libraries" / "mcapp" / "tests" /
                                  std::string(k_test_file_appender_plugin_libname);
    if (!mc::filesystem::exists(plugin)) {
        ADD_FAILURE() << "missing plugin " << plugin.string();
        return false;
    }
    if (!mc::filesystem::exists(dir) && !mc::filesystem::create_directories(dir)) {
        return false;
    }

    mc::filesystem::path link = dir / std::string(k_builtin_file_appender_libname);
    return mc::filesystem::create_symlink(plugin, link);
}

bool populate_appenders_scratch_dir_with_socket_plugin(mc::filesystem::path dir)
{
    const char* build_root = resolve_global_build_root_with_candidate("libraries/log_appenders/" +
                                                                      std::string(k_builtin_socket_appender_libname));
    if (build_root == nullptr) {
        ADD_FAILURE() << "cannot resolve libraries/log_appenders socket plugin (MC_GLOBAL_BUILD_ROOT / MC_BUILD_ROOT)";
        return false;
    }

    mc::filesystem::path plugin = mc::filesystem::path(build_root) / "libraries" / "log_appenders" /
                                  std::string(k_builtin_socket_appender_libname);
    if (!mc::filesystem::exists(plugin)) {
        ADD_FAILURE() << "missing plugin " << plugin.string();
        return false;
    }
    if (!mc::filesystem::exists(dir) && !mc::filesystem::create_directories(dir)) {
        return false;
    }

    mc::filesystem::path link = dir / std::string(k_builtin_socket_appender_libname);
    return mc::filesystem::create_symlink(plugin, link);
}

std::string read_file_contents(const mc::filesystem::path& file_path)
{
    std::ifstream file(file_path);
    if (!file.is_open()) {
        return {};
    }

    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

struct default_logger_memory_capture_guard {
    explicit default_logger_memory_capture_guard(std::shared_ptr<memory_capture_appender> sink)
        : m_sink(std::move(sink))
    {
        m_sink->set_name("mcapp_ut_default_logger_capture");
        m_sink->clear_lines();
        mc::log::appender_ptr as_base = std::static_pointer_cast<mc::log::appender>(m_sink);
        mc::log::log_manager::instance().get_logger().add_appender(as_base);
    }

    ~default_logger_memory_capture_guard()
    {
        mc::log::log_manager::instance().get_logger().remove_appender("mcapp_ut_default_logger_capture");
        m_sink->clear_lines();
    }

    std::shared_ptr<memory_capture_appender> m_sink;
};

constexpr mc::string_view k_default_file_appender_create_failed_hint{
    "mcapp create default file appender[default_file] failed"};

} // namespace

namespace test_mcapp {

struct sample_service_config {
    MC_REFLECTABLE("mc.test.SampleServiceConfig")

    std::string greeting{"default-greeting"};
};

class sample_service : public mc::app::service {
public:
    using config_type = sample_service_config;

    explicit sample_service(std::string name) : mc::app::service(std::move(name))
    {}

    bool on_configure() override
    {
        mc::from_variant(properties(), m_config);
        return true;
    }

    bool on_start() override
    {
        ++m_start_count;
        return true;
    }

    bool on_stop() override
    {
        ++m_stop_count;
        return true;
    }

    sample_service_config m_config;
    int                   m_start_count{0};
    int                   m_stop_count{0};
};

class echo_interface : public mc::engine::interface<echo_interface> {
public:
    MC_INTERFACE("org.test.application.Echo")

    mc::string m_greeting{"hello"};
    int32_t    m_counter{0};

    mc::string say(const mc::string& who)
    {
        ++m_counter;
        return m_greeting + ":" + who;
    }

    mc::signal<void(mc::string)> greeted;
};

class echo_object : public mc::engine::object<echo_object> {
public:
    MC_OBJECT(echo_object, "EchoObject", "/mc/test/application/echo", (echo_interface))

    echo_interface m_iface;
};

class echo_service : public mc::app::service {
public:
    explicit echo_service(std::string name) : mc::app::service(std::move(name))
    {}

    mc::shared_ptr<echo_object> echo_obj() const noexcept
    {
        return m_echo;
    }

protected:
    bool on_start() override
    {
        m_echo = mc::make_shared<echo_object>();
        register_object(m_echo);
        return true;
    }

    bool on_stop() override
    {
        if (m_echo) {
            unregister_object(m_echo);
            m_echo.reset();
        }
        return true;
    }

private:
    mc::shared_ptr<echo_object> m_echo;
};

} // namespace test_mcapp

MC_REFLECT(test_mcapp::sample_service_config, (greeting))
MC_REFLECT(test_mcapp::echo_interface, ((m_greeting, "Greeting"))((m_counter, "Counter"))((say, "Say"))(greeted))
MC_REFLECT(test_mcapp::echo_object, ((m_iface, "Iface")))

class application_test : public mc::test::TestWithDbusDaemon {
protected:
    void SetUp() override
    {
#if MCENGINE_USE_SHM
        mc::shm::shutdown_default_runtime();
        mc::shm::detail::shared_memory_backend::remove(k_default_shm_region);
#endif
        mc::engine::engine::reset_for_test();
        mc::app::base_app::reset_for_test();
        // 先移除 logger 持有的插件 appender，再重置工厂卸载 so，避免悬空虚表。
        mc::log::default_logger().clear_appenders();
        mc::log::appender_factory::reset_for_test();
        ::unsetenv("MCAPP_LOG_APPENDERS_DIR");
        ::unsetenv("MCAPP_TEST_DEFAULT_FILE_LOG_PATH");
        m_app = std::make_unique<mc::app::application>();
        m_app->registry().register_service<test_mcapp::sample_service>("sample_service");
        m_app->registry().register_service<test_mcapp::echo_service>("echo_service");
    }

    void TearDown() override
    {
        if (m_app) {
            m_app->stop();
        }
        m_app.reset();
        // 先移除 logger 持有的插件 appender，再重置工厂卸载 so，避免悬空虚表。
        mc::log::default_logger().clear_appenders();
        mc::log::appender_factory::reset_for_test();
        mc::app::base_app::reset_for_test();
        mc::engine::engine::reset_for_test();
        ::unsetenv("MCAPP_LOG_APPENDERS_DIR");
        ::unsetenv("MCAPP_TEST_DEFAULT_FILE_LOG_PATH");
#if MCENGINE_USE_SHM
        mc::shm::shutdown_default_runtime();
        mc::shm::detail::shared_memory_backend::remove(k_default_shm_region);
#endif
    }

#if MCENGINE_USE_SHM
    static constexpr mc::string_view k_default_shm_region{"mc.default"};
#endif

    mc::app::service_definition make_sample_service_definition(mc::string_view service_name = "mc.test.alpha")
    {
        mc::app::service_definition definition;
        definition.name    = mc::string(service_name);
        definition.type    = "sample_service";
        definition.enabled = true;
        return definition;
    }

    bool initialize_with_single_service(mc::app::service_definition definition, mc::string_view app_name = "sample-app",
                                        std::size_t io_threads = 1, std::size_t work_threads = 1)
    {
        mc::app::service_plan plan;
        plan.application.name         = mc::string(app_name);
        plan.application.io_threads   = io_threads;
        plan.application.work_threads = work_threads;
        plan.services.push_back(std::move(definition));
        return m_app->initialize_with_plan(std::move(plan));
    }

    bool initialize_from_config_file(const mc::filesystem::path& config_path)
    {
        std::vector<std::string> argv_storage{"mcapp_test", "--config", config_path.string()};
        std::vector<char*>       argv;
        argv.reserve(argv_storage.size());
        for (auto& arg : argv_storage) {
            argv.push_back(arg.data());
        }

        mc::app::app_options options;
        options.argc = static_cast<int>(argv.size());
        options.argv = argv.data();
        return m_app->initialize(options);
    }

    mc::shared_ptr<test_mcapp::echo_service>
    start_echo_service(mc::string_view service_name = "mc.test.application.echo")
    {
        mc::app::service_definition definition;
        definition.name    = mc::string(service_name);
        definition.type    = "echo_service";
        definition.enabled = true;

        EXPECT_TRUE(initialize_with_single_service(std::move(definition), "mcapp_dbus_test", 1, 1));
        EXPECT_TRUE(m_app->start());

        return mc::static_pointer_cast<test_mcapp::echo_service>(m_app->get_service(service_name));
    }

    static mc::dbus::message make_call(mc::string_view destination, mc::string_view interface, mc::string_view member)
    {
        return mc::dbus::message::new_method_call(destination, mc::string_view("/mc/test/application/echo"), interface,
                                                  member);
    }

    static mc::dbus::message call_sync(mc::dbus::connection& conn, mc::dbus::message call)
    {
        auto future = conn.async_send_with_reply(std::move(call), mc::seconds(5));
        return std::move(future).get();
    }

    std::unique_ptr<mc::app::application> m_app;
};

TEST_F(application_test, default_logging_end_to_end_loads_appenders_and_writes_default_file_log)
{
    scratch_appenders_directory_guard dir_guard(make_scratch_appenders_directory_token("e2e_default_file"));
    ASSERT_TRUE(populate_appenders_scratch_dir_with_test_file_plugin(dir_guard.dir));
    ASSERT_TRUE(populate_appenders_scratch_dir_with_socket_plugin(dir_guard.dir));

    auto log_file = dir_guard.dir / "default_file_output.log";
    ASSERT_EQ(::setenv("MCAPP_LOG_APPENDERS_DIR", dir_guard.dir.string().c_str(), 1), 0);
    ASSERT_EQ(::setenv("MCAPP_TEST_DEFAULT_FILE_LOG_PATH", log_file.string().c_str(), 1), 0);

    auto default_log = mc::log::default_logger();
    default_log.remove_appender("default_file");
    ASSERT_EQ(default_log.find_appender("default_file"), nullptr);
    ASSERT_EQ(mc::log::appender_factory::instance().get_appender("default_file"), nullptr);
    default_log.set_level(mc::log::level::info);

    ASSERT_TRUE(initialize_with_single_service(make_sample_service_definition("mc.test.e2e.default_file"),
                                               "mcapp_default_file_e2e"));

    ASSERT_NE(default_log.find_appender("default_file"), nullptr);

    mc::dict socket_args{{"path", mc::string("/tmp/mcapp_ut_e2e.sock")},
                         {"hb_path", mc::string("/tmp/mcapp_ut_e2e.hb")},
                         {"auto_connect", false}};
    auto     socket_probe = mc::log::appender_factory::instance().create("ut_socket_probe_e2e", "socket", socket_args);
    ASSERT_NE(socket_probe, nullptr);

    const std::string marker = "mcapp default file e2e marker " + std::to_string(::getpid());
    ilog("${marker}", ("marker", marker));

    ASSERT_TRUE(mc::filesystem::exists(log_file)) << log_file.string();
    EXPECT_NE(read_file_contents(log_file).find(marker), std::string::npos);

    default_log.remove_appender("default_file");
}

TEST_F(application_test, default_logging_scans_env_appenders_directory)
{
    scratch_appenders_directory_guard dir_guard(make_scratch_appenders_directory_token("scan"));
    ASSERT_TRUE(populate_appenders_scratch_dir_with_file_plugin(dir_guard.dir));

    ASSERT_EQ(::setenv("MCAPP_LOG_APPENDERS_DIR", dir_guard.dir.string().c_str(), 1), 0);

    ASSERT_TRUE(initialize_with_single_service(make_sample_service_definition()));

    auto file_app = mc::log::appender_factory::instance().create("mcapp_ut_file_probe", "file", mc::dict{});
    ASSERT_NE(file_app, nullptr);
}

TEST_F(application_test, default_logging_attaches_default_file_appender_when_logging_omitted)
{
    scratch_appenders_directory_guard dir_guard(make_scratch_appenders_directory_token("default"));
    ASSERT_TRUE(populate_appenders_scratch_dir_with_file_plugin(dir_guard.dir));
    ASSERT_EQ(::setenv("MCAPP_LOG_APPENDERS_DIR", dir_guard.dir.string().c_str(), 1), 0);

    auto default_log = mc::log::default_logger();
    default_log.remove_appender("default_file");
    ASSERT_EQ(default_log.find_appender("default_file"), nullptr);

    ASSERT_TRUE(initialize_with_single_service(make_sample_service_definition()));

    EXPECT_NE(default_log.find_appender("default_file"), nullptr);

    default_log.remove_appender("default_file");
}

TEST_F(application_test, default_logging_load_failure_logged_when_missing_directory_and_no_explicit_logging)
{
    const std::string                   marker = "badappenders_marker_" + std::to_string(::getpid());
    auto                                sink   = std::make_shared<memory_capture_appender>();
    default_logger_memory_capture_guard log_guard(sink);

    mc::filesystem::path bad_appenders_root =
        mc::filesystem::temp_directory_path() /
        mc::filesystem::path("mcapp_missing_ad_root_" + std::to_string(::getpid()));
    mc::filesystem::path bad_appenders_dir = bad_appenders_root / marker;
    ASSERT_FALSE(mc::filesystem::exists(bad_appenders_dir));

    ASSERT_EQ(::setenv("MCAPP_LOG_APPENDERS_DIR", bad_appenders_dir.string().c_str(), 1), 0);
    ASSERT_TRUE(initialize_with_single_service(make_sample_service_definition()));
    EXPECT_TRUE(sink->any_line_contains_both(k_default_file_appender_create_failed_hint, "default_file"));
}

TEST_F(application_test, default_logging_skip_avoids_builtin_appenders_probe_when_explicit_logging_marker)
{
    const std::string                   marker = "badappenders_skip_marker_" + std::to_string(::getpid());
    auto                                sink   = std::make_shared<memory_capture_appender>();
    default_logger_memory_capture_guard log_guard(sink);

    mc::filesystem::path bad_appenders_root =
        mc::filesystem::temp_directory_path() /
        mc::filesystem::path("mcapp_missing_ad_skip_root_" + std::to_string(::getpid()));
    mc::filesystem::path bad_appenders_dir = bad_appenders_root / marker;
    ASSERT_FALSE(mc::filesystem::exists(bad_appenders_dir));

    ASSERT_EQ(::setenv("MCAPP_LOG_APPENDERS_DIR", bad_appenders_dir.string().c_str(), 1), 0);

    auto                  definition = make_sample_service_definition();
    mc::app::service_plan plan;
    plan.application.name         = mc::string("logging-skipped-app");
    plan.application.io_threads   = 1;
    plan.application.work_threads = 1;
    plan.has_logging_config       = true;
    plan.services.push_back(std::move(definition));

    ASSERT_TRUE(m_app->initialize_with_plan(std::move(plan)));
    EXPECT_FALSE(sink->any_line_contains_both(k_default_file_appender_create_failed_hint, "default_file"));
}

TEST_F(application_test, explicit_logging_without_appender_dirs_still_loads_builtin_file_appender)
{
    scratch_appenders_directory_guard dir_guard(make_scratch_appenders_directory_token("explicit_logging"));
    ASSERT_TRUE(populate_appenders_scratch_dir_with_file_plugin(dir_guard.dir));
    ASSERT_EQ(::setenv("MCAPP_LOG_APPENDERS_DIR", dir_guard.dir.string().c_str(), 1), 0);

    auto          config_path = dir_guard.dir / "config.json";
    std::ofstream config_file(config_path);
    ASSERT_TRUE(config_file.is_open()) << config_path.string();
    config_file << R"JSON([
  {
    "api_version": "v1",
    "kind": "Application",
    "meta": {
      "name": "mcapp_explicit_logging_app"
    }
  },
  {
    "api_version": "v1",
    "kind": "Service",
    "type": "sample_service",
    "meta": {
      "name": "mc.test.explicit.logging"
    },
    "enabled": true
  },
  {
    "api_version": "v1",
    "kind": "Logging",
    "appenders": [
      {
        "name": "explicit_file",
        "type": "file",
        "properties": {
          "module_name": "mcapp_explicit_logging"
        }
      }
    ],
    "loggers": [
      {
        "name": "default",
        "level": "notice",
        "appenders": ["explicit_file"]
      }
    ]
  }
])JSON";
    config_file.close();

    auto default_log = mc::log::default_logger();
    default_log.remove_appender("default_file");
    ASSERT_EQ(default_log.find_appender("default_file"), nullptr);

    ASSERT_TRUE(initialize_from_config_file(config_path));

    EXPECT_EQ(default_log.find_appender("default_file"), nullptr);
    EXPECT_NE(default_log.find_appender("explicit_file"), nullptr);
    EXPECT_NE(mc::log::appender_factory::instance().get_appender("explicit_file"), nullptr);
}

TEST_F(application_test, explicit_logging_with_empty_appender_dirs_does_not_load_default_dir)
{
    scratch_appenders_directory_guard dir_guard(make_scratch_appenders_directory_token("explicit_logging_empty_dirs"));
    ASSERT_TRUE(populate_appenders_scratch_dir_with_file_plugin(dir_guard.dir));
    ASSERT_EQ(::setenv("MCAPP_LOG_APPENDERS_DIR", dir_guard.dir.string().c_str(), 1), 0);

    auto          config_path = dir_guard.dir / "config.json";
    std::ofstream config_file(config_path);
    ASSERT_TRUE(config_file.is_open()) << config_path.string();
    config_file << R"JSON([
  {
    "api_version": "v1",
    "kind": "Application",
    "meta": {
      "name": "mcapp_explicit_logging_empty_dirs_app"
    }
  },
  {
    "api_version": "v1",
    "kind": "Service",
    "type": "sample_service",
    "meta": {
      "name": "mc.test.explicit.logging.empty_dirs"
    },
    "enabled": true
  },
  {
    "api_version": "v1",
    "kind": "Logging",
    "appender_dirs": [],
    "appenders": [
      {
        "name": "explicit_file_empty_dirs",
        "type": "file",
        "properties": {
          "module_name": "mcapp_explicit_logging_empty_dirs"
        }
      }
    ],
    "loggers": [
      {
        "name": "default",
        "level": "notice",
        "appenders": ["explicit_file_empty_dirs"]
      }
    ]
  }
])JSON";
    config_file.close();

    auto default_log = mc::log::default_logger();
    default_log.remove_appender("default_file");
    ASSERT_EQ(default_log.find_appender("default_file"), nullptr);

    ASSERT_TRUE(initialize_from_config_file(config_path));

    EXPECT_EQ(default_log.find_appender("default_file"), nullptr);
    EXPECT_EQ(default_log.find_appender("explicit_file_empty_dirs"), nullptr);
    EXPECT_EQ(mc::log::appender_factory::instance().get_appender("explicit_file_empty_dirs"), nullptr);
}

TEST_F(application_test, explicit_logging_with_explicit_appender_dirs_does_not_merge_default_dir)
{
    scratch_appenders_directory_guard default_dir_guard(make_scratch_appenders_directory_token("default_source"));
    scratch_appenders_directory_guard explicit_dir_guard(make_scratch_appenders_directory_token("explicit_source"));
    ASSERT_TRUE(populate_appenders_scratch_dir_with_file_plugin(default_dir_guard.dir));
    ASSERT_TRUE(mc::filesystem::exists(explicit_dir_guard.dir) ||
                mc::filesystem::create_directories(explicit_dir_guard.dir));
    ASSERT_EQ(::setenv("MCAPP_LOG_APPENDERS_DIR", default_dir_guard.dir.string().c_str(), 1), 0);

    auto          config_path = explicit_dir_guard.dir / "config.json";
    std::ofstream config_file(config_path);
    ASSERT_TRUE(config_file.is_open()) << config_path.string();
    config_file << R"JSON([
  {
    "api_version": "v1",
    "kind": "Application",
    "meta": {
      "name": "mcapp_explicit_logging_dirs_app"
    }
  },
  {
    "api_version": "v1",
    "kind": "Service",
    "type": "sample_service",
    "meta": {
      "name": "mc.test.explicit.logging.dirs"
    },
    "enabled": true
  },
  {
    "api_version": "v1",
    "kind": "Logging",
    "appender_dirs": [
      "__EXPLICIT_DIR__"
    ],
    "appenders": [
      {
        "name": "explicit_file_dirs",
        "type": "file",
        "properties": {
          "module_name": "mcapp_explicit_logging_dirs"
        }
      }
    ],
    "loggers": [
      {
        "name": "default",
        "level": "notice",
        "appenders": ["explicit_file_dirs"]
      }
    ]
  }
])JSON";
    config_file.close();

    auto config_text = read_file_contents(config_path);
    auto marker_pos  = config_text.find("__EXPLICIT_DIR__");
    ASSERT_NE(marker_pos, std::string::npos);
    config_text.replace(marker_pos, std::string("__EXPLICIT_DIR__").size(), explicit_dir_guard.dir.string());
    std::ofstream rewrite_file(config_path);
    ASSERT_TRUE(rewrite_file.is_open()) << config_path.string();
    rewrite_file << config_text;
    rewrite_file.close();

    auto default_log = mc::log::default_logger();
    default_log.remove_appender("default_file");
    ASSERT_EQ(default_log.find_appender("default_file"), nullptr);

    ASSERT_TRUE(initialize_from_config_file(config_path));

    EXPECT_EQ(default_log.find_appender("default_file"), nullptr);
    EXPECT_EQ(default_log.find_appender("explicit_file_dirs"), nullptr);
    EXPECT_EQ(mc::log::appender_factory::instance().get_appender("explicit_file_dirs"), nullptr);
}

TEST_F(application_test, manifest_logging_without_appender_dirs_still_loads_builtin_file_appender)
{
    scratch_appenders_directory_guard dir_guard(make_scratch_appenders_directory_token("manifest_logging"));
    ASSERT_TRUE(populate_appenders_scratch_dir_with_file_plugin(dir_guard.dir));
    ASSERT_EQ(::setenv("MCAPP_LOG_APPENDERS_DIR", dir_guard.dir.string().c_str(), 1), 0);

    auto          config_path = dir_guard.dir / "config.json";
    std::ofstream config_file(config_path);
    ASSERT_TRUE(config_file.is_open()) << config_path.string();
    config_file << R"JSON({
  "api_version": "v1",
  "kind": "ApplicationManifest",
  "meta": {
    "name": "mcapp_manifest_logging_app"
  },
  "services": [
    {
      "api_version": "v1",
      "kind": "Service",
      "type": "sample_service",
      "meta": {
        "name": "mc.test.manifest.logging"
      },
      "enabled": true
    }
  ],
  "logging": {
    "appenders": [
      {
        "name": "manifest_file",
        "type": "file",
        "properties": {
          "module_name": "mcapp_manifest_logging"
        }
      }
    ],
    "loggers": [
      {
        "name": "default",
        "level": "notice",
        "appenders": ["manifest_file"]
      }
    ]
  }
})JSON";
    config_file.close();

    auto default_log = mc::log::default_logger();
    default_log.remove_appender("default_file");
    ASSERT_EQ(default_log.find_appender("default_file"), nullptr);

    ASSERT_TRUE(initialize_from_config_file(config_path));

    EXPECT_EQ(default_log.find_appender("default_file"), nullptr);
    EXPECT_NE(default_log.find_appender("manifest_file"), nullptr);
    EXPECT_NE(mc::log::appender_factory::instance().get_appender("manifest_file"), nullptr);
}

TEST_F(application_test, initialize_builds_service_plan_from_plan)
{
    auto definition                   = make_sample_service_definition();
    definition.properties["greeting"] = mc::variant(mc::string("hello"));

    ASSERT_TRUE(initialize_with_single_service(std::move(definition), "sample-app", 2, 4));

    const auto& plan = m_app->plan();
    ASSERT_EQ(plan.services.size(), 1U);
    EXPECT_EQ(plan.application.name, "sample-app");
    EXPECT_EQ(plan.application.io_threads, 2U);
    EXPECT_EQ(plan.application.work_threads, 4U);
    EXPECT_EQ(plan.services[0].name, "mc.test.alpha");
    EXPECT_EQ(plan.services[0].type, "sample_service");

    auto service = m_app->get_service("mc.test.alpha");
    ASSERT_NE(service, nullptr);
    auto typed = mc::static_pointer_cast<test_mcapp::sample_service>(service);
    EXPECT_EQ(typed->m_config.greeting, "hello");
}

TEST_F(application_test, start_and_stop_manage_service_lifecycle)
{
    auto definition                   = make_sample_service_definition();
    definition.properties["greeting"] = mc::variant(mc::string("hello"));
    ASSERT_TRUE(initialize_with_single_service(std::move(definition)));
    ASSERT_TRUE(m_app->start());

    auto service = m_app->get_service("mc.test.alpha");
    ASSERT_NE(service, nullptr);
    auto typed = mc::static_pointer_cast<test_mcapp::sample_service>(service);
    EXPECT_EQ(typed->state(), mc::app::service_state::running);
    EXPECT_EQ(typed->m_start_count, 1);

    ASSERT_TRUE(m_app->stop());
    EXPECT_EQ(typed->state(), mc::app::service_state::stopped);
    EXPECT_EQ(typed->m_stop_count, 1);
}

namespace {

class micro_component_iface_probe : public mc::engine::interface_proxy<micro_component_iface_probe> {
public:
    MC_INTERFACE_PROXY("bmc.kepler.MicroComponent");

    MC_PROXY_PROP(mc::string, Name);
    MC_PROXY_PROP(mc::string, Status);
    MC_PROXY_METHOD(int32_t, HealthCheck, ((mc::dict, context)));
};

class micro_component_object_probe {
public:
    micro_component_iface_probe iface;

    void bind_proxy(const mc::engine::object_proxy_seed& seed)
    {
        iface.bind_proxy(seed);
    }
};

} // namespace

TEST_F(application_test, app_service_auto_registers_micro_component_object)
{
    ASSERT_TRUE(initialize_with_single_service(make_sample_service_definition("mc.test.alpha"), "auto-mc-app"));
    ASSERT_TRUE(m_app->start());

    auto service = m_app->get_service("mc.test.alpha");
    ASSERT_NE(service, nullptr);

    auto* mc_obj = service->micro_component();
    ASSERT_NE(mc_obj, nullptr);
    EXPECT_EQ(mc_obj->get_object_path(), "/bmc/kepler/alpha/MicroComponent");

    auto proxy = service->get_proxy<micro_component_object_probe>(mc_obj->get_object_path());
    ASSERT_NE(proxy, nullptr);

    EXPECT_EQ(proxy->iface.HealthCheck(mc::dict{}), 0);
    EXPECT_EQ(proxy->iface.Name.get_value(), mc::string("alpha"));
    EXPECT_EQ(proxy->iface.Status.get_value(), mc::string("InitCompleted"));

    ASSERT_TRUE(m_app->stop());
    EXPECT_EQ(service->micro_component(), nullptr);
}

TEST_F(application_test, default_logging_loads_log_appenders_plugins_for_file_and_socket)
{
    scratch_appenders_directory_guard dir_guard(make_scratch_appenders_directory_token("file_socket"));
    ASSERT_TRUE(populate_appenders_scratch_dir_with_file_plugin(dir_guard.dir));

    const char* build_root = std::getenv("MC_GLOBAL_BUILD_ROOT");
    if (build_root == nullptr) {
        build_root = std::getenv("MC_BUILD_ROOT");
    }
    ASSERT_NE(build_root, nullptr);
    mc::filesystem::path socket_plugin = mc::filesystem::path(build_root) / "libraries" / "log_appenders" /
                                         std::string(k_builtin_socket_appender_libname);
    ASSERT_TRUE(mc::filesystem::exists(socket_plugin)) << socket_plugin.string();
    ASSERT_TRUE(
        mc::filesystem::create_symlink(socket_plugin, dir_guard.dir / std::string(k_builtin_socket_appender_libname)));

    ASSERT_EQ(::setenv("MCAPP_LOG_APPENDERS_DIR", dir_guard.dir.string().c_str(), 1), 0);

    auto default_log = mc::log::default_logger();
    default_log.remove_appender("default_file");
    ASSERT_EQ(default_log.find_appender("default_file"), nullptr);

    ASSERT_TRUE(initialize_with_single_service(make_sample_service_definition()));

    EXPECT_NE(default_log.find_appender("default_file"), nullptr);

    mc::dict socket_args{{"path", mc::string("/tmp/mcapp_ut_probe.sock")},
                         {"hb_path", mc::string("/tmp/mcapp_ut_probe.hb")},
                         {"auto_connect", false}};
    auto     socket_probe = mc::log::appender_factory::instance().create("ut_socket_probe", "socket", socket_args);
    EXPECT_NE(socket_probe, nullptr);

    auto file_probe = mc::log::appender_factory::instance().create("ut_file_probe", "file", mc::dict{});
    EXPECT_NE(file_probe, nullptr);

    default_log.remove_appender("default_file");
}

TEST_F(application_test, initialize_applies_descriptor_default_properties_before_service_config)
{
    ASSERT_TRUE(initialize_with_single_service(make_sample_service_definition()));

    auto service = m_app->get_service("mc.test.alpha");
    ASSERT_NE(service, nullptr);
    auto typed = mc::static_pointer_cast<test_mcapp::sample_service>(service);
    EXPECT_EQ(typed->m_config.greeting, "default-greeting");
}

TEST_F(application_test, initialize_keeps_module_loading_in_bootstrap_flow)
{
    auto                  definition = make_sample_service_definition();
    mc::app::service_plan plan;
    plan.application.name = "sample-app";
    plan.application.modules.push_back("mc.test.nonexistent.module");
    plan.services.push_back(std::move(definition));

    EXPECT_FALSE(m_app->initialize_with_plan(std::move(plan)));
}

TEST_F(application_test, initialize_creates_root_service_and_derives_service_path_from_name)
{
    ASSERT_TRUE(initialize_with_single_service(make_sample_service_definition("mc.test.alpha.beta")));

    auto root = m_app->root_service();
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->path(), "/");

    auto service = m_app->get_service("mc.test.alpha.beta");
    ASSERT_NE(service, nullptr);
    EXPECT_EQ(service->name(), "mc.test.alpha.beta");
    EXPECT_EQ(service->path(), "/mc/test/alpha/beta");

    auto children = root->get_children();
    ASSERT_EQ(children.size(), 1U);
    EXPECT_EQ(children[0].get(), service.get());
    ASSERT_NE(service->get_parent(), nullptr);
    EXPECT_EQ(service->get_parent().get(), root.get());
}

TEST_F(application_test, initialize_keeps_explicit_service_path)
{
    auto definition = make_sample_service_definition("mc.test.alpha.beta");
    definition.path = "/services/custom";
    ASSERT_TRUE(initialize_with_single_service(std::move(definition)));

    auto service = m_app->get_service("mc.test.alpha.beta");
    ASSERT_NE(service, nullptr);
    EXPECT_EQ(service->path(), "/services/custom");
}

TEST_F(application_test, initialize_rejects_invalid_explicit_service_path)
{
    auto definition = make_sample_service_definition("mc.test.alpha.beta");
    definition.path = "invalid/path";
    EXPECT_FALSE(initialize_with_single_service(std::move(definition)));
}

namespace {

mc::dbus::connection open_test_connection(mc::string_view service_name)
{
    auto conn = mc::dbus::connection::open_session_bus(mc::runtime::get_io_context());
    EXPECT_TRUE(conn.start());
    EXPECT_TRUE(conn.is_connected());
    if (!service_name.empty()) {
        EXPECT_TRUE(std::get<0>(conn.request_name(service_name)));
    }
    return conn;
}
TEST_F(application_test, peer_ping_hits_standard_interface)
{
    constexpr mc::string_view service_name = "mc.test.application.echo";

    auto svc = start_echo_service(service_name);
    ASSERT_NE(svc, nullptr);

    auto peer_conn = open_test_connection("");
    auto call      = make_call(service_name, mc::string_view("org.freedesktop.DBus.Peer"), mc::string_view("Ping"));
    auto reply     = call_sync(peer_conn, std::move(call));

    ASSERT_TRUE(reply.is_method_return())
        << "Peer.Ping 应返回 method_return, got " << static_cast<int>(reply.get_type());
    EXPECT_TRUE(reply.read_args().empty());
}

TEST_F(application_test, properties_get_all_returns_registered_properties)
{
    constexpr mc::string_view service_name = "mc.test.application.echo";

    auto svc = start_echo_service(service_name);
    ASSERT_NE(svc, nullptr);

    auto peer_conn = open_test_connection("");
    auto call = make_call(service_name, mc::string_view("org.freedesktop.DBus.Properties"), mc::string_view("GetAll"));
    {
        auto writer = call.writer();
        writer.write_variant_value(mc::variant(mc::string("org.test.application.Echo")));
    }
    auto reply = call_sync(peer_conn, std::move(call));

    ASSERT_TRUE(reply.is_method_return());
    auto args = reply.read_args();
    ASSERT_EQ(args.size(), 1U);
    ASSERT_TRUE(args.front().is_dict());
    auto dict = args.front().as<mc::dict>();
    EXPECT_EQ(dict["Greeting"], mc::variant(mc::string("hello")));
    EXPECT_EQ(dict["Counter"], mc::variant(int32_t{0}));
}

TEST_F(application_test, properties_get_and_set_round_trip)
{
    constexpr mc::string_view service_name = "mc.test.application.echo";

    auto svc = start_echo_service(service_name);
    ASSERT_NE(svc, nullptr);

    auto peer_conn = open_test_connection("");

    // Set Greeting = "world" (ssv)
    auto set_call = make_call(service_name, mc::string_view("org.freedesktop.DBus.Properties"), mc::string_view("Set"));
    {
        auto writer = set_call.writer();
        writer.write_variant_value(mc::variant(mc::string("org.test.application.Echo")));
        writer.write_variant_value(mc::variant(mc::string("Greeting")));
        writer.write_variant(mc::variant(mc::string("world")), 0);
    }
    auto set_reply = call_sync(peer_conn, std::move(set_call));
    ASSERT_TRUE(set_reply.is_method_return()) << "Set 失败: type=" << static_cast<int>(set_reply.get_type());

    // Get Greeting (ss -> v)
    auto get_call = make_call(service_name, mc::string_view("org.freedesktop.DBus.Properties"), mc::string_view("Get"));
    {
        auto writer = get_call.writer();
        writer.write_variant_value(mc::variant(mc::string("org.test.application.Echo")));
        writer.write_variant_value(mc::variant(mc::string("Greeting")));
    }
    auto get_reply = call_sync(peer_conn, std::move(get_call));
    ASSERT_TRUE(get_reply.is_method_return());
    auto get_args = get_reply.read_args();
    ASSERT_EQ(get_args.size(), 1U);
    EXPECT_EQ(get_args.front(), mc::variant(mc::string("world")));

    EXPECT_EQ(svc->echo_obj()->m_iface.m_greeting, "world");
}

TEST_F(application_test, introspect_returns_xml_with_registered_and_standard_interfaces)
{
    constexpr mc::string_view service_name = "mc.test.application.echo";

    auto svc = start_echo_service(service_name);
    ASSERT_NE(svc, nullptr);

    auto peer_conn = open_test_connection("");
    auto call =
        make_call(service_name, mc::string_view("org.freedesktop.DBus.Introspectable"), mc::string_view("Introspect"));
    auto reply = call_sync(peer_conn, std::move(call));
    ASSERT_TRUE(reply.is_method_return());

    auto args = reply.read_args();
    ASSERT_EQ(args.size(), 1U);
    ASSERT_TRUE(args.front().is_string());
    auto xml = args.front().as<mc::string>();
    EXPECT_NE(xml.find("<interface name=\"org.test.application.Echo\">"), mc::string::npos) << xml;
    EXPECT_NE(xml.find("<method name=\"Say\">"), mc::string::npos);
    EXPECT_NE(xml.find("<property name=\"Greeting\""), mc::string::npos);
    EXPECT_NE(xml.find("<interface name=\"org.freedesktop.DBus.Properties\">"), mc::string::npos);
    EXPECT_NE(xml.find("<interface name=\"org.freedesktop.DBus.Introspectable\">"), mc::string::npos);
    EXPECT_NE(xml.find("<interface name=\"org.freedesktop.DBus.Peer\">"), mc::string::npos);
    EXPECT_NE(xml.find("<interface name=\"org.freedesktop.DBus.ObjectManager\">"), mc::string::npos);
}

TEST_F(application_test, own_method_dispatched_via_reflection)
{
    constexpr mc::string_view service_name = "mc.test.application.echo";

    auto svc = start_echo_service(service_name);
    ASSERT_NE(svc, nullptr);

    auto peer_conn = open_test_connection("");

    auto call = make_call(service_name, mc::string_view("org.test.application.Echo"), mc::string_view("Say"));
    {
        auto writer = call.writer();
        writer.write_variant_value(mc::variant(mc::string("friend")));
    }
    auto reply = call_sync(peer_conn, std::move(call));
    ASSERT_TRUE(reply.is_method_return()) << "Say 失败: type=" << static_cast<int>(reply.get_type());

    auto args = reply.read_args();
    ASSERT_EQ(args.size(), 1U);
    EXPECT_EQ(args.front(), mc::variant(mc::string("hello:friend")));
    EXPECT_EQ(svc->echo_obj()->m_iface.m_counter, 1);
}

TEST_F(application_test, service_can_call_outbound_dbus_via_connection)
{
    constexpr mc::string_view service_name = "mc.test.application.echo";

    auto svc = start_echo_service(service_name);
    ASSERT_NE(svc, nullptr);
    ASSERT_TRUE(svc->connection().is_connected()) << "基类应当已经连上 session bus";

    auto call = mc::dbus::message::new_method_call(
        mc::string_view("org.freedesktop.DBus"), mc::string_view("/org/freedesktop/DBus"),
        mc::string_view("org.freedesktop.DBus"), mc::string_view("ListNames"));

    auto future = svc->connection().async_send_with_reply(std::move(call), mc::seconds(5));
    auto reply  = std::move(future).get();

    ASSERT_TRUE(reply.is_method_return());
    auto args = reply.read_args();
    ASSERT_EQ(args.size(), 1U);
    ASSERT_TRUE(args.front().is_array());
    const auto names      = args.front().get_array();
    bool       found_self = false;
    for (const auto& entry : names) {
        if (entry.is_string() && entry.get_string() == service_name) {
            found_self = true;
            break;
        }
    }
    EXPECT_TRUE(found_self) << "service 的 request_name 应体现在 daemon 的 ListNames 里: " << service_name;
}

// ---- DBus error mapping ----

TEST_F(application_test, error_name_on_unknown_object_is_dbus_mapped)
{
    constexpr mc::string_view service_name = "mc.test.application.echo";

    auto svc = start_echo_service(service_name);
    ASSERT_NE(svc, nullptr);

    auto peer_conn = open_test_connection("");

    // 调用一个不存在的 object path，应触发 unknown_object 错误
    auto call =
        mc::dbus::message::new_method_call(service_name, mc::string_view("/mc/test/no/such/object"),
                                           mc::string_view("org.freedesktop.DBus.Properties"), mc::string_view("Get"));
    {
        auto writer = call.writer();
        writer.write_variant_value(mc::variant(mc::string("org.test.application.Echo")));
        writer.write_variant_value(mc::variant(mc::string("Greeting")));
    }
    auto reply = call_sync(peer_conn, std::move(call));

    ASSERT_TRUE(reply.is_error()) << "expected error, got type=" << static_cast<int>(reply.get_type());
    auto err_name = reply.get_error_name();
    EXPECT_EQ(err_name, std::string_view("org.freedesktop.DBus.Error.UnknownObject"))
        << "engine 错误名 mc.engine.* 应映射为 org.freedesktop.DBus.Error.*";
}

TEST_F(application_test, error_name_on_unknown_method_is_dbus_mapped)
{
    constexpr mc::string_view service_name = "mc.test.application.echo";

    auto svc = start_echo_service(service_name);
    ASSERT_NE(svc, nullptr);

    auto peer_conn = open_test_connection("");

    // 调用已有 object 但不存在的方法，应触发 unknown_method 错误
    auto call  = mc::dbus::message::new_method_call(service_name, mc::string_view("/mc/test/application/echo"),
                                                    mc::string_view("org.test.application.Echo"),
                                                    mc::string_view("NonExistentMethod"));
    auto reply = call_sync(peer_conn, std::move(call));

    ASSERT_TRUE(reply.is_error()) << "expected error, got type=" << static_cast<int>(reply.get_type());
    auto err_name = reply.get_error_name();
    EXPECT_EQ(err_name, std::string_view("org.freedesktop.DBus.Error.UnknownMethod"))
        << "engine 错误名 mc.engine.* 应映射为 org.freedesktop.DBus.Error.*";
}

TEST_F(application_test, error_name_on_unknown_interface_is_dbus_mapped)
{
    constexpr mc::string_view service_name = "mc.test.application.echo";

    auto svc = start_echo_service(service_name);
    ASSERT_NE(svc, nullptr);

    auto peer_conn = open_test_connection("");

    // 调用已有 object 但不存在的 interface，应触发 unknown_interface 错误
    auto call  = mc::dbus::message::new_method_call(service_name, mc::string_view("/mc/test/application/echo"),
                                                    mc::string_view("org.no.such.Interface"), mc::string_view("Ping"));
    auto reply = call_sync(peer_conn, std::move(call));

    ASSERT_TRUE(reply.is_error()) << "expected error, got type=" << static_cast<int>(reply.get_type());
    auto err_name = reply.get_error_name();
    EXPECT_EQ(err_name, std::string_view("org.freedesktop.DBus.Error.UnknownInterface"))
        << "engine 错误名 mc.engine.* 应映射为 org.freedesktop.DBus.Error.*";
}

TEST_F(application_test, properties_get_all_unknown_interface_returns_error)
{
    constexpr mc::string_view service_name = "mc.test.application.echo";

    auto svc = start_echo_service(service_name);
    ASSERT_NE(svc, nullptr);

    auto peer_conn = open_test_connection("");
    auto call = make_call(service_name, mc::string_view("org.freedesktop.DBus.Properties"), mc::string_view("GetAll"));
    {
        auto writer = call.writer();
        writer.write_variant_value(mc::variant(mc::string("org.no.such.Interface")));
    }
    auto reply = call_sync(peer_conn, std::move(call));

    // 未知 interface 应返回 UnknownInterface 错误，而非静默返回空 dict
    ASSERT_TRUE(reply.is_error()) << "GetAll on unknown interface should return error, got type="
                                  << static_cast<int>(reply.get_type());
    EXPECT_EQ(reply.get_error_name(), std::string_view("org.freedesktop.DBus.Error.UnknownInterface"));
}

TEST_F(application_test, introspect_intermediate_path_lists_child_segments)
{
    constexpr mc::string_view service_name = "mc.test.application.echo";

    auto svc = start_echo_service(service_name);
    ASSERT_NE(svc, nullptr);

    auto peer_conn = open_test_connection("");

    // /mc/test/application 上无注册对象，但其下方挂有 /mc/test/application/echo，
    // 按 DBus 规范应返回最小节点 XML，列出下一段子节点名 echo。
    auto call  = mc::dbus::message::new_method_call(service_name, mc::string_view("/mc/test/application"),
                                                    mc::string_view("org.freedesktop.DBus.Introspectable"),
                                                    mc::string_view("Introspect"));
    auto reply = call_sync(peer_conn, std::move(call));

    ASSERT_TRUE(reply.is_method_return())
        << "Introspect on intermediate path should return XML, got type=" << static_cast<int>(reply.get_type());
    auto args = reply.read_args();
    ASSERT_EQ(args.size(), 1U);
    ASSERT_TRUE(args.front().is_string());
    auto xml = args.front().as<mc::string>();
    EXPECT_NE(xml.find("<node name=\"echo\"/>"), mc::string::npos)
        << "中间路径 Introspect 应包含 <node name=\"echo\"/>: " << xml;
    EXPECT_EQ(xml.find("<interface"), mc::string::npos) << "中间路径 Introspect 不应列出接口: " << xml;
}

TEST_F(application_test, introspect_unknown_path_without_descendants_returns_unknown_object)
{
    constexpr mc::string_view service_name = "mc.test.application.echo";

    auto svc = start_echo_service(service_name);
    ASSERT_NE(svc, nullptr);

    auto peer_conn = open_test_connection("");

    // 该 path 既未注册对象，下方也无任何已注册子对象，应返回 UnknownObject 错误。
    auto call = mc::dbus::message::new_method_call(
        service_name, mc::string_view("/mc/test/application/echo/nonexistent_child"),
        mc::string_view("org.freedesktop.DBus.Introspectable"), mc::string_view("Introspect"));
    auto reply = call_sync(peer_conn, std::move(call));

    ASSERT_TRUE(reply.is_error()) << "Introspect on path with neither object nor descendants should error";
    EXPECT_EQ(reply.get_error_name(), std::string_view("org.freedesktop.DBus.Error.UnknownObject"));
}

// ---- 业务信号 match_rule 订阅示例 ----

static mc::engine::message make_signal_msg(mc::string_view path, mc::string_view interface_name,
                                           mc::string_view member_name, const mc::variants& args,
                                           mc::string_view signature)
{
    mc::engine::message msg;
    msg.header.type           = mc::engine::message_type::signal;
    msg.header.path           = mc::string(path);
    msg.header.interface_name = mc::string(interface_name);
    msg.header.member_name    = mc::string(member_name);
    msg.body                  = mc::engine::make_payload<mc::engine::signal_payload>(signature, mc::variants(args));
    return msg;
}

TEST_F(application_test, business_signal_match_rule_subscribe_and_receive)
{
    constexpr mc::string_view service_name = "mc.test.application.echo";

    auto svc = start_echo_service(service_name);
    ASSERT_NE(svc, nullptr);

    // 构建 match_rule：匹配 org.test.application.Echo 的 Greeted 信号
    mc::engine::match::match_rule rule;
    rule.type           = "signal";
    rule.interface_name = mc::string(mc::string_view("org.test.application.Echo"));
    rule.member_name    = mc::string(mc::string_view("Greeted"));

    // 通过 add_match 订阅
    std::mutex                         mutex;
    std::condition_variable            cv;
    std::optional<mc::engine::message> captured;
    auto id = svc->add_match(rule, mc::engine::filter_spec{}, [&](const mc::engine::message& msg) {
        std::lock_guard lock(mutex);
        captured = msg;
        cv.notify_all();
    });
    ASSERT_NE(id, 0u);

    // 发送 Greeted 信号
    auto sig = make_signal_msg("/mc/test/application/echo", "org.test.application.Echo", "Greeted",
                               mc::variants{mc::variant(mc::string("world"))}, "s");
    svc->emit(sig);

    // 等待 callback 触发
    {
        std::unique_lock lock(mutex);
        ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(2), [&]() {
            return captured.has_value();
        }));
    }
    ASSERT_TRUE(captured.has_value());
    EXPECT_EQ(captured->header.type, mc::engine::message_type::signal);
    EXPECT_EQ(captured->header.interface_name, "org.test.application.Echo");
    EXPECT_EQ(captured->header.member_name, "Greeted");

    svc->remove_match(id);
}

} // namespace
