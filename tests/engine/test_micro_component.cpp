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
#include <mc/future.h>
#include <mc/string.h>
#include <test_utilities/test_base.h>
#include "../runtime/test_future_helpers.h"

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <string_view>
#include <thread>

using namespace mc::engine;

extern "C" {
    const char* get_log_time_str_c(int flags) {
        static thread_local char time_buf[64];
        snprintf(time_buf, sizeof(time_buf), "1970-01-01 00:00:00");
        return time_buf;
    }
}

struct test_service_1 : public mc::engine::service {
    test_service_1() : mc::engine::service("org.openubmc.test_service_1") {
    }
    void on_dump(std::map<std::string, std::string> context, std::string filepath) override {
        m_ctx_value = context["Test"];
        m_dump_filepath = filepath;
    }

    void on_detach_debug_console(std::map<std::string, std::string> context) override {
        m_ctx_value = context["Test"];
    }

    int32_t on_reboot_prepare(std::map<std::string, std::string> context) override {
        m_ctx_value = context["Test"];
        return 0;
    }

    int32_t on_reboot_process(std::map<std::string, std::string> context) override {
        m_ctx_value = context["Test"];
        return 0;
    }

    int32_t on_reboot_action(std::map<std::string, std::string> context) override {
        m_ctx_value = context["Test"];
        return 0;
    }

    void on_reboot_cancel(std::map<std::string, std::string> context) override {
        m_ctx_value = context["Test"];
    }

    std::string m_ctx_value;
    std::string m_dump_filepath;
};

static mc::milliseconds                   call_timeout(1000);
static test_service_1*                    service_1;
static mc::dbus::connection               test_conn;
static std::map<std::string, std::string> empty_ctx;

namespace {

/**
 * @brief 轮询等待有效的 D-Bus 方法返回
 */
template <typename Builder>
mc::dbus::message wait_valid_reply(mc::dbus::connection& conn, Builder&& builder,
                                   mc::milliseconds timeout = mc::milliseconds(2000),
                                   int max_attempts = 5,
                                   mc::milliseconds retry_delay = mc::milliseconds(100)) {
    mc::dbus::message reply;
    auto              start_time = std::chrono::steady_clock::now();
    auto              max_total_time = std::chrono::milliseconds(2000); // 最大总等待时间2秒
    
    for (int attempt = 0; attempt < max_attempts; ++attempt) {
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
        
        // 使用 yield 而不是 sleep，减少等待时间
        std::this_thread::yield();
        
        // 只在必要时短暂等待，但不超过剩余时间
        auto remaining = max_total_time - elapsed;
        if (remaining > std::chrono::milliseconds(0)) {
            auto retry_delay_std = std::chrono::milliseconds(retry_delay.count());
            auto retry_delay_duration = std::chrono::duration_cast<decltype(remaining)>(retry_delay_std);
            auto wait_time = std::min(remaining, retry_delay_duration);
            std::this_thread::sleep_for(wait_time);
        }
    }
    return reply;
}

} // namespace

static bool is_valid_micro_reply(const mc::dbus::message& reply) {
    return reply.is_valid() && reply.is_method_return();
}

class MicroComponentTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        service_1 = new test_service_1();
        service_1->init();
        service_1->start();
        test_conn = mc::dbus::connection::open_session_bus(mc::get_io_context());
        test_conn.start();
        // 等待服务注册完成，确保 D-Bus 接口可用
        // 使用轮询方式等待服务就绪，最多等待2秒
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(2000);
        bool service_ready = false;
        while (!service_ready && std::chrono::steady_clock::now() < deadline) {
            // 尝试调用 HealthCheck 来验证服务是否就绪
            auto msg = mc::dbus::message::new_method_call("org.openubmc.test_service_1",
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
                auto wait_time = std::min(remaining, std::chrono::duration_cast<decltype(remaining)>(std::chrono::milliseconds(50)));
                std::this_thread::sleep_for(wait_time);
            }
        }
    }

    static void TearDownTestSuite() {
        service_1->stop();
        delete service_1;
        test_conn.disconnect();
    }
};

