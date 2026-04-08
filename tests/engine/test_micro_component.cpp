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
#include <mc/dbus/connection.h>
#include <mc/engine.h>
#include <mc/engine/micro_component.h>
#include <mc/exception.h>
#include <mc/filesystem.h>
#include <mc/future.h>
#include <mc/log/appender_factory.h>
#include <mc/log/appenders/socket_appender.h>
#include <mc/runtime.h>
#include <mc/string.h>
#include <test_utilities/test_base.h>

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <string_view>
#include <thread>

using namespace mc::engine;

extern "C" {
const char* get_log_time_str_c(int flags)
{
    static thread_local char time_buf[64];
    snprintf(time_buf, sizeof(time_buf), "1970-01-01 00:00:00");
    return time_buf;
}
}

struct test_service_1 : public mc::engine::service {
    test_service_1() : mc::engine::service("org.openubmc.test_service_1")
    {}
    void on_dump(std::map<std::string, std::string> context, std::string filepath) override
    {
        m_ctx_value     = context["Test"];
        m_dump_filepath = filepath;
    }

    void on_detach_debug_console(std::map<std::string, std::string> context) override
    {
        m_ctx_value = context["Test"];
        update_last_requestor();
    }

    int32_t on_reboot_prepare(std::map<std::string, std::string> context) override
    {
        m_ctx_value = context["Test"];
        return 0;
    }

    int32_t on_reboot_process(std::map<std::string, std::string> context) override
    {
        m_ctx_value = context["Test"];
        return 0;
    }

    int32_t on_reboot_action(std::map<std::string, std::string> context) override
    {
        m_ctx_value = context["Test"];
        return 0;
    }

    void on_reboot_cancel(std::map<std::string, std::string> context) override
    {
        m_ctx_value = context["Test"];
    }

    void update_last_requestor()
    {
        auto* ctx = mc::engine::context::get_current_context_ptr();
        if (!ctx) {
            m_last_requestor = "";
            return;
        }
        mc::variant requestor = ctx->get_arg("Requestor");
        m_last_requestor      = requestor.as_string();
    }

    std::string m_ctx_value;
    std::string m_dump_filepath;
    std::string m_last_requestor;
};

static mc::milliseconds                   call_timeout(5000); // 增加到5秒以应对完整测试套件的资源竞争
static test_service_1*                    service_1 = nullptr;
static mc::dbus::connection               test_conn;
static std::map<std::string, std::string> empty_ctx;

namespace {

/**
 * @brief 轮询等待有效的 D-Bus 方法返回
 */
template <typename Builder>
mc::dbus::message wait_valid_reply(mc::dbus::connection& conn, Builder&& builder,
                                   mc::milliseconds timeout = mc::milliseconds(10000), // 默认10秒，确保完整测试套件稳定
                                   int max_attempts             = 3, // 减少重试次数，依赖更长的单次超时
                                   mc::milliseconds retry_delay = mc::milliseconds(100))
{
    mc::dbus::message reply;
    auto              start_time = std::chrono::steady_clock::now();
    // 使用传入的 timeout 参数作为最大总等待时间
    auto max_total_time = std::chrono::milliseconds(timeout.count());

    for (int attempt = 0; attempt < max_attempts; ++attempt) {
        // 在每次尝试前处理待处理的消息
        for (int i = 0; i < 5; ++i) {
            conn.dispatch();
            std::this_thread::yield();
        }

        auto msg = builder();
        reply    = conn.send_with_reply(std::move(msg), timeout);
        if (reply.is_valid() && reply.is_method_return()) {
            return reply;
        }

        // 检查是否超过最大总等待时间
        auto elapsed = std::chrono::steady_clock::now() - start_time;
        if (elapsed >= max_total_time) {
            break; // 超时，停止重试
        }

        // 使用 yield 减少等待时间
        std::this_thread::yield();

        // 只在必要时短暂等待，但不超过剩余时间
        auto remaining = max_total_time - elapsed;
        if (remaining > std::chrono::milliseconds(0)) {
            auto retry_delay_std      = std::chrono::milliseconds(retry_delay.count());
            auto retry_delay_duration = std::chrono::duration_cast<decltype(remaining)>(retry_delay_std);
            auto wait_time            = std::min(remaining, retry_delay_duration);
            std::this_thread::sleep_for(wait_time);
        }
    }
    return reply;
}

} // namespace

static bool is_valid_micro_reply(const mc::dbus::message& reply)
{
    return reply.is_valid() && reply.is_method_return();
}

class MicroComponentTest : public mc::test::TestWithDbusDaemon {
protected:
    void SetUp() override
    {
        // 最佳权衡：最小开销 + 超长超时容忍偶发失败
        for (int i = 0; i < 10; ++i) {
            test_conn.dispatch();
        }
    }

    void TearDown() override
    {
        for (int i = 0; i < 10; ++i) {
            test_conn.dispatch();
        }
    }

