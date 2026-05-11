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

#include <mc/app/micro_component.h>
#include <mc/engine.h>
#include <mc/engine/proxy.h>
#include <mc/engine/service.h>
#include <mc/exception.h>
#include <mc/filesystem.h>
#include <mc/log/appender_factory.h>
#include <mc/log/log.h>
#include <mc/log/log_manager.h>
#include <mc/log/logger.h>
#include <test_utilities/engine_test_base.h>

#include <chrono>
#include <cstdlib>
#include <memory>

// micro_component 端到端测试：客户端使用 proxy 经反射 dispatch 调用接口方法和属性。

namespace {

constexpr mc::string_view k_service_name{"org.openubmc.test_micro_component"};
constexpr mc::string_view k_app_name{"test_micro_component"};

// 跟踪型 socket appender：记录 init 调用次数与最近一次配置参数，避免触达 /dev/shm。
class tracking_socket_appender : public mc::log::appender {
public:
    bool init(const mc::variant& args) override
    {
        ++m_init_count;
        m_last_config = args.is_dict() ? args.as<mc::dict>() : mc::dict{};
        return true;
    }

    void append(const mc::log::message&) override
    {}

    int init_count() const noexcept
    {
        return m_init_count;
    }

    const mc::dict& last_config() const noexcept
    {
        return m_last_config;
    }

private:
    int      m_init_count{0};
    mc::dict m_last_config;
};

void register_tracking_socket_creator()
{
    mc::log::appender_factory::instance().register_creator("socket", []() {
        return std::make_shared<tracking_socket_appender>();
    });
}

void clear_socket_appenders()
{
    auto default_log = mc::log::default_logger();
    default_log.remove_appender("mdbctl");
    auto mdbctl_log = mc::log::logger::get(MC_LOG_MDBCTL_LOGGER);
    mdbctl_log.remove_appender("mdbctl_socket");
}

class micro_component_iface_proxy : public mc::engine::interface_proxy<micro_component_iface_proxy> {
public:
    MC_INTERFACE_PROXY("bmc.kepler.MicroComponent");

    MC_PROXY_PROP(int32_t, Pid);
    MC_PROXY_PROP(mc::string, Name);
    MC_PROXY_PROP(mc::string, Status);
    MC_PROXY_PROP(mc::string, Version);

    MC_PROXY_METHOD(int32_t, HealthCheck, ((mc::dict, context)));
};

class config_manage_iface_proxy : public mc::engine::interface_proxy<config_manage_iface_proxy> {
public:
    MC_INTERFACE_PROXY("bmc.kepler.MicroComponent.ConfigManage");

    MC_PROXY_METHOD(mc::variant, Backup, ((mc::dict, context))((mc::string, filepath)));
    MC_PROXY_METHOD(mc::string, Export, ((mc::dict, context))((mc::string, type)));
    MC_PROXY_METHOD(void, Import, ((mc::dict, context))((mc::string, data))((mc::string, type)));
    MC_PROXY_METHOD(void, Recover, ((mc::dict, context))((mc::dict, preserve_list)));
    MC_PROXY_METHOD(mc::string, Verify, ((mc::dict, context))((mc::string, data)));
    MC_PROXY_METHOD(mc::string, GetPreservedConfig, ((mc::dict, context))((mc::dict, preserve_flag)));
    MC_PROXY_METHOD(mc::string, GetTrustedConfig, ((mc::dict, context)));
};

class debug_iface_proxy : public mc::engine::interface_proxy<debug_iface_proxy> {
public:
    MC_INTERFACE_PROXY("bmc.kepler.MicroComponent.Debug");

    MC_PROXY_PROP(mc::string, DlogLevel);
    MC_PROXY_PROP(mc::string, DlogType);

    MC_PROXY_METHOD(void, AttachDebugConsole, ((uint32_t, port)));
    MC_PROXY_METHOD(void, DetachDebugConsole);
    MC_PROXY_METHOD(void, Dump, ((mc::string, filepath)));
    MC_PROXY_METHOD(void, SetDlogLevel, ((mc::string, level))((uint8_t, effective_hours)));
};

class reboot_iface_proxy : public mc::engine::interface_proxy<reboot_iface_proxy> {
public:
    MC_INTERFACE_PROXY("bmc.kepler.MicroComponent.Reboot");