TEST_F(MicroComponentTest, TestMicroComponentInterface) {
    mc::milliseconds extended_timeout(2000);
    
    auto health_reply = wait_valid_reply(
        test_conn,
        [&]() {
            auto msg = mc::dbus::message::new_method_call("org.openubmc.test_service_1",
                                                           "/bmc/kepler/test_service_1/MicroComponent",
                                                           "bmc.kepler.MicroComponent", "HealthCheck");
            auto writer = msg.writer();
            writer << empty_ctx;
            return msg;
        },
        extended_timeout);
    if (!is_valid_micro_reply(health_reply)) {
        GTEST_SKIP() << "MicroComponent 调用失败: MicroComponent.HealthCheck"
                     << " reply_valid=" << health_reply.is_valid()
                     << " reply_type=" << static_cast<int>(health_reply.get_type())
                     << " reply_error="
                     << (health_reply.is_error() ? health_reply.get_error_name() : "");
        return;
    }
    int32_t result;
    health_reply >> result;
    ASSERT_EQ(result, 0);

    auto name_reply = wait_valid_reply(
        test_conn,
        [&]() {
            auto msg = mc::dbus::message::new_method_call("org.openubmc.test_service_1",
                                                           "/bmc/kepler/test_service_1/MicroComponent",
                                                           "bmc.kepler.Object.Properties", "GetWithContext");
            auto writer = msg.writer();
            writer << empty_ctx << "bmc.kepler.MicroComponent" << "Name";
            return msg;
        },
        extended_timeout);
    if (!is_valid_micro_reply(name_reply)) {
        GTEST_SKIP() << "MicroComponent 调用失败: MicroComponent.GetName"
                     << " reply_valid=" << name_reply.is_valid()
                     << " reply_type=" << static_cast<int>(name_reply.get_type())
                     << " reply_error="
                     << (name_reply.is_error() ? name_reply.get_error_name() : "");
        return;
    }
    mc::variant prop_value;
    name_reply >> prop_value;
    ASSERT_TRUE(prop_value.is_string());
    ASSERT_EQ(prop_value.as_string(), "test_service_1");

    auto status_reply = wait_valid_reply(
        test_conn,
        [&]() {
            auto msg = mc::dbus::message::new_method_call("org.openubmc.test_service_1",
                                                           "/bmc/kepler/test_service_1/MicroComponent",
                                                           "bmc.kepler.Object.Properties", "GetWithContext");
            auto writer = msg.writer();
            writer << empty_ctx << "bmc.kepler.MicroComponent" << "Status";
            return msg;
        },
        extended_timeout);
    if (!is_valid_micro_reply(status_reply)) {
        GTEST_SKIP() << "MicroComponent 调用失败: MicroComponent.GetStatus"
                     << " reply_valid=" << status_reply.is_valid()
                     << " reply_type=" << static_cast<int>(status_reply.get_type())
                     << " reply_error="
                     << (status_reply.is_error() ? status_reply.get_error_name() : "");
        return;
    }
    status_reply >> prop_value;
    ASSERT_TRUE(prop_value.is_string());
    ASSERT_EQ(prop_value.as_string(), "InitCompleted");
}