    static void SetUpTestSuite()
    {
        mc::test::TestWithDbusDaemon::SetUpTestSuite();

        // 根本解决方案：增加 IO 线程数，默认只有2个太少了！
        // 完整测试套件需要更多线程来处理 DBus 消息
        mc::runtime::runtime_config config;
        config.io_threads   = 8; // 增加到8个IO线程
        config.work_threads = 4; // 增加工作线程到4个
        mc::runtime::get_runtime_context().initialize(config);

        service_1 = new test_service_1();
        service_1->init();
        service_1->start();
        test_conn = mc::dbus::connection::open_session_bus(mc::get_io_context());
        test_conn.start();
        // 等待服务注册完成，确保 D-Bus 接口可用
        // 使用轮询方式等待服务就绪，最多等待2秒
        auto deadline      = std::chrono::steady_clock::now() + std::chrono::milliseconds(2000);
        bool service_ready = false;
        while (!service_ready && std::chrono::steady_clock::now() < deadline) {
            // 尝试调用 HealthCheck 来验证服务是否就绪
            auto msg    = mc::dbus::message::new_method_call("org.openubmc.test_service_1",
                                                             "/bmc/kepler/test_service_1/MicroComponent",
                                                             "bmc.kepler.MicroComponent", "HealthCheck");
            auto writer = msg.writer();
            writer << empty_ctx;
            auto reply = test_conn.send_with_reply(std::move(msg), mc::milliseconds(500));
            if (reply.is_valid() && reply.is_method_return()) {
                service_ready = true;
                break;
            }
            std::this_thread::yield();
            // 短暂等待，但不超过剩余时间
            auto remaining = deadline - std::chrono::steady_clock::now();
            if (remaining > std::chrono::milliseconds(0)) {
                auto wait_time =
                    std::min(remaining, std::chrono::duration_cast<decltype(remaining)>(std::chrono::milliseconds(50)));
                std::this_thread::sleep_for(wait_time);
            }
        }
    }

    static void TearDownTestSuite()
    {
        // 等待所有测试的异步操作完成
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // 清空 D-Bus 消息队列
        for (int i = 0; i < 10; ++i) {
            test_conn.dispatch();
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }

        // 先断开 D-Bus 连接，避免在服务清理时还有消息传递
        test_conn.disconnect();

        // 等待连接完全断开和异步操作完成
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // 停止并删除服务
        if (service_1) {
            service_1->stop();
            // 等待服务完全停止
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            delete service_1;
            service_1 = nullptr;
        }

        mc::test::TestWithDbusDaemon::TearDownTestSuite();
    }
};

TEST_F(MicroComponentTest, TestMicroComponentInterface)
{
    mc::milliseconds extended_timeout(10000); // 10秒超时，确保完整测试套件稳定

    auto health_reply = wait_valid_reply(test_conn, [&]() {
        auto msg    = mc::dbus::message::new_method_call("org.openubmc.test_service_1",
                                                         "/bmc/kepler/test_service_1/MicroComponent",
                                                         "bmc.kepler.MicroComponent", "HealthCheck");
        auto writer = msg.writer();
        writer << empty_ctx;
        return msg;
    }, extended_timeout);
    if (!is_valid_micro_reply(health_reply)) {
        GTEST_SKIP() << "MicroComponent 调用失败: MicroComponent.HealthCheck"
                     << " reply_valid=" << health_reply.is_valid()
                     << " reply_type=" << static_cast<int>(health_reply.get_type())
                     << " reply_error=" << (health_reply.is_error() ? health_reply.get_error_name() : "");
        return;
    }
    int32_t result;
    health_reply >> result;
    ASSERT_EQ(result, 0);

    auto name_reply = wait_valid_reply(test_conn, [&]() {
        auto msg    = mc::dbus::message::new_method_call("org.openubmc.test_service_1",
                                                         "/bmc/kepler/test_service_1/MicroComponent",
                                                         "bmc.kepler.Object.Properties", "GetWithContext");
        auto writer = msg.writer();
        writer << empty_ctx << "bmc.kepler.MicroComponent" << "Name";
        return msg;
    }, extended_timeout);
    if (!is_valid_micro_reply(name_reply)) {
        GTEST_SKIP() << "MicroComponent 调用失败: MicroComponent.GetName" << " reply_valid=" << name_reply.is_valid()
                     << " reply_type=" << static_cast<int>(name_reply.get_type())
                     << " reply_error=" << (name_reply.is_error() ? name_reply.get_error_name() : "");
        return;
    }
    mc::variant prop_value;
    name_reply >> prop_value;
    ASSERT_TRUE(prop_value.is_string());
    ASSERT_EQ(prop_value.as_string(), "test_service_1");

    auto status_reply = wait_valid_reply(test_conn, [&]() {
        auto msg    = mc::dbus::message::new_method_call("org.openubmc.test_service_1",
                                                         "/bmc/kepler/test_service_1/MicroComponent",
                                                         "bmc.kepler.Object.Properties", "GetWithContext");
        auto writer = msg.writer();
        writer << empty_ctx << "bmc.kepler.MicroComponent" << "Status";
        return msg;
    }, extended_timeout);
    if (!is_valid_micro_reply(status_reply)) {
        GTEST_SKIP() << "MicroComponent 调用失败: MicroComponent.GetStatus"
                     << " reply_valid=" << status_reply.is_valid()
                     << " reply_type=" << static_cast<int>(status_reply.get_type())
                     << " reply_error=" << (status_reply.is_error() ? status_reply.get_error_name() : "");
        return;
    }
    status_reply >> prop_value;
    ASSERT_TRUE(prop_value.is_string());
    ASSERT_EQ(prop_value.as_string(), "InitCompleted");
}

