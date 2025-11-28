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
#include <mc/engine/micro_component.h>
#include <mc/core/timer.h>
#include <mc/log.h>
#include <mc/log/appenders/socket_appender.h>
#include <mc/time.h>
#include <mc/engine/base.h>

#include <cstdlib>
#include <mutex>

using mc::log::socket_appender;
using mc::log::logger;
using mc::log::appender_factory;

namespace {
const std::string DEFAULT_DLOG_LEVEL = "notice";
const std::string DEFAULT_DLOG_TYPE  = "file";
const std::string DEFAULT_SOCKET_APPENDER_NAME  = "mdbctl";
std::string MODULE_NAME = "Unknown";
}

namespace mc::engine {
int32_t micro_component_interface::health_check(std::map<std::string, std::string> context) const {
    return 0;
}

std::vector<std::tuple<std::string, std::string>>
mc_config_manage_interface::backup(std::map<std::string, std::string> context, std::string filepath) {
    return {};
}

std::string mc_config_manage_interface::export_config(std::map<std::string, std::string> context, std::string type) {
    return "";
}

void mc_config_manage_interface::import_config(std::map<std::string, std::string> context, std::string data,
                                               std::string type) {
}

void mc_config_manage_interface::recover(std::map<std::string, std::string> context,
                                         std::map<std::string, std::string> preserve_list) {
}

std::string mc_config_manage_interface::verify(std::map<std::string, std::string> context, std::string data) {
    return "";
}

std::string mc_config_manage_interface::get_preserved_config(std::map<std::string, std::string> context,
                                                             std::map<std::string, std::string> preserve_flag) {
    return "";
}

std::string mc_config_manage_interface::get_trusted_config(std::map<std::string, std::string> context) {
    return "";
}

void mc_debug_interface::dump(std::map<std::string, std::string> context, std::string filepath) {
}

int32_t mc_reboot_interface::prepare(std::map<std::string, std::string> context) {
    auto service = get_service();
    return service->on_reboot_prepare(context);
}

int32_t mc_reboot_interface::process(std::map<std::string, std::string> context) {
    auto service = get_service();
    return service->on_reboot_process(context);
}

int32_t mc_reboot_interface::action(std::map<std::string, std::string> context) {
    auto service = get_service();
    return service->on_reboot_action(context);
}

void mc_reboot_interface::cancel(std::map<std::string, std::string> context) {
    auto service = get_service();
    service->on_reboot_cancel(context);
}

int32_t mc_reset_interface::prepare(std::map<std::string, std::string> context, std::string reset_type) {
    return 0;
}

int32_t mc_reset_interface::action(std::map<std::string, std::string> context, std::string reset_type) {
    return 0;
}

void mc_reset_interface::cancel(std::map<std::string, std::string> context, std::string reset_type) {
}

namespace {
std::mutex                      g_dlog_level_timer_mutex;
mc::core::timer_ptr             g_dlog_level_timer;
std::mutex                      g_dlog_limit_timer_mutex;
mc::core::timer_ptr             g_dlog_limit_timer;
std::mutex                      g_debug_console_mutex;
mc::core::timer_ptr             g_debug_console_timer;
std::weak_ptr<socket_appender>  g_debug_console_appender;

void stop_debug_console_heartbeat() {
    std::lock_guard<std::mutex> lock(g_debug_console_mutex);

    if (g_debug_console_timer) {
        g_debug_console_timer->stop();
        g_debug_console_timer.reset();
    }

    if (auto appender = g_debug_console_appender.lock()) {
        appender->disconnect();
    }

    g_debug_console_appender.reset();
}

void start_debug_console_heartbeat(mc_debug_interface* iface, const std::shared_ptr<socket_appender>& appender) {
    if (!iface || !appender) {
        return;
    }

    std::lock_guard<std::mutex> lock(g_debug_console_mutex);
    g_debug_console_appender = appender;

    if (!g_debug_console_timer) {
        g_debug_console_timer = mc::core::timer_ptr(new mc::core::timer());
        g_debug_console_timer->set_interval(mc::seconds(1));
        g_debug_console_timer->timeout.connect([iface]() {
            std::shared_ptr<socket_appender> locked_appender;
            {
                std::lock_guard<std::mutex> timer_lock(g_debug_console_mutex);
                locked_appender = g_debug_console_appender.lock();
            }

            if (!locked_appender) {
                return;
            }

            if (!locked_appender->heartbeat()) {
                iface->detach_debug_console({});
            }
        });
    }

    g_debug_console_timer->start(mc::seconds(1));
}
} // namespace

void mc_debug_interface::attach_debug_console(std::map<std::string, std::string> context, uint32_t port) {
    auto& default_log = mc::log::default_logger();
    auto          socket_path    = mc::string::concat("/dev/shm/", std::to_string(port), ".sock");
    auto          hb_socket_path = mc::string::concat("/dev/shm/", std::to_string(port), ".hbsock");

    auto appender = default_log.find_appender(DEFAULT_SOCKET_APPENDER_NAME);
    if (!appender) {
        mc::dict properties{{"path", socket_path}, {"hb_path", hb_socket_path}, {"module_name", MODULE_NAME}};
        appender = appender_factory::instance().get_or_create_appender(DEFAULT_SOCKET_APPENDER_NAME, "socket", properties);
        if (appender) {
            default_log.add_appender(appender);
        } else {
            elog("attach debug console ${port} failed, create socket appender failed", ("port", port));
            return;
        }
    }

    auto socket_appender_ptr = std::dynamic_pointer_cast<socket_appender>(appender);
    if (!socket_appender_ptr) {
        MC_THROW(mc::runtime_exception, "attach debug console ${port} failed, create socket appender failed", ("port", port));
    }

    if (socket_appender_ptr->is_connected()) {
        elog("attach debug console ${port} failed, already attached", ("port", port));
        MC_THROW(mc::busy_exception, "Resource is busy, module has already been attached or connection error occured.");
    }

    socket_appender_ptr->set_path(socket_path);
    socket_appender_ptr->set_hb_path(hb_socket_path);
    if (!socket_appender_ptr->connect()) {
        MC_THROW(mc::busy_exception, "Resource is busy, module has already been attached or connection error occured.");
    }
    ilog("attached debug console ${port}", ("port", port));
    
    socket_appender_ptr->set_type(this->m_dlog_type);
    start_debug_console_heartbeat(this, socket_appender_ptr);
    ilog("start heartbeat check of debug console ${port}", ("port", port));
}

void mc_debug_interface::detach_debug_console(std::map<std::string, std::string> context) {
    auto& default_log = mc::log::default_logger();
    this->m_dlog_type = DEFAULT_DLOG_TYPE;
    ilog("set debug log type to ${type}", ("type", DEFAULT_DLOG_TYPE));
    stop_debug_console_heartbeat();
    auto appender = default_log.find_appender(DEFAULT_SOCKET_APPENDER_NAME);
    if (auto socket_appender_ptr = std::dynamic_pointer_cast<socket_appender>(appender)) {
        socket_appender_ptr->set_type(DEFAULT_DLOG_TYPE);
        socket_appender_ptr->disconnect();
        default_log.remove_appender(socket_appender_ptr->get_name());
    }
}

static void set_log_level(mc::log::level lvl, std::string log_info) {
    mc::log::log_manager::instance().set_dlog_level(lvl);
    // 记录操作日志
    operation_log(log_info);
}

void mc_debug_interface::set_dlog_level(std::map<std::string, std::string> context, std::string level,
    uint8_t effective_hours) {
    auto lvl_opt = mc::log::to_level(level);
    if (lvl_opt == std::nullopt) {
        MC_THROW(mc::invalid_argument_exception, "Invalid log level: ${level}", ("level", level));
    }
    mc::log::level lvl = *lvl_opt;
    std::string log_info = mc::string::concat("Set debug log level to ", level, " successfully");

    {
        std::lock_guard<std::mutex> lock(g_dlog_level_timer_mutex);
        if (g_dlog_level_timer) {
            g_dlog_level_timer->stop();
            g_dlog_level_timer.reset();
        }
    }

    if (lvl < mc::log::level::notice) {
        std::string_view hour_str = (effective_hours == 1) ? "hour" : "hours";
        log_info = mc::string::concat(log_info, ", will set back to default level (notice) in ", std::to_string(effective_hours), " ", hour_str);
        set_log_level(lvl, log_info);
        this->m_dlog_level = level;
        {
            std::lock_guard<std::mutex> lock(g_dlog_level_timer_mutex);
            g_dlog_level_timer = mc::core::timer::single_shot(mc::hours(effective_hours), [this]() {
                set_log_level(mc::log::level::notice, std::string("Set debug log level back to default level (notice) successfully"));
                this->m_dlog_level = DEFAULT_DLOG_LEVEL;
                // 清理定时器引用，避免持有已完成的定时器对象
                std::lock_guard<std::mutex> lock(g_dlog_level_timer_mutex);
                g_dlog_level_timer.reset();
            });
        }
    } else {
        set_log_level(lvl, log_info);
        this->m_dlog_level = level;
    }
}

static void set_log_limit_env(bool enabled, std::string log_info) {
    if (!enabled) {
        setenv("MCC_DEBUG", "1", 1);
    } else {
        unsetenv("MCC_DEBUG");
    }
    operation_log(log_info);
}

void mc_maintenance_interface::dlog_limit(std::map<std::string, std::string> context, bool enabled,
                                          uint8_t duration_mins) {
    std::lock_guard<std::mutex> lock(g_dlog_limit_timer_mutex);
    if (g_dlog_limit_timer) {
        g_dlog_limit_timer->stop();
        g_dlog_limit_timer.reset();
    }

    std::string log_info = "Enable log limit successfully";
    if (enabled || duration_mins == 0) {
        set_log_limit_env(true, std::move(log_info));
        return;
    }
    log_info = mc::string::concat(
        "Disable log limit successfully, will resume in ",
        std::to_string(duration_mins),
        duration_mins > 1 ? " mins" : " min"
    );

    set_log_limit_env(false, log_info);
    auto duration = mc::minutes(static_cast<int64_t>(duration_mins));
    g_dlog_limit_timer = mc::core::timer::single_shot(duration, []() {
        set_log_limit_env(true, std::string("Resume log limit successfully"));
        // 清理定时器引用，避免持有已完成的定时器对象
        std::lock_guard<std::mutex> lock(g_dlog_limit_timer_mutex);
        g_dlog_limit_timer.reset();
    });
}

void mc_debug_interface::set_dlog_type(std::string type) {
    auto& default_log = mc::log::default_logger();
    auto appender = default_log.find_appender(DEFAULT_SOCKET_APPENDER_NAME);
    if (auto socket_appender_ptr = std::dynamic_pointer_cast<socket_appender>(appender)) {
        socket_appender_ptr->set_type(type);
        operation_log("set debug log output type to ${type} successfully", ("type", type));
    } else {
        operation_log("set debug log output type to ${type} failed, appender not found", ("type", type));
    }
}

void micro_component_object::init(std::string_view service_name) {
    size_t pos      = service_name.find_last_of('.');
    auto   app_name = service_name.substr(pos + 1);
    auto   object_name = mc::string::concat("MicroComponent_", app_name);
    auto   object_path = mc::string::concat("/bmc/kepler/", std::string(app_name), "/MicroComponent");
    set_object_name(object_name);
    set_object_path(object_path);
    m_mc_iface.m_name = std::string(app_name);
    MODULE_NAME = std::string(app_name);
    m_mc_iface.m_pid              = getpid();
    m_mc_iface.m_status           = "InitCompleted";
    m_mc_debug_iface.m_dlog_level = DEFAULT_DLOG_LEVEL;
    m_mc_debug_iface.m_dlog_type  = DEFAULT_DLOG_TYPE;
    m_mc_debug_iface.property_changed().connect([this](const mc::variant& value, const property_base& prop) {
        if (prop.get_name() == "DlogType") {
            m_mc_debug_iface.set_dlog_type(value.get_string());
        }
    });
}
} // namespace mc::engine