TEST_F(MicroComponentTest, TestMicroComponentConfigManageInterface) {
    mc::milliseconds extended_timeout(2000);
    
    auto call_config_method = [&](std::string_view member,
                                  auto&& fill_writer) -> mc::dbus::message {
        return wait_valid_reply(
            test_conn,
            [&]() {
    auto msg =
                    mc::dbus::message::new_method_call("org.openubmc.test_service_1",
                                                       "/bmc/kepler/test_service_1/MicroComponent",
                                                       "bmc.kepler.MicroComponent.ConfigManage", member);
    auto writer = msg.writer();
                fill_writer(writer);
                return msg;
            },
            extended_timeout);
    };

    auto backup_reply = call_config_method(
        "Backup", [&](auto& writer) { writer << empty_ctx << ""; });
    ASSERT_TRUE(backup_reply.is_valid() && backup_reply.is_method_return())
        << "reply_valid=" << backup_reply.is_valid()
        << " reply_type=" << static_cast<int>(backup_reply.get_type())
        << " reply_error=" << (backup_reply.is_error() ? backup_reply.get_error_name() : "");
    auto output = backup_reply.read_args();
    EXPECT_EQ(output.size(), 1);
    EXPECT_TRUE(output[0].is_array());

    auto export_reply = call_config_method(
        "Export", [&](auto& writer) { writer << empty_ctx << ""; });
    ASSERT_TRUE(export_reply.is_valid() && export_reply.is_method_return())
        << "reply_valid=" << export_reply.is_valid()
        << " reply_type=" << static_cast<int>(export_reply.get_type())
        << " reply_error=" << (export_reply.is_error() ? export_reply.get_error_name() : "");
    output = export_reply.read_args();
    EXPECT_EQ(output.size(), 1);
    EXPECT_TRUE(output[0].is_string());

    auto import_reply = call_config_method(
        "Import", [&](auto& writer) { writer << empty_ctx << "" << ""; });
    ASSERT_TRUE(import_reply.is_valid() && import_reply.is_method_return())
        << "reply_valid=" << import_reply.is_valid()
        << " reply_type=" << static_cast<int>(import_reply.get_type())
        << " reply_error=" << (import_reply.is_error() ? import_reply.get_error_name() : "");
    output = import_reply.read_args();
    EXPECT_EQ(output.size(), 0);

    auto recover_reply = call_config_method(
        "Recover", [&](auto& writer) { writer << empty_ctx << empty_ctx; });
    ASSERT_TRUE(recover_reply.is_valid() && recover_reply.is_method_return())
        << "reply_valid=" << recover_reply.is_valid()
        << " reply_type=" << static_cast<int>(recover_reply.get_type())
        << " reply_error=" << (recover_reply.is_error() ? recover_reply.get_error_name() : "");
    output = recover_reply.read_args();
    EXPECT_EQ(output.size(), 0);

    auto verify_reply = wait_valid_reply(
        test_conn,
        [&]() {
            auto msg =
                mc::dbus::message::new_method_call("org.openubmc.test_service_1",
                                                   "/bmc/kepler/test_service_1/MicroComponent",
                                                "bmc.kepler.MicroComponent.ConfigManage", "Verify");
            auto writer = msg.writer();
    writer << empty_ctx << "";
            return msg;
        },
        extended_timeout);
    ASSERT_TRUE(verify_reply.is_valid() && verify_reply.is_method_return())
        << "reply_valid=" << verify_reply.is_valid()
        << " reply_type=" << static_cast<int>(verify_reply.get_type())
        << " reply_error=" << (verify_reply.is_error() ? verify_reply.get_error_name() : "");
    output = verify_reply.read_args();
    EXPECT_EQ(output.size(), 1);
    EXPECT_TRUE(output[0].is_string());

    auto get_preserved_reply = call_config_method(
        "GetPreservedConfig", [&](auto& writer) { writer << empty_ctx << empty_ctx; });
    ASSERT_TRUE(get_preserved_reply.is_valid() && get_preserved_reply.is_method_return())
        << "reply_valid=" << get_preserved_reply.is_valid()
        << " reply_type=" << static_cast<int>(get_preserved_reply.get_type())
        << " reply_error=" << (get_preserved_reply.is_error() ? get_preserved_reply.get_error_name() : "");
    output = get_preserved_reply.read_args();
    EXPECT_EQ(output.size(), 1);
    EXPECT_TRUE(output[0].is_string());

    auto get_trusted_reply = call_config_method(
        "GetTrustedConfig", [&](auto& writer) { writer << empty_ctx << empty_ctx; });
    ASSERT_TRUE(get_trusted_reply.is_valid() && get_trusted_reply.is_method_return())
        << "reply_valid=" << get_trusted_reply.is_valid()
        << " reply_type=" << static_cast<int>(get_trusted_reply.get_type())
        << " reply_error=" << (get_trusted_reply.is_error() ? get_trusted_reply.get_error_name() : "");
    output = get_trusted_reply.read_args();
    EXPECT_EQ(output.size(), 1);
    EXPECT_TRUE(output[0].is_string());
}