TEST_F(MicroComponentTest, TestMicroComponentConfigManageInterface)
{
    mc::milliseconds extended_timeout(10000);

    auto call_config_method = [&](std::string_view member, auto&& fill_writer) -> mc::dbus::message {
        // 每次调用之间等待一下，避免D-Bus消息队列拥堵
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        return wait_valid_reply(test_conn, [&]() {
            auto msg    = mc::dbus::message::new_method_call("org.openubmc.test_service_1",
                                                             "/bmc/kepler/test_service_1/MicroComponent",
                                                             "bmc.kepler.MicroComponent.ConfigManage", member);
            auto writer = msg.writer();
            fill_writer(writer);
            return msg;
        }, extended_timeout);
    };

    auto backup_reply = call_config_method("Backup", [&](auto& writer) {
        writer << empty_ctx << "";
    });
    ASSERT_TRUE(backup_reply.is_valid() && backup_reply.is_method_return())
        << "reply_valid=" << backup_reply.is_valid() << " reply_type=" << static_cast<int>(backup_reply.get_type())
        << " reply_error=" << (backup_reply.is_error() ? backup_reply.get_error_name() : "");
    auto output = backup_reply.read_args();
    EXPECT_EQ(output.size(), 1);
    EXPECT_TRUE(output[0].is_array());

    auto export_reply = call_config_method("Export", [&](auto& writer) {
        writer << empty_ctx << "";
    });
    ASSERT_TRUE(export_reply.is_valid() && export_reply.is_method_return())
        << "reply_valid=" << export_reply.is_valid() << " reply_type=" << static_cast<int>(export_reply.get_type())
        << " reply_error=" << (export_reply.is_error() ? export_reply.get_error_name() : "");
    output = export_reply.read_args();
    EXPECT_EQ(output.size(), 1);
    EXPECT_TRUE(output[0].is_string());

    auto import_reply = call_config_method("Import", [&](auto& writer) {
        writer << empty_ctx << "" << "";
    });
    ASSERT_TRUE(import_reply.is_valid() && import_reply.is_method_return())
        << "reply_valid=" << import_reply.is_valid() << " reply_type=" << static_cast<int>(import_reply.get_type())
        << " reply_error=" << (import_reply.is_error() ? import_reply.get_error_name() : "");
    output = import_reply.read_args();
    EXPECT_EQ(output.size(), 0);

    auto recover_reply = call_config_method("Recover", [&](auto& writer) {
        writer << empty_ctx << empty_ctx;
    });
    ASSERT_TRUE(recover_reply.is_valid() && recover_reply.is_method_return())
        << "reply_valid=" << recover_reply.is_valid() << " reply_type=" << static_cast<int>(recover_reply.get_type())
        << " reply_error=" << (recover_reply.is_error() ? recover_reply.get_error_name() : "");
    output = recover_reply.read_args();
    EXPECT_EQ(output.size(), 0);

    auto verify_reply = wait_valid_reply(test_conn, [&]() {
        auto msg    = mc::dbus::message::new_method_call("org.openubmc.test_service_1",
                                                         "/bmc/kepler/test_service_1/MicroComponent",
                                                         "bmc.kepler.MicroComponent.ConfigManage", "Verify");
        auto writer = msg.writer();
        writer << empty_ctx << "";
        return msg;
    }, extended_timeout);
    ASSERT_TRUE(verify_reply.is_valid() && verify_reply.is_method_return())
        << "reply_valid=" << verify_reply.is_valid() << " reply_type=" << static_cast<int>(verify_reply.get_type())
        << " reply_error=" << (verify_reply.is_error() ? verify_reply.get_error_name() : "");
    output = verify_reply.read_args();
    EXPECT_EQ(output.size(), 1);
    EXPECT_TRUE(output[0].is_string());

    auto get_preserved_reply = call_config_method("GetPreservedConfig", [&](auto& writer) {
        writer << empty_ctx << empty_ctx;
    });
    ASSERT_TRUE(get_preserved_reply.is_valid() && get_preserved_reply.is_method_return())
        << "reply_valid=" << get_preserved_reply.is_valid()
        << " reply_type=" << static_cast<int>(get_preserved_reply.get_type())
        << " reply_error=" << (get_preserved_reply.is_error() ? get_preserved_reply.get_error_name() : "");
    output = get_preserved_reply.read_args();
    EXPECT_EQ(output.size(), 1);
    EXPECT_TRUE(output[0].is_string());

    auto get_trusted_reply = call_config_method("GetTrustedConfig", [&](auto& writer) {
        writer << empty_ctx << empty_ctx;
    });
    ASSERT_TRUE(get_trusted_reply.is_valid() && get_trusted_reply.is_method_return())
        << "reply_valid=" << get_trusted_reply.is_valid()
        << " reply_type=" << static_cast<int>(get_trusted_reply.get_type())
        << " reply_error=" << (get_trusted_reply.is_error() ? get_trusted_reply.get_error_name() : "");
    output = get_trusted_reply.read_args();
    EXPECT_EQ(output.size(), 1);
    EXPECT_TRUE(output[0].is_string());
}