    MC_PROXY_METHOD(int32_t, Prepare);
    MC_PROXY_METHOD(int32_t, Process);
    MC_PROXY_METHOD(int32_t, Action);
    MC_PROXY_METHOD(void, Cancel);
};

class reset_iface_proxy : public mc::engine::interface_proxy<reset_iface_proxy> {
public:
    MC_INTERFACE_PROXY("bmc.kepler.MicroComponent.Reset");

    MC_PROXY_METHOD(int32_t, Prepare, ((mc::string, reset_type)));
    MC_PROXY_METHOD(int32_t, Action, ((mc::string, reset_type)));
    MC_PROXY_METHOD(void, Cancel, ((mc::string, reset_type)));
};

class maintenance_iface_proxy : public mc::engine::interface_proxy<maintenance_iface_proxy> {
public:
    MC_INTERFACE_PROXY("bmc.kepler.Release.Maintenance");

    MC_PROXY_METHOD(void, DlogLimit, ((bool, enabled))((uint8_t, duration_mins)));
};

class micro_component_object_proxy {
public:
    micro_component_iface_proxy mc_iface;
    config_manage_iface_proxy   config_iface;
    debug_iface_proxy           debug_iface;
    reboot_iface_proxy          reboot_iface;
    reset_iface_proxy           reset_iface;
    maintenance_iface_proxy     maintenance_iface;

    void bind_proxy(const mc::engine::object_proxy_seed& seed)
    {
        mc_iface.bind_proxy(seed);
        config_iface.bind_proxy(seed);
        debug_iface.bind_proxy(seed);
        reboot_iface.bind_proxy(seed);
        reset_iface.bind_proxy(seed);
        maintenance_iface.bind_proxy(seed);
    }
};

struct test_micro_service : public mc::engine::service {
    test_micro_service() : mc::engine::service(k_service_name)
    {}
};

class micro_component_proxy_test : public mc::test::TestWithEngine {
protected:
    void SetUp() override
    {
        TestWithEngine::SetUp();

        clear_socket_appenders();
        register_tracking_socket_creator();

        m_service = std::make_unique<test_micro_service>();
        ASSERT_TRUE(m_service->init());
        ASSERT_TRUE(m_service->start());

        m_object = mc::make_shared<mc::app::micro_component_object>();
        m_object->init(k_service_name);
        m_service->register_object(*m_object);

        m_proxy = m_service->get_proxy<micro_component_object_proxy>(m_object->get_object_path());
        ASSERT_NE(m_proxy, nullptr);
    }

    void TearDown() override
    {
        m_proxy.reset();
        if (m_service && m_object) {
            m_service->unregister_object(m_object->get_object_path());
        }
        m_object.reset();
        if (m_service) {
            m_service->stop();
            m_service->cleanup();
        }
        m_service.reset();

        clear_socket_appenders();
        ::unsetenv("MCC_DEBUG");

        TestWithEngine::TearDown();
    }

    std::unique_ptr<test_micro_service>             m_service;
    mc::shared_ptr<mc::app::micro_component_object> m_object;
    std::unique_ptr<micro_component_object_proxy>   m_proxy;
};

} // namespace

TEST_F(micro_component_proxy_test, health_check_via_proxy_returns_zero)
{
    EXPECT_EQ(m_proxy->mc_iface.HealthCheck(mc::dict{}), 0);
}

TEST_F(micro_component_proxy_test, properties_via_proxy_match_init_defaults)
{
    EXPECT_EQ(m_proxy->mc_iface.Name.get_value(), mc::string(k_app_name));
    EXPECT_EQ(m_proxy->mc_iface.Status.get_value(), mc::string("InitCompleted"));
    EXPECT_EQ(m_proxy->mc_iface.Pid.get_value(), static_cast<int32_t>(::getpid()));
}