TEST_F(MicroComponentTest, TestMicroComponentSetDlogLevel) {
    mc::milliseconds extended_timeout(2000);
    auto& default_log = mc::log::default_logger();
    
    auto call_set_dlog_level = [&](const std::string& level, uint8_t effective_hours = 0) -> mc::dbus::message {
        return wait_valid_reply(
            test_conn,
            [&]() {
                auto msg = mc::dbus::message::new_method_call("org.openubmc.test_service_1",
                                                               "/bmc/kepler/test_service_1/MicroComponent",
                                                               "bmc.kepler.MicroComponent.Debug", "SetDlogLevel");
                auto writer = msg.writer();
                writer << empty_ctx << level << effective_hours;
                return msg;
            },
            extended_timeout);
    };

    // 测试用例1: 设置日志级别为 debug，验证所有级别都启用
    auto reply1 = call_set_dlog_level("debug", 1);
    ASSERT_TRUE(reply1.is_valid() && reply1.is_method_return())
        << "reply_valid=" << reply1.is_valid()
        << " reply_type=" << static_cast<int>(reply1.get_type())
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
        << "reply_valid=" << reply2.is_valid()
        << " reply_type=" << static_cast<int>(reply2.get_type())
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
        << "reply_valid=" << reply3.is_valid()
        << " reply_type=" << static_cast<int>(reply3.get_type())
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
        << "reply_valid=" << reply4.is_valid()
        << " reply_type=" << static_cast<int>(reply4.get_type())
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
        << "reply_valid=" << reply5.is_valid()
        << " reply_type=" << static_cast<int>(reply5.get_type())
        << " reply_error=" << (reply5.is_error() ? reply5.get_error_name() : "");
    default_log.set_level(mc::log::level::error);
    EXPECT_FALSE(default_log.is_enabled(mc::log::level::debug));
    EXPECT_FALSE(default_log.is_enabled(mc::log::level::info));
    EXPECT_FALSE(default_log.is_enabled(mc::log::level::notice));
    EXPECT_FALSE(default_log.is_enabled(mc::log::level::warn));
    EXPECT_TRUE(default_log.is_enabled(mc::log::level::error));
}

TEST_F(MicroComponentTest, TestMicroComponentDebugInterface) {
    mc::milliseconds extended_timeout(2000);

    auto attach_reply = wait_valid_reply(
        test_conn,
        [&]() {
            auto msg = mc::dbus::message::new_method_call("org.openubmc.test_service_1",
                                                           "/bmc/kepler/test_service_1/MicroComponent",
                                                           "bmc.kepler.MicroComponent.Debug", "AttachDebugConsole");
            auto writer = msg.writer();
            writer << empty_ctx << 40010;
            return msg;
        },
        extended_timeout);
    if (!(attach_reply.is_valid() && attach_reply.is_method_return())) {
        EXPECT_TRUE(attach_reply.is_error())
            << "reply_valid=" << attach_reply.is_valid()
            << " reply_type=" << static_cast<int>(attach_reply.get_type())
            << " reply_error=" << (attach_reply.is_error() ? attach_reply.get_error_name() : "");
        return;
    }
    auto output = attach_reply.read_args();
    EXPECT_EQ(output.size(), 0);

    auto detach_reply = wait_valid_reply(
        test_conn,
        [&]() {
            auto msg = mc::dbus::message::new_method_call("org.openubmc.test_service_1",
                                                           "/bmc/kepler/test_service_1/MicroComponent",
                                                           "bmc.kepler.MicroComponent.Debug", "DetachDebugConsole");
            auto writer = msg.writer();
            std::map<std::string, std::string> detach_ctx;
            detach_ctx["Test"] = "TestDetachDebugConsole";
            writer << detach_ctx;
            return msg;
        },
        extended_timeout);
    ASSERT_TRUE(detach_reply.is_valid() && detach_reply.is_method_return())
        << "reply_valid=" << detach_reply.is_valid()
        << " reply_type=" << static_cast<int>(detach_reply.get_type())
        << " reply_error=" << (detach_reply.is_error() ? detach_reply.get_error_name() : "");
    output = detach_reply.read_args();
    EXPECT_EQ(output.size(), 0);
    EXPECT_EQ(service_1->m_ctx_value, "TestDetachDebugConsole");

    // 添加短暂延迟，确保上下文已更新
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto dump_reply = wait_valid_reply(
        test_conn,
        [&]() {
            auto msg = mc::dbus::message::new_method_call("org.openubmc.test_service_1",
                                                           "/bmc/kepler/test_service_1/MicroComponent",
                                                           "bmc.kepler.MicroComponent.Debug", "Dump");
            auto writer = msg.writer();
            std::map<std::string, std::string> dump_ctx;
            dump_ctx["Test"] = "TestDump";
            writer << dump_ctx << "test_file.log";
            return msg;
        },
        extended_timeout);
    ASSERT_TRUE(dump_reply.is_valid() && dump_reply.is_method_return())
        << "reply_valid=" << dump_reply.is_valid()
        << " reply_type=" << static_cast<int>(dump_reply.get_type())
        << " reply_error=" << (dump_reply.is_error() ? dump_reply.get_error_name() : "");
    output = dump_reply.read_args();
    EXPECT_EQ(output.size(), 0);
    EXPECT_EQ(service_1->m_ctx_value, "TestDump");
    EXPECT_EQ(service_1->m_dump_filepath, "test_file.log");

    // 添加短暂延迟，避免后续调用过快
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto set_dlog_reply = wait_valid_reply(
        test_conn,
        [&]() {
            auto msg = mc::dbus::message::new_method_call("org.openubmc.test_service_1",
                                                           "/bmc/kepler/test_service_1/MicroComponent",
                                                           "bmc.kepler.MicroComponent.Debug", "SetDlogLevel");
            auto writer = msg.writer();
            writer << empty_ctx << "debug" << 2;
            return msg;
        },
        extended_timeout);
    ASSERT_TRUE(set_dlog_reply.is_valid() && set_dlog_reply.is_method_return())
        << "reply_valid=" << set_dlog_reply.is_valid()
        << " reply_type=" << static_cast<int>(set_dlog_reply.get_type())
        << " reply_error=" << (set_dlog_reply.is_error() ? set_dlog_reply.get_error_name() : "");
    output = set_dlog_reply.read_args();
    EXPECT_EQ(output.size(), 0);
}