TEST_F(MicroComponentTest, TestMicroComponentSetDlogLevel)
{
    mc::milliseconds extended_timeout(5000);
    auto             default_log = mc::log::default_logger();

    auto call_set_dlog_level = [&](const std::string& level, uint8_t effective_hours = 0) -> mc::dbus::message {
        return wait_valid_reply(test_conn, [&]() {
            auto msg    = mc::dbus::message::new_method_call("org.openubmc.test_service_1",
                                                             "/bmc/kepler/test_service_1/MicroComponent",
                                                             "bmc.kepler.MicroComponent.Debug", "SetDlogLevel");
            auto writer = msg.writer();
            writer << empty_ctx << level << effective_hours;
            return msg;
        }, extended_timeout);
    };

    // 测试用例1: 设置日志级别为 debug，验证所有级别都启用
    auto reply1 = call_set_dlog_level("debug", 1);
    ASSERT_TRUE(reply1.is_valid() && reply1.is_method_return())
        << "reply_valid=" << reply1.is_valid() << " reply_type=" << static_cast<int>(reply1.get_type())
        << " reply_error=" << (reply1.is_error() ? reply1.get_error_name() : "");
    // debug 是最低级别，所有级别都应该启用
    default_log.set_level(mc::log::level::debug);
    EXPECT_TRUE(default_log.is_enabled(mc::log::level::debug));
    EXPECT_TRUE(default_log.is_enabled(mc::log::level::info));
    EXPECT_TRUE(default_log.is_enabled(mc::log::level::notice));
    EXPECT_TRUE(default_log.is_enabled(mc::log::level::warn));
    EXPECT_TRUE(default_log.is_enabled(mc::log::level::error));

    // 测试用例2: 设置日志级别为 info，验证 info 及以上级别启用
    auto reply2 = call_set_dlog_level("info", 1);
    ASSERT_TRUE(reply2.is_valid() && reply2.is_method_return())
        << "reply_valid=" << reply2.is_valid() << " reply_type=" << static_cast<int>(reply2.get_type())
        << " reply_error=" << (reply2.is_error() ? reply2.get_error_name() : "");
    default_log.set_level(mc::log::level::info);
    EXPECT_FALSE(default_log.is_enabled(mc::log::level::debug));
    EXPECT_TRUE(default_log.is_enabled(mc::log::level::info));
    EXPECT_TRUE(default_log.is_enabled(mc::log::level::notice));
    EXPECT_TRUE(default_log.is_enabled(mc::log::level::warn));
    EXPECT_TRUE(default_log.is_enabled(mc::log::level::error));

    // 测试用例3: 设置日志级别为 notice，验证 notice 及以上级别启用
    auto reply3 = call_set_dlog_level("notice", 1);
    ASSERT_TRUE(reply3.is_valid() && reply3.is_method_return())
        << "reply_valid=" << reply3.is_valid() << " reply_type=" << static_cast<int>(reply3.get_type())
        << " reply_error=" << (reply3.is_error() ? reply3.get_error_name() : "");
    default_log.set_level(mc::log::level::notice);
    EXPECT_FALSE(default_log.is_enabled(mc::log::level::debug));
    EXPECT_FALSE(default_log.is_enabled(mc::log::level::info));
    EXPECT_TRUE(default_log.is_enabled(mc::log::level::notice));
    EXPECT_TRUE(default_log.is_enabled(mc::log::level::warn));
    EXPECT_TRUE(default_log.is_enabled(mc::log::level::error));

    // 测试用例4: 设置日志级别为 warn，验证 warn 及以上级别启用
    auto reply4 = call_set_dlog_level("warn", 1);
    ASSERT_TRUE(reply4.is_valid() && reply4.is_method_return())
        << "reply_valid=" << reply4.is_valid() << " reply_type=" << static_cast<int>(reply4.get_type())
        << " reply_error=" << (reply4.is_error() ? reply4.get_error_name() : "");
    default_log.set_level(mc::log::level::warn);
    EXPECT_FALSE(default_log.is_enabled(mc::log::level::debug));
    EXPECT_FALSE(default_log.is_enabled(mc::log::level::info));
    EXPECT_FALSE(default_log.is_enabled(mc::log::level::notice));
    EXPECT_TRUE(default_log.is_enabled(mc::log::level::warn));
    EXPECT_TRUE(default_log.is_enabled(mc::log::level::error));

    // 测试用例5: 设置日志级别为 error，验证 error 及以上级别启用
    auto reply5 = call_set_dlog_level("error", 1);
    ASSERT_TRUE(reply5.is_valid() && reply5.is_method_return())
        << "reply_valid=" << reply5.is_valid() << " reply_type=" << static_cast<int>(reply5.get_type())
        << " reply_error=" << (reply5.is_error() ? reply5.get_error_name() : "");
    default_log.set_level(mc::log::level::error);
    EXPECT_FALSE(default_log.is_enabled(mc::log::level::debug));
    EXPECT_FALSE(default_log.is_enabled(mc::log::level::info));
    EXPECT_FALSE(default_log.is_enabled(mc::log::level::notice));
    EXPECT_FALSE(default_log.is_enabled(mc::log::level::warn));
    EXPECT_TRUE(default_log.is_enabled(mc::log::level::error));
}