TEST_F(micro_component_proxy_test, config_manage_methods_via_proxy_return_defaults)
{
    auto backup_result = m_proxy->config_iface.Backup(mc::dict{}, mc::string("/tmp"));
    EXPECT_TRUE(backup_result.is_array());
    EXPECT_EQ(m_proxy->config_iface.Export(mc::dict{}, mc::string("yaml")), mc::string());
    EXPECT_NO_THROW(m_proxy->config_iface.Import(mc::dict{}, mc::string(), mc::string()));
    EXPECT_NO_THROW(m_proxy->config_iface.Recover(mc::dict{}, mc::dict{}));
    EXPECT_EQ(m_proxy->config_iface.Verify(mc::dict{}, mc::string()), mc::string());
    EXPECT_EQ(m_proxy->config_iface.GetPreservedConfig(mc::dict{}, mc::dict{}), mc::string());
    EXPECT_EQ(m_proxy->config_iface.GetTrustedConfig(mc::dict{}), mc::string());
}

TEST_F(micro_component_proxy_test, attach_debug_console_via_proxy_configures_socket_appenders)
{
    m_proxy->debug_iface.AttachDebugConsole(2233U);

    auto default_log      = mc::log::default_logger();
    auto default_appender = std::dynamic_pointer_cast<tracking_socket_appender>(default_log.find_appender("mdbctl"));
    ASSERT_NE(default_appender, nullptr);
    EXPECT_EQ(default_appender->init_count(), 1);
    EXPECT_EQ(default_appender->last_config()["path"], "/dev/shm/2233.sock");
    EXPECT_EQ(default_appender->last_config()["hb_path"], "/dev/shm/2233.hbsock");
    EXPECT_EQ(default_appender->last_config()["type"], "file");
    EXPECT_EQ(default_appender->last_config()["auto_connect"], true);
    EXPECT_EQ(default_appender->last_config()["heartbeat_interval_sec"], static_cast<std::int64_t>(1));

    auto mdbctl_log = mc::log::logger::get(MC_LOG_MDBCTL_LOGGER);
    auto mdbctl_appender =
        std::dynamic_pointer_cast<tracking_socket_appender>(mdbctl_log.find_appender("mdbctl_socket"));
    ASSERT_NE(mdbctl_appender, nullptr);
    EXPECT_EQ(mdbctl_appender->last_config()["type"], "local");

    m_proxy->debug_iface.DlogType.set_value(mc::string("remote"));
    EXPECT_EQ(default_appender->init_count(), 2);
    EXPECT_EQ(default_appender->last_config()["type"], "remote");
}

TEST_F(micro_component_proxy_test, detach_debug_console_via_proxy_clears_socket_appenders)
{
    m_proxy->debug_iface.AttachDebugConsole(4321U);

    auto default_log = mc::log::default_logger();
    auto mdbctl_log  = mc::log::logger::get(MC_LOG_MDBCTL_LOGGER);
    ASSERT_NE(default_log.find_appender("mdbctl"), nullptr);
    ASSERT_NE(mdbctl_log.find_appender("mdbctl_socket"), nullptr);

    m_proxy->debug_iface.DetachDebugConsole();

    EXPECT_EQ(default_log.find_appender("mdbctl"), nullptr);
    EXPECT_EQ(mdbctl_log.find_appender("mdbctl_socket"), nullptr);
    EXPECT_EQ(m_proxy->debug_iface.DlogType.get_value(), mc::string("file"));
}

TEST_F(micro_component_proxy_test, attach_debug_console_twice_via_proxy_throws)
{
    m_proxy->debug_iface.AttachDebugConsole(5555U);
    EXPECT_ANY_THROW(m_proxy->debug_iface.AttachDebugConsole(5555U));
}

TEST_F(micro_component_proxy_test, set_dlog_level_via_proxy_updates_log_manager)
{
    m_proxy->debug_iface.SetDlogLevel(mc::string("warn"), 0);
    EXPECT_EQ(m_proxy->debug_iface.DlogLevel.get_value(), mc::string("warn"));
    EXPECT_TRUE(mc::log::default_logger().is_enabled(mc::log::level::warn));
    EXPECT_FALSE(mc::log::default_logger().is_enabled(mc::log::level::info));

    m_proxy->debug_iface.SetDlogLevel(mc::string("error"), 0);
    EXPECT_EQ(m_proxy->debug_iface.DlogLevel.get_value(), mc::string("error"));
    EXPECT_FALSE(mc::log::default_logger().is_enabled(mc::log::level::warn));

    m_proxy->debug_iface.SetDlogLevel(mc::string("notice"), 0);
    EXPECT_EQ(m_proxy->debug_iface.DlogLevel.get_value(), mc::string("notice"));
    EXPECT_TRUE(mc::log::default_logger().is_enabled(mc::log::level::notice));
}