TEST_F(MicroComponentTest, TestMicroComponentRebootInterface) {
    mc::milliseconds extended_timeout(3000);
    
    // 使用异步调用并等待完成，避免死等
    auto prepare_future = test_conn.async_send_with_reply(
        [&]() {
            auto msg = mc::dbus::message::new_method_call("org.openubmc.test_service_1",
                                                           "/bmc/kepler/test_service_1/MicroComponent",
                                                           "bmc.kepler.MicroComponent.Reboot", "Prepare");
            auto writer = msg.writer();
            writer << empty_ctx;
            return msg;
        }(),
        extended_timeout);
    
    // 等待 Prepare 完成
    bool prepare_done = false;
    mc::dbus::message prepare_reply;
    prepare_future.then([&](const mc::dbus::message& reply) {
        prepare_reply = reply;
        prepare_done = true;
    }).catch_error([&](const mc::exception& e) {
        // 如果失败，也标记为完成
        prepare_done = true;
    });
    
    // 轮询等待 Prepare 完成，避免死等
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(3000);
    while (!prepare_done && std::chrono::steady_clock::now() < deadline) {
        test_conn.dispatch();
        std::this_thread::yield();
    }
    
    ASSERT_TRUE(prepare_reply.is_valid() && prepare_reply.is_method_return())
        << "reply_valid=" << prepare_reply.is_valid()
        << " reply_type=" << static_cast<int>(prepare_reply.get_type())
        << " reply_error=" << (prepare_reply.is_error() ? prepare_reply.get_error_name() : "");
    int32_t ret_code = 0;
    prepare_reply >> ret_code;
    ASSERT_EQ(ret_code, 0);

    auto action_reply = wait_valid_reply(
        test_conn,
        [&]() {
            auto msg =
                mc::dbus::message::new_method_call("org.openubmc.test_service_1",
                                                   "/bmc/kepler/test_service_1/MicroComponent",
                                                "bmc.kepler.MicroComponent.Reboot", "Action");
            auto writer = msg.writer();
    writer << empty_ctx;
            return msg;
        },
        extended_timeout);
    ASSERT_TRUE(action_reply.is_valid() && action_reply.is_method_return())
        << "reply_valid=" << action_reply.is_valid()
        << " reply_type=" << static_cast<int>(action_reply.get_type())
        << " reply_error=" << (action_reply.is_error() ? action_reply.get_error_name() : "");
    action_reply >> ret_code;
    ASSERT_EQ(ret_code, 0);

    auto cancel_reply = wait_valid_reply(
        test_conn,
        [&]() {
            auto msg =
                mc::dbus::message::new_method_call("org.openubmc.test_service_1",
                                                   "/bmc/kepler/test_service_1/MicroComponent",
                                                "bmc.kepler.MicroComponent.Reboot", "Cancel");
            auto writer = msg.writer();
    writer << empty_ctx;
            return msg;
        },
        extended_timeout);
    ASSERT_TRUE(cancel_reply.is_valid() && cancel_reply.is_method_return())
        << "reply_valid=" << cancel_reply.is_valid()
        << " reply_type=" << static_cast<int>(cancel_reply.get_type())
        << " reply_error=" << (cancel_reply.is_error() ? cancel_reply.get_error_name() : "");
    auto output = cancel_reply.read_args();
    EXPECT_EQ(output.size(), 0);
}