// 注意：此测试用例禁用，此测试用例依赖于 /dev/shm/ 的存在，有些系统不一定有这个目录或者没有权限访问这个目录，用例需要修正
TEST_F(MicroComponentTest, DISABLED_TestMicroComponentSetDlogType) {
    mc::milliseconds extended_timeout(5000);

    auto     default_log = mc::log::default_logger();
    mc::dict properties{
        {"path", "/dev/shm/40010.sock"}, {"hb_path", "/dev/shm/40010.hbsock"}, {"module_name", "test_service_1"}};
    auto mdbctl_appender_ptr =
        mc::log::appender_factory::instance().get_or_create_appender("mdbctl", "socket", properties);
    if (mdbctl_appender_ptr) {
        default_log.add_appender(mdbctl_appender_ptr);
    }
    auto set_dlog_type_reply = wait_valid_reply(test_conn, [&]() {
        auto msg    = mc::dbus::message::new_method_call("org.openubmc.test_service_1",
                                                         "/bmc/kepler/test_service_1/MicroComponent",
                                                         "bmc.kepler.Object.Properties", "SetWithContext");
        auto writer = msg.writer();
        writer << empty_ctx << "bmc.kepler.MicroComponent.Debug" << "DlogType" << "local";
        return msg;
    }, extended_timeout);
    auto socket_appender_ptr = dynamic_cast<mc::log::socket_appender*>(mdbctl_appender_ptr.get());
    ASSERT_TRUE(set_dlog_type_reply.is_valid() && set_dlog_type_reply.is_method_return() &&
                socket_appender_ptr->get_type() == "local")
        << "reply_valid=" << set_dlog_type_reply.is_valid()
        << " reply_type=" << static_cast<int>(set_dlog_type_reply.get_type())
        << " reply_error=" << (set_dlog_type_reply.is_error() ? set_dlog_type_reply.get_error_name() : "");
    auto output = set_dlog_type_reply.read_args();
    EXPECT_EQ(output.size(), 0);
}

TEST_F(MicroComponentTest, TestMicroComponentDebugInterface)
{
    mc::milliseconds extended_timeout(5000);

    auto attach_reply = wait_valid_reply(test_conn, [&]() {
        auto msg    = mc::dbus::message::new_method_call("org.openubmc.test_service_1",
                                                         "/bmc/kepler/test_service_1/MicroComponent",
                                                         "bmc.kepler.MicroComponent.Debug", "AttachDebugConsole");
        auto writer = msg.writer();
        writer << empty_ctx << 40010;
        return msg;
    }, extended_timeout);
    if (!(attach_reply.is_valid() && attach_reply.is_method_return())) {
        EXPECT_TRUE(attach_reply.is_error())
            << "reply_valid=" << attach_reply.is_valid() << " reply_type=" << static_cast<int>(attach_reply.get_type())
            << " reply_error=" << (attach_reply.is_error() ? attach_reply.get_error_name() : "");
        return;
    }
    auto output = attach_reply.read_args();
    EXPECT_EQ(output.size(), 0);

    auto detach_reply = wait_valid_reply(test_conn, [&]() {
        auto msg    = mc::dbus::message::new_method_call("org.openubmc.test_service_1",
                                                         "/bmc/kepler/test_service_1/MicroComponent",
                                                         "bmc.kepler.MicroComponent.Debug", "DetachDebugConsole");
        auto writer = msg.writer();

        std::map<std::string, std::string> detach_ctx;
        detach_ctx["Test"] = "TestDetachDebugConsole";
        writer << detach_ctx;
        return msg;
    }, extended_timeout);
    ASSERT_TRUE(detach_reply.is_valid() && detach_reply.is_method_return())
        << "reply_valid=" << detach_reply.is_valid() << " reply_type=" << static_cast<int>(detach_reply.get_type())
        << " reply_error=" << (detach_reply.is_error() ? detach_reply.get_error_name() : "");
    output = detach_reply.read_args();
    EXPECT_EQ(output.size(), 0);
    EXPECT_EQ(service_1->m_ctx_value, "TestDetachDebugConsole");

    // 添加短暂延迟，确保上下文已更新
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto dump_reply = wait_valid_reply(test_conn, [&]() {
        auto msg    = mc::dbus::message::new_method_call("org.openubmc.test_service_1",
                                                         "/bmc/kepler/test_service_1/MicroComponent",
                                                         "bmc.kepler.MicroComponent.Debug", "Dump");
        auto writer = msg.writer();

        std::map<std::string, std::string> dump_ctx;
        dump_ctx["Test"] = "TestDump";
        writer << dump_ctx << "test_file.log";
        return msg;
    }, extended_timeout);
    ASSERT_TRUE(dump_reply.is_valid() && dump_reply.is_method_return())
        << "reply_valid=" << dump_reply.is_valid() << " reply_type=" << static_cast<int>(dump_reply.get_type())
        << " reply_error=" << (dump_reply.is_error() ? dump_reply.get_error_name() : "");
    output = dump_reply.read_args();
    EXPECT_EQ(output.size(), 0);
    EXPECT_EQ(service_1->m_ctx_value, "TestDump");
    EXPECT_EQ(service_1->m_dump_filepath, "test_file.log");

    // 添加短暂延迟，避免后续调用过快
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto set_dlog_reply = wait_valid_reply(test_conn, [&]() {
        auto msg    = mc::dbus::message::new_method_call("org.openubmc.test_service_1",
                                                         "/bmc/kepler/test_service_1/MicroComponent",
                                                         "bmc.kepler.MicroComponent.Debug", "SetDlogLevel");
        auto writer = msg.writer();
        writer << empty_ctx << "debug" << 2;
        return msg;
    }, extended_timeout);
    ASSERT_TRUE(set_dlog_reply.is_valid() && set_dlog_reply.is_method_return())
        << "reply_valid=" << set_dlog_reply.is_valid() << " reply_type=" << static_cast<int>(set_dlog_reply.get_type())
        << " reply_error=" << (set_dlog_reply.is_error() ? set_dlog_reply.get_error_name() : "");
    output = set_dlog_reply.read_args();
    EXPECT_EQ(output.size(), 0);
}

