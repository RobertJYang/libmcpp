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
#include <algorithm>
#include <memory>

#include <mc/core/timer.h>
#include <mc/engine/base.h>
#include <mc/engine/micro_component.h>
#include <mc/filesystem.h>
#include <mc/log.h>
#include <mc/log/appenders/socket_appender.h>
#include <mc/time.h>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <cstdlib>
#include <mutex>

using mc::log::appender_factory;
using mc::log::logger;
using mc::log::socket_appender;

namespace {
const std::string DEFAULT_DLOG_LEVEL            = "notice";
const std::string DEFAULT_DLOG_TYPE             = "file";
const std::string DEFAULT_SOCKET_APPENDER_NAME  = "mdbctl";
const std::string MDBCTL_SOCKET_APPENDER_NAME   = "mdbctl_socket";
std::string       MODULE_NAME                  = "Unknown";
} // namespace

namespace mc::engine {

namespace {

// 获取 DBus 路径下的排序子节点列表
std::vector<std::string> get_sorted_dbus_children(DBusConnection* conn, const std::string& path) {
    std::vector<std::string> children;
    char**                   child_entries = nullptr;

    if (!dbus_connection_list_registered(conn, path.c_str(), &child_entries)) {
        return children;
    }

    for (int i = 0; child_entries[i] != nullptr; ++i) {
        children.emplace_back(child_entries[i]);
    }
    dbus_free_string_array(child_entries);

    std::sort(children.begin(), children.end());
    return children;
}

// 递归输出 DBus 对象树拓扑结构
void dump_dbus_object_tree(std::ostream& os, DBusConnection* conn, const std::string& path,
                           bool is_last, const std::string& prefix) {
    // 输出当前节点路径
    os << prefix << (is_last ? "└─" : "├─") << path << "\n";

    // 计算子节点的前缀
    std::string child_prefix = prefix + (is_last ? "  " : "│ ");

    // 获取并排序子节点
    auto children = get_sorted_dbus_children(conn, path);

    // 递归遍历子节点
    for (size_t i = 0; i < children.size(); ++i) {
        bool        child_is_last = (i == children.size() - 1);
        std::string child_path    = (path == "/") ? ("/" + children[i]) : (path + "/" + children[i]);
        dump_dbus_object_tree(os, conn, child_path, child_is_last, child_prefix);
    }
}

// 输出资源树拓扑结构入口
void dump_object_tree(std::ostream& os, DBusConnection* conn, std::string_view service_name) {
    // 输出service名称作为根节点
    os << service_name << "\n";

    // 获取根路径下的子节点
    auto children = get_sorted_dbus_children(conn, "/");

    // 遍历根路径下的子节点
    for (size_t i = 0; i < children.size(); ++i) {
        bool        is_last    = (i == children.size() - 1);
        std::string child_path = "/" + children[i];
        dump_dbus_object_tree(os, conn, child_path, is_last, "");
    }
}

// 输出单个接口的属性信息
void dump_interface_properties(std::ostream& os, abstract_object* node, abstract_interface* iface,
                               const interface_metadata& iface_meta) {
    // 收集属性信息
    std::vector<std::tuple<std::string, std::string_view, std::string>> props;
    iface_meta.metadata->visit_properties([&](const property_type_info* prop_info) {
        auto value = iface->get_property(prop_info->name, property_options::memory);
        props.emplace_back(std::string(prop_info->name), prop_info->get_signature(), value.to_string());
    });

    std::sort(props.begin(), props.end(),
              [](const auto& a, const auto& b) {
        return std::get<0>(a) < std::get<0>(b);
    });

    os << node->get_object_path() << "  " << iface->get_interface_name() << "\n";

    for (const auto& [name, signature, value] : props) {
        os << "." << name << "\n";
        os << "  signature:  " << signature << "\n";
        os << "  value:      " << value << "\n";
        os << "  flags:      emits-change\n";
        os << "  readonly:   false\n";
    }

    os << "\n";
}

// 输出单个对象的所有接口属性
void dump_single_object_properties(std::ostream& os, abstract_object* node) {
    const auto& metadata = node->get_metadata();
    metadata.visit_interfaces([&](const interface_metadata& iface_meta) {
        auto* iface = to_interface_ptr(node, iface_meta.interface);
        if (iface != nullptr) {
            dump_interface_properties(os, node, iface, iface_meta);
        }
    });
}

// 递归输出对象属性（使用 DBus 遍历）
void dump_object_properties(std::ostream& os, DBusConnection* conn, service* svc,
                            const std::string& path) {
    // 尝试从对象表中查找对象并输出属性
    auto& table  = svc->get_object_table();
    auto  obj_it = table.find_object(by_path::field == path);

    if (obj_it) {
        auto* node = static_cast<abstract_object*>(obj_it.get());
        dump_single_object_properties(os, node);
    }

    // 获取子节点并递归遍历
    auto children = get_sorted_dbus_children(conn, path);
    for (const auto& child_name : children) {
        std::string child_path = (path == "/") ? ("/" + child_name) : (path + "/" + child_name);
        dump_object_properties(os, conn, svc, child_path);
    }
}

void dump_service_tree(service* svc, std::string_view filepath) {
    if (svc == nullptr) {
        elog("dump service tree failed: service is nullptr");
        return;
    }

    // 验证目录路径是否存在且是合法目录
    if (!mc::filesystem::is_directory(filepath)) {
        elog("dump service tree failed: invalid directory path ${path}", ("path", filepath));
        return;
    }

    // 获取 DBus 连接
    auto conn = svc->get_connection().get_connection();
    if (conn == nullptr) {
        elog("dump service tree failed: cannot get dbus connection");
        return;
    }

    std::string        log_path = std::string(filepath) + "/mdb_info.log";
    std::ostringstream oss;

    // 输出资源树拓扑结构
    dump_object_tree(oss, conn, svc->name());
    oss << "\n";

    // 输出各个接口的属性
    dump_object_properties(oss, conn, svc, "/");

    if (!mc::filesystem::write_file(log_path, oss.str())) {
        elog("dump service tree failed: cannot write mdb_info.log");
        return;
    }
}

} // namespace
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
    auto service = get_service();
    dump_service_tree(service, filepath);
    service->on_dump(context, filepath);
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

// mc_debug_interface 析构函数实现
mc_debug_interface::~mc_debug_interface() {
    // 停止所有 timer，确保在对象析构前清理
    {
        std::lock_guard<std::mutex> lock(m_dlog_level_mutex);
        if (m_dlog_level_timer) {
            m_dlog_level_timer->stop();
            m_dlog_level_timer.reset();
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_debug_console_mutex);
        if (m_debug_console_timer) {
            m_debug_console_timer->stop();
            m_debug_console_timer.reset();
        }

        if (auto appender = m_debug_console_appender.lock()) {
            appender->disconnect();
        }
        m_debug_console_appender.reset();
    }
}

// mc_maintenance_interface 析构函数实现
mc_maintenance_interface::~mc_maintenance_interface() {
    // 停止 timer，确保在对象析构前清理
    std::lock_guard<std::mutex> lock(m_dlog_limit_mutex);
    if (m_dlog_limit_timer) {
        m_dlog_limit_timer->stop();
        m_dlog_limit_timer.reset();
    }
}

void mc_debug_interface::attach_debug_console(std::map<std::string, std::string> context, uint32_t port) {
    std::lock_guard<std::mutex> lock(m_debug_console_mutex);

    auto default_log    = mc::log::default_logger();
    auto socket_path    = mc::string::concat("/dev/shm/", std::to_string(port), ".sock");
    auto hb_socket_path = mc::string::concat("/dev/shm/", std::to_string(port), ".hbsock");

    auto appender = default_log.find_appender(DEFAULT_SOCKET_APPENDER_NAME);
    if (!appender) {
        mc::dict properties{{"path", socket_path}, {"hb_path", hb_socket_path}, {"module_name", MODULE_NAME}};
        appender = appender_factory::instance().get_or_create_appender(DEFAULT_SOCKET_APPENDER_NAME, "socket", properties);
        if (!appender) {
            elog("attach debug console ${port} failed, create socket appender failed", ("port", port));
            return;
        }
        default_log.add_appender(appender);
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

    // 使用成员变量而非全局变量
    m_debug_console_appender = socket_appender_ptr;

    // 在 mdbctl_logger 中新增共享 socket_client 的 socket_appender
    auto mdbctl_log = mc::log::mdbctl_logger();
    mdbctl_log.set_level(mc::log::level::debug);
    auto mdbctl_appender = mdbctl_log.find_appender(MDBCTL_SOCKET_APPENDER_NAME);
    if (!mdbctl_appender) {
        auto shared_client = socket_appender_ptr->get_client_shared();
        auto mdbctl_socket = std::make_shared<socket_appender>(shared_client);
        mdbctl_socket->set_type("local");
        mdbctl_log.add_appender(mdbctl_appender);
    }

    if (!m_debug_console_timer) {
        m_debug_console_timer = mc::make_shared<mc::core::timer>();
        m_debug_console_timer->set_interval(mc::seconds(1));
        m_debug_console_timer->timeout.connect([this]() {
            auto locked_appender = m_debug_console_appender.lock();
            if (!locked_appender) {
                return;
            }

            if (!locked_appender->heartbeat()) {
                this->detach_debug_console({});
            }
        });
    }

    m_debug_console_timer->start(mc::seconds(1));
    ilog("start heartbeat check of debug console ${port}", ("port", port));
}

void mc_debug_interface::detach_debug_console(std::map<std::string, std::string> context) {
    std::lock_guard<std::mutex> lock(m_debug_console_mutex);

    auto default_log  = mc::log::default_logger();
    this->m_dlog_type = DEFAULT_DLOG_TYPE;
    ilog("set debug log type to ${type}", ("type", DEFAULT_DLOG_TYPE));

    // 停止心跳定时器
    if (m_debug_console_timer) {
        m_debug_console_timer->stop();
        m_debug_console_timer.reset();
    }

    if (auto appender = m_debug_console_appender.lock()) {
        appender->disconnect();
    }
    m_debug_console_appender.reset();

    auto appender = default_log.find_appender(DEFAULT_SOCKET_APPENDER_NAME);
    if (auto socket_appender_ptr = std::dynamic_pointer_cast<socket_appender>(appender)) {
        socket_appender_ptr->set_type(DEFAULT_DLOG_TYPE);
        socket_appender_ptr->disconnect();
        default_log.remove_appender(socket_appender_ptr->get_name());
        mc::log::mdbctl_logger().remove_appender(MDBCTL_SOCKET_APPENDER_NAME);
    }
    auto service = get_service();
    service->on_detach_debug_console(context);
}

static void set_log_level(mc::log::level lvl, std::string log_info) {
    mc::log::log_manager::instance().set_dlog_level(lvl);
    // 记录操作日志
    operation_log(log_info);
}

void mc_debug_interface::set_dlog_level(std::map<std::string, std::string> context, std::string level,
                                        uint8_t effective_hours) {
    std::lock_guard<std::mutex> lock(m_dlog_level_mutex);

    auto lvl_opt = mc::log::to_level(level);
    if (lvl_opt == std::nullopt) {
        MC_THROW(mc::invalid_argument_exception, "Invalid log level: ${level}", ("level", level));
    }
    mc::log::level lvl      = *lvl_opt;
    std::string    log_info = mc::string::concat("Set debug log level to ", level, " successfully");

    // 停止之前的定时器
    if (m_dlog_level_timer) {
        m_dlog_level_timer->stop();
        m_dlog_level_timer.reset();
    }

    if (lvl < mc::log::level::notice) {
        std::string_view hour_str = (effective_hours == 1) ? "hour" : "hours";
        log_info                  = mc::string::concat(log_info, ", will set back to default level (notice) in ", std::to_string(effective_hours), " ", hour_str);
        set_log_level(lvl, log_info);
        this->m_dlog_level = level;

        // 使用成员变量
        m_dlog_level_timer = mc::core::timer::single_shot(mc::hours(effective_hours), [this]() {
            std::lock_guard<std::mutex> timer_lock(m_dlog_level_mutex);
            set_log_level(mc::log::level::notice, std::string("Set debug log level back to default level (notice) successfully"));
            this->m_dlog_level = DEFAULT_DLOG_LEVEL;
            // 清理定时器引用，避免持有已完成的定时器对象
            m_dlog_level_timer.reset();
        });
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
    std::lock_guard<std::mutex> lock(m_dlog_limit_mutex);

    // 停止之前的定时器
    if (m_dlog_limit_timer) {
        m_dlog_limit_timer->stop();
        m_dlog_limit_timer.reset();
    }

    std::string log_info = "Enable log limit successfully";
    if (enabled || duration_mins == 0) {
        set_log_limit_env(true, std::move(log_info));
        return;
    }
    log_info = mc::string::concat(
        "Disable log limit successfully, will resume in ",
        std::to_string(duration_mins),
        duration_mins > 1 ? " mins" : " min");

    set_log_limit_env(false, log_info);
    auto duration = mc::minutes(static_cast<int64_t>(duration_mins));

    // 使用成员变量
    m_dlog_limit_timer = mc::core::timer::single_shot(duration, [this]() {
        std::lock_guard<std::mutex> timer_lock(m_dlog_limit_mutex);
        set_log_limit_env(true, std::string("Resume log limit successfully"));
        // 清理定时器引用，避免持有已完成的定时器对象
        m_dlog_limit_timer.reset();
    });
}

void mc_debug_interface::set_dlog_type(std::string type) {
    auto default_log = mc::log::default_logger();
    auto appender    = default_log.find_appender(DEFAULT_SOCKET_APPENDER_NAME);
    if (auto socket_appender_ptr = std::dynamic_pointer_cast<socket_appender>(appender)) {
        socket_appender_ptr->set_type(type);
        operation_log("set debug log output type to ${type} successfully", ("type", type));
    } else {
        operation_log("set debug log output type to ${type} failed, appender not found", ("type", type));
    }
}

void micro_component_object::init(std::string_view service_name) {
    size_t pos         = service_name.find_last_of('.');
    auto   app_name    = service_name.substr(pos + 1);
    auto   object_name = mc::string::concat("MicroComponent_", app_name);
    auto   object_path = mc::string::concat("/bmc/kepler/", std::string(app_name), "/MicroComponent");
    set_object_name(object_name);
    set_object_path(object_path);
    m_mc_iface.m_name             = std::string(app_name);
    MODULE_NAME                   = std::string(app_name);
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
MC_REFLECT(mc::engine::mc_reboot_interface, ((prepare, "Prepare"))((process, "Process"))((action, "Action"))((cancel, "Cancel")))
MC_REFLECT(mc::engine::mc_reset_interface, ((prepare, "Prepare"))((action, "Action"))((cancel, "Cancel")))
MC_REFLECT(mc::engine::mc_maintenance_interface, ((dlog_limit, "DlogLimit")))
MC_REFLECT(
    mc::engine::micro_component_object,
    ((m_mc_iface, "bmc.kepler.MicroComponent"))((m_mc_config_manage_iface, "bmc.kepler.MicroComponent.ConfigManage"))(
        (m_mc_debug_iface, "bmc.kepler.MicroComponent.Debug"))((m_mc_reboot_iface, "bmc.kepler.MicroComponent.Reboot"))(
        (m_mc_reset_iface, "bmc.kepler.MicroComponent.Reset"))((m_mc_maintenance_iface,
                                                                "bmc.kepler.Release.Maintenance")))