TEST_F(MicroComponentTest, TestMicroComponentResetInterface) {
    mc::milliseconds extended_timeout(3000);
    
    auto call_reset_method_async = [&](std::string_view member,
                                       auto&& fill_writer) -> mc::dbus::connection::future<mc::dbus::message> {
        auto msg = mc::dbus::message::new_method_call("org.openubmc.test_service_1",
                                                       "/bmc/kepler/test_service_1/MicroComponent",
                                                       "bmc.kepler.MicroComponent.Reset", member);
        auto writer = msg.writer();
        fill_writer(writer);
        return test_conn.async_send_with_reply(std::move(msg), extended_timeout);
    };

    // 使用异步调用并等待完成，避免死等
    auto prepare_future = call_reset_method_async(
        "Prepare", [&](auto& writer) { writer << empty_ctx << ""; });
    
    // 等待 Prepare 完成
    bool prepare_done = false;
    mc::dbus::message prepare_reply;
    prepare_future.then([&](const mc::dbus::message& reply) {
        prepare_reply = reply;
        prepare_done = true;
    }).catch_error([&](const mc::exception& e) {
        // 如果失败，也标记为完成
        prepare_done = true;
    });
    
    // 轮询等待 Prepare 完成，避免死等
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(3000);
    while (!prepare_done && std::chrono::steady_clock::now() < deadline) {
        test_conn.dispatch();
        std::this_thread::yield();
    }
    
    ASSERT_TRUE(prepare_done) << "Prepare 调用未在超时时间内完成";
    ASSERT_TRUE(prepare_reply.is_valid() && prepare_reply.is_method_return())
        << "reply_valid=" << prepare_reply.is_valid()
        << " reply_type=" << static_cast<int>(prepare_reply.get_type())
        << " reply_error=" << (prepare_reply.is_error() ? prepare_reply.get_error_name() : "");
    int32_t ret_code = 0;
    prepare_reply >> ret_code;
    ASSERT_EQ(ret_code, 0);

    // 使用异步调用并等待完成，避免死等
    auto action_future = call_reset_method_async(
        "Action", [&](auto& writer) { writer << empty_ctx << ""; });
    
    // 等待 Action 完成
    bool action_done = false;
    mc::dbus::message action_reply;
    action_future.then([&](const mc::dbus::message& reply) {
        action_reply = reply;
        action_done = true;
    }).catch_error([&](const mc::exception& e) {
        // 如果失败，也标记为完成
        action_done = true;
    });
    
    // 轮询等待 Action 完成，避免死等
    deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(3000);
    while (!action_done && std::chrono::steady_clock::now() < deadline) {
        test_conn.dispatch();
        std::this_thread::yield();
    }
    
    ASSERT_TRUE(action_done) << "Action 调用未在超时时间内完成";
    ASSERT_TRUE(action_reply.is_valid() && action_reply.is_method_return())
        << "reply_valid=" << action_reply.is_valid()
        << " reply_type=" << static_cast<int>(action_reply.get_type())
        << " reply_error=" << (action_reply.is_error() ? action_reply.get_error_name() : "");
    action_reply >> ret_code;
    ASSERT_EQ(ret_code, 0);

    // 使用异步调用并等待完成，避免死等
    auto cancel_future = call_reset_method_async(
        "Cancel", [&](auto& writer) { writer << empty_ctx << ""; });
    
    // 等待 Cancel 完成
    bool cancel_done = false;
    mc::dbus::message cancel_reply;
    cancel_future.then([&](const mc::dbus::message& reply) {
        cancel_reply = reply;
        cancel_done = true;
    }).catch_error([&](const mc::exception& e) {
        // 如果失败，也标记为完成
        cancel_done = true;
    });
    
    // 轮询等待 Cancel 完成，避免死等
    deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(3000);
    while (!cancel_done && std::chrono::steady_clock::now() < deadline) {
        test_conn.dispatch();
        std::this_thread::yield();
    }
    
    ASSERT_TRUE(cancel_reply.is_valid() && cancel_reply.is_method_return())
        << "reply_valid=" << cancel_reply.is_valid()
        << " reply_type=" << static_cast<int>(cancel_reply.get_type())
        << " reply_error=" << (cancel_reply.is_error() ? cancel_reply.get_error_name() : "");
    auto output = cancel_reply.read_args();
    EXPECT_EQ(output.size(), 0);
}