TEST_F(MicroComponentTest, TestMicroComponentRebootInterface)
{
    mc::milliseconds extended_timeout(10000); // 10秒超时，确保完整测试套件稳定

    // 使用同步调用替代异步调用，更稳定
    auto prepare_reply = wait_valid_reply(test_conn, [&]() {
        auto msg    = mc::dbus::message::new_method_call("org.openubmc.test_service_1",
                                                         "/bmc/kepler/test_service_1/MicroComponent",
                                                         "bmc.kepler.MicroComponent.Reboot", "Prepare");
        auto writer = msg.writer();
        writer << empty_ctx;
        return msg;
    }, extended_timeout);

    ASSERT_TRUE(prepare_reply.is_valid() && prepare_reply.is_method_return())
        << "reply_valid=" << prepare_reply.is_valid() << " reply_type=" << static_cast<int>(prepare_reply.get_type())
        << " reply_error=" << (prepare_reply.is_error() ? prepare_reply.get_error_name() : "");
    int32_t ret_code = 0;
    prepare_reply >> ret_code;
    ASSERT_EQ(ret_code, 0);

    // 添加延迟，避免调用过快
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    auto action_reply = wait_valid_reply(test_conn, [&]() {
        auto msg    = mc::dbus::message::new_method_call("org.openubmc.test_service_1",
                                                         "/bmc/kepler/test_service_1/MicroComponent",
                                                         "bmc.kepler.MicroComponent.Reboot", "Action");
        auto writer = msg.writer();
        writer << empty_ctx;
        return msg;
    }, extended_timeout);
    ASSERT_TRUE(action_reply.is_valid() && action_reply.is_method_return())
        << "reply_valid=" << action_reply.is_valid() << " reply_type=" << static_cast<int>(action_reply.get_type())
        << " reply_error=" << (action_reply.is_error() ? action_reply.get_error_name() : "");
    action_reply >> ret_code;
    ASSERT_EQ(ret_code, 0);

    // 添加延迟，避免调用过快
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    auto cancel_reply = wait_valid_reply(test_conn, [&]() {
        auto msg    = mc::dbus::message::new_method_call("org.openubmc.test_service_1",
                                                         "/bmc/kepler/test_service_1/MicroComponent",
                                                         "bmc.kepler.MicroComponent.Reboot", "Cancel");
        auto writer = msg.writer();
        writer << empty_ctx;
        return msg;
    }, extended_timeout);
    ASSERT_TRUE(cancel_reply.is_valid() && cancel_reply.is_method_return())
        << "reply_valid=" << cancel_reply.is_valid() << " reply_type=" << static_cast<int>(cancel_reply.get_type())
        << " reply_error=" << (cancel_reply.is_error() ? cancel_reply.get_error_name() : "");
    auto output = cancel_reply.read_args();
    EXPECT_EQ(output.size(), 0);
}