TEST_F(micro_component_proxy_test, set_dlog_level_via_proxy_rejects_invalid)
{
    EXPECT_ANY_THROW(m_proxy->debug_iface.SetDlogLevel(mc::string("verbose-not-supported"), 0));
}

TEST_F(micro_component_proxy_test, reboot_methods_via_proxy_return_zero)
{
    EXPECT_EQ(m_proxy->reboot_iface.Prepare(), 0);
    EXPECT_EQ(m_proxy->reboot_iface.Process(), 0);
    EXPECT_EQ(m_proxy->reboot_iface.Action(), 0);
    EXPECT_NO_THROW(m_proxy->reboot_iface.Cancel());
}

TEST_F(micro_component_proxy_test, reset_methods_via_proxy_return_zero)
{
    EXPECT_EQ(m_proxy->reset_iface.Prepare(mc::string("warm")), 0);
    EXPECT_EQ(m_proxy->reset_iface.Action(mc::string("cold")), 0);
    EXPECT_NO_THROW(m_proxy->reset_iface.Cancel(mc::string("warm")));
}

TEST_F(micro_component_proxy_test, dlog_limit_via_proxy_handles_all_branches)
{
    setenv("MCC_DEBUG", "1", 1);
    m_proxy->maintenance_iface.DlogLimit(true, 60);
    EXPECT_EQ(getenv("MCC_DEBUG"), nullptr);

    setenv("MCC_DEBUG", "1", 1);
    m_proxy->maintenance_iface.DlogLimit(false, 0);
    EXPECT_EQ(getenv("MCC_DEBUG"), nullptr);

    unsetenv("MCC_DEBUG");
    m_proxy->maintenance_iface.DlogLimit(false, 60);
    const char* val = getenv("MCC_DEBUG");
    ASSERT_NE(val, nullptr);
    EXPECT_STREQ(val, "1");

    unsetenv("MCC_DEBUG");
    m_proxy->maintenance_iface.DlogLimit(false, 1);
    val = getenv("MCC_DEBUG");
    ASSERT_NE(val, nullptr);
    EXPECT_STREQ(val, "1");
}

TEST_F(micro_component_proxy_test, dump_via_proxy_writes_log_to_existing_directory)
{
    auto unique_suffix =
        std::to_string(static_cast<long long>(std::chrono::steady_clock::now().time_since_epoch().count()));
    mc::string dest_dir = mc::string::concat("./testdir_dump_proxy_", unique_suffix);
    mc::filesystem::create_directories(dest_dir);

    m_proxy->debug_iface.Dump(dest_dir);

    mc::string log_path = mc::string::concat(dest_dir, "/mdb_info.log");
    EXPECT_TRUE(mc::filesystem::exists(log_path));
    auto content = mc::filesystem::read_file(log_path);
    ASSERT_TRUE(content.has_value());
    EXPECT_FALSE(content->empty());

    mc::filesystem::remove_all(dest_dir);
}

TEST_F(micro_component_proxy_test, dump_via_proxy_with_invalid_directory_does_not_throw)
{
    mc::string invalid_dir = "/nonexistent/invalid/micro_component_dump_proxy_path";
    EXPECT_NO_THROW(m_proxy->debug_iface.Dump(invalid_dir));
    mc::string log_path = mc::string::concat(invalid_dir, "/mdb_info.log");
    EXPECT_FALSE(mc::filesystem::exists(log_path));
}

TEST(micro_component_test, init_sets_object_path_and_defaults)
{
    mc::app::micro_component_object obj;
    obj.init("com.example.MyService");

    EXPECT_EQ(obj.get_object_path(), "/bmc/kepler/MyService/MicroComponent");
    EXPECT_EQ(obj.get_object_name(), "MicroComponent_MyService");
    EXPECT_EQ(mc::string(obj.m_mc_iface.m_name), "MyService");
    EXPECT_EQ(mc::string(obj.m_mc_iface.m_status), "InitCompleted");
    EXPECT_EQ(mc::string(obj.m_mc_debug_iface.m_dlog_level), "notice");
    EXPECT_EQ(mc::string(obj.m_mc_debug_iface.m_dlog_type), "file");
}