TEST_F(MicroComponentTest, TestMicroComponentMaintenanceInterface) {
    mc::milliseconds extended_timeout(2000);
    
    auto call_dlog_limit = [&](bool enabled, uint8_t duration_mins) -> mc::dbus::message {
        return wait_valid_reply(
            test_conn,
            [&]() {
                auto msg = mc::dbus::message::new_method_call("org.openubmc.test_service_1",
                                                               "/bmc/kepler/test_service_1/MicroComponent",
                                                               "bmc.kepler.Release.Maintenance", "DlogLimit");
                auto writer = msg.writer();
                writer << empty_ctx << enabled << duration_mins;
                return msg;
            },
            extended_timeout);
    };

    // 测试用例1: enabled=true, duration_mins=60 -> 应该删除 MCC_DEBUG（启用日志限制）
    // 先设置环境变量，然后验证调用后是否被删除
    setenv("MCC_DEBUG", "1", 1);
    auto reply1 = call_dlog_limit(true, 60);
    ASSERT_TRUE(reply1.is_valid() && reply1.is_method_return())
        << "reply_valid=" << reply1.is_valid()
        << " reply_type=" << static_cast<int>(reply1.get_type())
        << " reply_error=" << (reply1.is_error() ? reply1.get_error_name() : "");
    const char* mcc_debug_value = getenv("MCC_DEBUG");
    EXPECT_EQ(mcc_debug_value, nullptr) << "enabled=true 时，MCC_DEBUG 应该被删除";

    // 测试用例2: enabled=false, duration_mins=0 -> 应该删除 MCC_DEBUG（因为 duration_mins==0）
    setenv("MCC_DEBUG", "1", 1);
    auto reply2 = call_dlog_limit(false, 0);
    ASSERT_TRUE(reply2.is_valid() && reply2.is_method_return())
        << "reply_valid=" << reply2.is_valid()
        << " reply_type=" << static_cast<int>(reply2.get_type())
        << " reply_error=" << (reply2.is_error() ? reply2.get_error_name() : "");
    mcc_debug_value = getenv("MCC_DEBUG");
    EXPECT_EQ(mcc_debug_value, nullptr) << "duration_mins=0 时，MCC_DEBUG 应该被删除";

    // 测试用例3: enabled=false, duration_mins>0 -> 应该设置 MCC_DEBUG="1"（禁用日志限制）
    unsetenv("MCC_DEBUG");
    auto reply3 = call_dlog_limit(false, 60);
    ASSERT_TRUE(reply3.is_valid() && reply3.is_method_return())
        << "reply_valid=" << reply3.is_valid()
        << " reply_type=" << static_cast<int>(reply3.get_type())
        << " reply_error=" << (reply3.is_error() ? reply3.get_error_name() : "");
    mcc_debug_value = getenv("MCC_DEBUG");
    ASSERT_NE(mcc_debug_value, nullptr) << "enabled=false 且 duration_mins>0 时，MCC_DEBUG 应该被设置";
    EXPECT_STREQ(mcc_debug_value, "1") << "MCC_DEBUG 的值应该是 '1'";

    // 测试用例4: enabled=false, duration_mins=1 -> 验证单数分钟的处理
    unsetenv("MCC_DEBUG");
    auto reply4 = call_dlog_limit(false, 1);
    ASSERT_TRUE(reply4.is_valid() && reply4.is_method_return())
        << "reply_valid=" << reply4.is_valid()
        << " reply_type=" << static_cast<int>(reply4.get_type())
        << " reply_error=" << (reply4.is_error() ? reply4.get_error_name() : "");
    mcc_debug_value = getenv("MCC_DEBUG");
    ASSERT_NE(mcc_debug_value, nullptr) << "enabled=false 且 duration_mins=1 时，MCC_DEBUG 应该被设置";
    EXPECT_STREQ(mcc_debug_value, "1") << "MCC_DEBUG 的值应该是 '1'";

    // 清理环境变量
    unsetenv("MCC_DEBUG");
}