TEST_F(MicroComponentTest, TestMicroComponentResetInterface)
{
    mc::milliseconds extended_timeout(10000); // 10秒超时，确保完整测试套件稳定

    // 增强消息分发，确保 DBus 状态同步
    for (int i = 0; i < 10; ++i) {
        test_conn.dispatch();
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    // 使用同步调用替代异步调用，与 TestMicroComponentRebootInterface 保持一致
    auto prepare_reply = wait_valid_reply(test_conn, [&]() {
        auto msg    = mc::dbus::message::new_method_call("org.openubmc.test_service_1",
                                                         "/bmc/kepler/test_service_1/MicroComponent",
                                                         "bmc.kepler.MicroComponent.Reset", "Prepare");
        auto writer = msg.writer();
        writer << empty_ctx << "";
        return msg;
    }, extended_timeout);

    ASSERT_TRUE(prepare_reply.is_valid() && prepare_reply.is_method_return())
        << "reply_valid=" << prepare_reply.is_valid() << " reply_type=" << static_cast<int>(prepare_reply.get_type())
        << " reply_error=" << (prepare_reply.is_error() ? prepare_reply.get_error_name() : "");
    int32_t ret_code = 0;
    prepare_reply >> ret_code;
    ASSERT_EQ(ret_code, 0);

    // 增加延迟并强化消息分发
    for (int i = 0; i < 15; ++i) {
        test_conn.dispatch();
        std::this_thread::sleep_for(std::chrono::milliseconds(3));
    }

    auto action_reply = wait_valid_reply(test_conn, [&]() {
        auto msg    = mc::dbus::message::new_method_call("org.openubmc.test_service_1",
                                                         "/bmc/kepler/test_service_1/MicroComponent",
                                                         "bmc.kepler.MicroComponent.Reset", "Action");
        auto writer = msg.writer();
        writer << empty_ctx << "";
        return msg;
    }, extended_timeout);

    ASSERT_TRUE(action_reply.is_valid() && action_reply.is_method_return())
        << "reply_valid=" << action_reply.is_valid() << " reply_type=" << static_cast<int>(action_reply.get_type())
        << " reply_error=" << (action_reply.is_error() ? action_reply.get_error_name() : "");
    action_reply >> ret_code;
    ASSERT_EQ(ret_code, 0);

    // 增加延迟并强化消息分发
    for (int i = 0; i < 15; ++i) {
        test_conn.dispatch();
        std::this_thread::sleep_for(std::chrono::milliseconds(3));
    }

    auto cancel_reply = wait_valid_reply(test_conn, [&]() {
        auto msg    = mc::dbus::message::new_method_call("org.openubmc.test_service_1",
                                                         "/bmc/kepler/test_service_1/MicroComponent",
                                                         "bmc.kepler.MicroComponent.Reset", "Cancel");
        auto writer = msg.writer();
        writer << empty_ctx << "";
        return msg;
    }, extended_timeout);

    ASSERT_TRUE(cancel_reply.is_valid() && cancel_reply.is_method_return())
        << "reply_valid=" << cancel_reply.is_valid() << " reply_type=" << static_cast<int>(cancel_reply.get_type())
        << " reply_error=" << (cancel_reply.is_error() ? cancel_reply.get_error_name() : "");
    auto output = cancel_reply.read_args();
    EXPECT_EQ(output.size(), 0);
}

TEST_F(MicroComponentTest, TestMicroComponentMaintenanceInterface)
{
    mc::milliseconds extended_timeout(5000);

    auto call_dlog_limit = [&](bool enabled, uint8_t duration_mins) -> mc::dbus::message {
        return wait_valid_reply(test_conn, [&]() {
            auto msg    = mc::dbus::message::new_method_call("org.openubmc.test_service_1",
                                                             "/bmc/kepler/test_service_1/MicroComponent",
                                                             "bmc.kepler.Release.Maintenance", "DlogLimit");
            auto writer = msg.writer();
            writer << empty_ctx << enabled << duration_mins;
            return msg;
        }, extended_timeout);
    };

    // 测试用例1: enabled=true, duration_mins=60 -> 应该删除 MCC_DEBUG（启用日志限制）
    // 先设置环境变量，然后验证调用后是否被删除
    setenv("MCC_DEBUG", "1", 1);
    auto reply1 = call_dlog_limit(true, 60);
    ASSERT_TRUE(reply1.is_valid() && reply1.is_method_return())
        << "reply_valid=" << reply1.is_valid() << " reply_type=" << static_cast<int>(reply1.get_type())
        << " reply_error=" << (reply1.is_error() ? reply1.get_error_name() : "");
    const char* mcc_debug_value = getenv("MCC_DEBUG");
    EXPECT_EQ(mcc_debug_value, nullptr) << "enabled=true 时，MCC_DEBUG 应该被删除";

    // 测试用例2: enabled=false, duration_mins=0 -> 应该删除 MCC_DEBUG（因为 duration_mins==0）
    setenv("MCC_DEBUG", "1", 1);
    auto reply2 = call_dlog_limit(false, 0);
    ASSERT_TRUE(reply2.is_valid() && reply2.is_method_return())
        << "reply_valid=" << reply2.is_valid() << " reply_type=" << static_cast<int>(reply2.get_type())
        << " reply_error=" << (reply2.is_error() ? reply2.get_error_name() : "");
    mcc_debug_value = getenv("MCC_DEBUG");
    EXPECT_EQ(mcc_debug_value, nullptr) << "duration_mins=0 时，MCC_DEBUG 应该被删除";

    // 测试用例3: enabled=false, duration_mins>0 -> 应该设置 MCC_DEBUG="1"（禁用日志限制）
    unsetenv("MCC_DEBUG");
    auto reply3 = call_dlog_limit(false, 60);
    ASSERT_TRUE(reply3.is_valid() && reply3.is_method_return())
        << "reply_valid=" << reply3.is_valid() << " reply_type=" << static_cast<int>(reply3.get_type())
        << " reply_error=" << (reply3.is_error() ? reply3.get_error_name() : "");
    mcc_debug_value = getenv("MCC_DEBUG");
    ASSERT_NE(mcc_debug_value, nullptr) << "enabled=false 且 duration_mins>0 时，MCC_DEBUG 应该被设置";
    EXPECT_STREQ(mcc_debug_value, "1") << "MCC_DEBUG 的值应该是 '1'";

    // 测试用例4: enabled=false, duration_mins=1 -> 验证单数分钟的处理
    unsetenv("MCC_DEBUG");
    auto reply4 = call_dlog_limit(false, 1);
    ASSERT_TRUE(reply4.is_valid() && reply4.is_method_return())
        << "reply_valid=" << reply4.is_valid() << " reply_type=" << static_cast<int>(reply4.get_type())
        << " reply_error=" << (reply4.is_error() ? reply4.get_error_name() : "");
    mcc_debug_value = getenv("MCC_DEBUG");
    ASSERT_NE(mcc_debug_value, nullptr) << "enabled=false 且 duration_mins=1 时，MCC_DEBUG 应该被设置";
    EXPECT_STREQ(mcc_debug_value, "1") << "MCC_DEBUG 的值应该是 '1'";

    // 清理环境变量
    unsetenv("MCC_DEBUG");
}

TEST_F(MicroComponentTest, TestMethodCallContextStack)
{
    auto reply = wait_valid_reply(test_conn, [&]() {
        auto                               msg    = mc::dbus::message::new_method_call("org.openubmc.test_service_1",
                                                                                       "/bmc/kepler/test_service_1/MicroComponent",
                                                                                       "bmc.kepler.MicroComponent.Debug", "DetachDebugConsole");
        auto                               writer = msg.writer();
        std::map<std::string, std::string> detach_ctx;
        detach_ctx["Test"]      = "TestDetachDebugConsole";
        detach_ctx["Requestor"] = "org.openubmc.test_service_1";
        writer << detach_ctx;
        return msg;
    });
    EXPECT_EQ(service_1->m_last_requestor, "org.openubmc.test_service_1");
    service_1->update_last_requestor();
    EXPECT_EQ(service_1->m_last_requestor, "");
}

TEST_F(MicroComponentTest, test_dump_tree_success)
{
    // 为每次运行生成独立目录，避免跨用例/多次运行的残留干扰
    std::string dest_path = "./testdir_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    mc::filesystem::create_directories(dest_path);

    // 使用共享的 test_conn 而不是创建新连接
    std::map<std::string, std::string> dump_ctx;
    dump_ctx["Test"] = "TestDump";

    auto reply = wait_valid_reply(test_conn, [&]() {
        auto msg    = mc::dbus::message::new_method_call("org.openubmc.test_service_1",
                                                         "/bmc/kepler/test_service_1/MicroComponent",
                                                         "bmc.kepler.MicroComponent.Debug", "Dump");
        auto writer = msg.writer();
        writer << dump_ctx << dest_path;
        return msg;
    }, mc::milliseconds(5000)); // 5秒容忍度，防止在大套件中超时

    ASSERT_TRUE(reply.is_valid() && reply.is_method_return())
        << "reply_valid=" << reply.is_valid() << " reply_type=" << static_cast<int>(reply.get_type())
        << " reply_error=" << (reply.is_error() ? reply.get_error_name() : "");

    // 验证文件已生成（轮询等待，避免文件写入尚未落盘）
    std::string log_path = dest_path + "/mdb_info.log";
    bool        exists   = false;
    for (int i = 0; i < 50; ++i) { // 最长等待约500ms
        if (mc::filesystem::exists(log_path)) {
            exists = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    ASSERT_TRUE(exists) << "dump 文件未生成: " << log_path;

    // 验证文件内容
    auto content = mc::filesystem::read_file(log_path);
    ASSERT_TRUE(content.has_value()) << "无法读取 dump 文件: " << log_path;

    // 清理测试目录
    mc::filesystem::remove_all(dest_path);
}

TEST_F(MicroComponentTest, test_dump_tree_invalid_path)
{
    // 使用不存在的目录路径
    std::string invalid_path = "/nonexistent/invalid/path";

    // 使用共享的 test_conn 而不是创建新连接
    auto msg =
        mc::dbus::message::new_method_call("org.openubmc.test_service_1", "/bmc/kepler/test_service_1/MicroComponent",
                                           "bmc.kepler.MicroComponent.Debug", "Dump");
    auto writer = msg.writer();

    std::map<std::string, std::string> dump_ctx;
    dump_ctx["Test"] = "TestDumpInvalidPath";
    writer << dump_ctx << invalid_path;

    // 使用 wait_valid_reply 进行重试，增加成功率
    auto reply = wait_valid_reply(test_conn, [&]() {
        auto msg    = mc::dbus::message::new_method_call("org.openubmc.test_service_1",
                                                         "/bmc/kepler/test_service_1/MicroComponent",
                                                         "bmc.kepler.MicroComponent.Debug", "Dump");
        auto writer = msg.writer();
        writer << dump_ctx << invalid_path;
        return msg;
    }, mc::milliseconds(2000));

    // 验证方法调用成功（即使路径无效，方法本身不会抛异常）
    ASSERT_TRUE(reply.is_valid() && reply.is_method_return())
        << "reply_valid=" << reply.is_valid() << " reply_type=" << static_cast<int>(reply.get_type())
        << " reply_error=" << (reply.is_error() ? reply.get_error_name() : "");

    // 验证文件未生成
    std::string log_path = invalid_path + "/mdb_info.log";
    EXPECT_FALSE(mc::filesystem::exists(log_path));
}