MC_REFLECT(mc::engine::micro_component_interface,
           ((m_pid, "Pid"))((m_author, "Author"))((m_description, "Description"))((m_license, "License"))(
               (m_name, "Name"))((m_status, "Status"))((m_version, "Version"))((health_check, "HealthCheck")))
MC_REFLECT(mc::engine::mc_config_manage_interface,
           ((backup, "Backup"))((export_config, "Export"))((import_config, "Import"))((recover, "Recover"))(
               (verify, "Verify"))((get_preserved_config, "GetPreservedConfig"))((get_trusted_config,
                                                                                  "GetTrustedConfig")))
MC_REFLECT(mc::engine::mc_debug_interface,
           ((m_dlog_level, "DlogLevel"))((m_dlog_type, "DlogType"))((attach_debug_console, "AttachDebugConsole"))(
               (detach_debug_console, "DetachDebugConsole"))((dump, "Dump"))((set_dlog_level, "SetDlogLevel")))
MC_REFLECT(mc::engine::mc_reboot_interface, ((prepare, "Prepare"))((process, "Process"))((action, "Action"))
                                            ((cancel, "Cancel")))
MC_REFLECT(mc::engine::mc_reset_interface, ((prepare, "Prepare"))((action, "Action"))((cancel, "Cancel")))
MC_REFLECT(mc::engine::mc_maintenance_interface, ((dlog_limit, "DlogLimit")))
MC_REFLECT(
    mc::engine::micro_component_object,
    ((m_mc_iface, "bmc.kepler.MicroComponent"))((m_mc_config_manage_iface, "bmc.kepler.MicroComponent.ConfigManage"))(
        (m_mc_debug_iface, "bmc.kepler.MicroComponent.Debug"))((m_mc_reboot_iface, "bmc.kepler.MicroComponent.Reboot"))(
        (m_mc_reset_iface, "bmc.kepler.MicroComponent.Reset"))((m_mc_maintenance_iface,
                                                                "bmc.kepler.Release.Maintenance")))