/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 * http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <mc/app/micro_component.h>

#include <mc/app/service.h>
#include <mc/engine/base.h>
#include <mc/engine/metadata.h>
#include <mc/engine/property/types.h>
#include <mc/exception.h>
#include <mc/filesystem.h>
#include <mc/im/key_buffer.h>
#include <mc/log/appender_factory.h>
#include <mc/log/log.h>
#include <mc/log/log_level.h>
#include <mc/log/log_manager.h>
#include <mc/log/logger.h>
#include <mc/string.h>
#include <mc/time.h>

#include <unistd.h>

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using mc::log::appender_factory;

namespace {

const mc::string DEFAULT_DLOG_LEVEL           = "notice";
const mc::string DEFAULT_DLOG_TYPE            = "file";
const mc::string DEFAULT_SOCKET_APPENDER_NAME = "mdbctl";
const mc::string MDBCTL_SOCKET_APPENDER_NAME  = "mdbctl_socket";

mc::string& module_name_storage()
{
    static mc::string s_name = "Unknown";
    return s_name;
}

mc::dict make_debug_console_properties(mc::string_view socket_path, mc::string_view hb_socket_path,
                                       mc::string_view type, bool auto_connect)
{
    return mc::dict{{"path", mc::string(socket_path)},      {"hb_path", mc::string(hb_socket_path)},
                    {"module_name", module_name_storage()}, {"type", mc::string(type)},
                    {"auto_connect", auto_connect},         {"heartbeat_interval_sec", static_cast<std::int64_t>(1)}};
}

void deactivate_debug_console_appender(const mc::log::appender_ptr& appender, mc::string_view socket_path,
                                       mc::string_view hb_socket_path)
{
    if (!appender || socket_path.empty() || hb_socket_path.empty()) {
        return;
    }
    appender->init(make_debug_console_properties(socket_path, hb_socket_path, DEFAULT_DLOG_TYPE, false));
}

std::set<mc::string> collect_child_segments(mc::engine::service_object_table& table, mc::string_view path)
{
    std::set<mc::string> segments;
    mc::string           prefix = path == "/" ? mc::string("/") : mc::string(path) + "/";
    table.with_index<mc::engine::by_path>([&](auto& idx) {
        for (auto it = idx.lower_bound(prefix); !it.is_end(); ++it) {
            mc::string_view k = it.key();
            if (!mc::im::has_prefix(k, mc::string_view(prefix))) {
                break;
            }
            mc::string_view rest      = k.substr(prefix.size());
            auto            slash_pos = rest.find('/');
            mc::string_view seg       = (slash_pos == mc::string_view::npos) ? rest : rest.substr(0, slash_pos);
            if (!seg.empty()) {
                segments.emplace(mc::string(seg));
            }
        }
    });
    return segments;
}

void dump_engine_subtree(std::ostream& os, mc::engine::service_object_table& table, const mc::string& path,
                         bool is_last, const std::string& prefix)
{
    os << prefix << (is_last ? "└─" : "├─") << path << "\n";

    std::string child_prefix = prefix + (is_last ? "  " : "│ ");
    auto        children     = collect_child_segments(table, path);
    const auto  n            = children.size();
    std::size_t i            = 0;
    for (const auto& seg : children) {
        mc::string child_path = (path == "/") ? ("/" + seg) : (path + "/" + seg);
        bool       last       = (++i == n);
        dump_engine_subtree(os, table, child_path, last, child_prefix);
    }
}

void dump_service_object_tree(std::ostream& os, mc::engine::service* svc)
{
    os << svc->name() << "\n";
    auto&       table = svc->get_object_table();
    auto        roots = collect_child_segments(table, "/");
    const auto  n     = roots.size();
    std::size_t i     = 0;
    for (const auto& seg : roots) {
        mc::string child_path = "/" + seg;
        bool       is_last    = (++i == n);
        dump_engine_subtree(os, table, child_path, is_last, "");
    }
}

void dump_interface_properties(std::ostream& os, mc::engine::abstract_object* node,
                               mc::engine::abstract_interface* iface, const mc::engine::interface_metadata& iface_meta)
{
    if (iface_meta.metadata == nullptr) {
        return;
    }

    std::vector<std::tuple<mc::string, mc::string_view, mc::string>> props;
    iface_meta.metadata->visit_properties([&](const mc::engine::property_type_info* prop_info) {
        auto value = iface->get_property(prop_info->name, mc::engine::property_options::memory);
        props.emplace_back(mc::string(prop_info->name), prop_info->get_signature(), value.to_string());
    });

    std::sort(props.begin(), props.end(), [](const auto& a, const auto& b) {
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

void dump_single_object_properties(std::ostream& os, mc::engine::abstract_object* node)
{
    const auto& metadata = node->get_metadata();
    metadata.visit_interfaces([&](const mc::engine::interface_metadata& iface_meta) {
        auto* iface = mc::engine::to_interface_ptr(node, iface_meta.interface);
        if (iface != nullptr) {
            dump_interface_properties(os, node, iface, iface_meta);
        }
    });
}

void dump_all_object_properties(std::ostream& os, mc::engine::service* svc)
{
    auto&                   table = svc->get_object_table();
    std::vector<mc::string> paths;
    table.with_index<mc::engine::by_path>([&](auto& idx) {
        for (auto it = idx.lower_bound(mc::string("/")); !it.is_end(); ++it) {
            paths.emplace_back(mc::string(it.key()));
        }
    });
    std::sort(paths.begin(), paths.end());
    for (const auto& p : paths) {
        auto it = table.find<mc::engine::by_path>(p);
        if (!it.is_end()) {
            auto* node = const_cast<mc::engine::abstract_object*>(&(*it));
            dump_single_object_properties(os, node);
        }
    }
}

void dump_service_tree(mc::engine::service* svc, mc::string_view filepath)
{
    if (svc == nullptr) {
        elog("dump service tree failed: service is nullptr");
        return;
    }

    if (!mc::filesystem::is_directory(filepath)) {
        elog("dump service tree failed: invalid directory path ${path}", ("path", filepath));
        return;
    }

    mc::string         log_path = mc::string::concat(filepath, "/mdb_info.log");
    std::ostringstream oss;

    dump_service_object_tree(oss, svc);
    oss << "\n";

    dump_all_object_properties(oss, svc);

    if (!mc::filesystem::write_file(log_path, oss.str())) {
        elog("dump service tree failed: cannot write mdb_info.log");
        return;
    }
}

} // namespace

namespace mc::app {

int32_t micro_component_interface::health_check() const
{
    return 0;
}

std::vector<std::tuple<mc::string, mc::string>> mc_config_manage_interface::backup(mc::string filepath)
{
    MC_UNUSED(filepath);
    return {};
}

mc::string mc_config_manage_interface::export_config(mc::string type)
{
    MC_UNUSED(type);
    return {};
}

void mc_config_manage_interface::import_config(mc::string data, mc::string type)
{
    MC_UNUSED(data);
    MC_UNUSED(type);
}

void mc_config_manage_interface::recover(mc::dict preserve_list)
{
    MC_UNUSED(preserve_list);
}

mc::string mc_config_manage_interface::verify(mc::string data)
{
    MC_UNUSED(data);
    return {};
}

mc::string mc_config_manage_interface::get_preserved_config(mc::dict preserve_flag)
{
    MC_UNUSED(preserve_flag);
    return {};
}

mc::string mc_config_manage_interface::get_trusted_config()
{
    return {};
}

void mc_debug_interface::dump(mc::string filepath)
{
    auto* service = get_service();
    dump_service_tree(service, filepath);
    service->on_dump(filepath);
}

int32_t mc_reboot_interface::prepare()
{
    auto* service = get_service();
    return service->on_reboot_prepare();
}

int32_t mc_reboot_interface::process()
{
    auto* service = get_service();
    return service->on_reboot_process();
}

int32_t mc_reboot_interface::action()
{
    auto* service = get_service();
    return service->on_reboot_action();
}

void mc_reboot_interface::cancel()
{
    auto* service = get_service();
    service->on_reboot_cancel();
}

int32_t mc_reset_interface::prepare(mc::string reset_type)
{
    MC_UNUSED(reset_type);
    return 0;
}

int32_t mc_reset_interface::action(mc::string reset_type)
{
    MC_UNUSED(reset_type);
    return 0;
}

void mc_reset_interface::cancel(mc::string reset_type)
{
    MC_UNUSED(reset_type);
}

mc_debug_interface::~mc_debug_interface()
{
    {
        std::lock_guard<std::mutex> lock(m_dlog_level_mutex);
        if (m_dlog_level_timer) {
            m_dlog_level_timer->stop();
            m_dlog_level_timer.reset();
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_debug_console_mutex);
        if (auto appender = m_debug_console_appender.lock()) {
            deactivate_debug_console_appender(appender, m_debug_socket_path, m_debug_hb_socket_path);
        }
        m_debug_console_appender.reset();
    }
}

mc_maintenance_interface::~mc_maintenance_interface()
{
    std::lock_guard<std::mutex> lock(m_dlog_limit_mutex);
    if (m_dlog_limit_timer) {
        m_dlog_limit_timer->stop();
        m_dlog_limit_timer.reset();
    }
}

void mc_debug_interface::attach_debug_console(uint32_t port)
{
    std::lock_guard<std::mutex> lock(m_debug_console_mutex);

    auto default_log    = mc::log::default_logger();
    auto socket_path    = mc::string::concat("/dev/shm/", std::to_string(port), ".sock");
    auto hb_socket_path = mc::string::concat("/dev/shm/", std::to_string(port), ".hbsock");

    m_debug_socket_path    = socket_path;
    m_debug_hb_socket_path = hb_socket_path;

    if (m_debug_console_appender.lock()) {
        elog("attach debug console ${port} failed, already attached", ("port", port));
        MC_THROW(mc::busy_exception, "Resource is busy, module has already been attached or connection error occured.");
    }

    mc::dict properties = make_debug_console_properties(socket_path, hb_socket_path, mc::string(m_dlog_type), true);

    auto appender =
        appender_factory::instance().get_or_create_appender(DEFAULT_SOCKET_APPENDER_NAME, "socket", properties);
    if (!appender) {
        elog("attach debug console ${port} failed, create socket appender failed", ("port", port));
        MC_THROW(mc::busy_exception, "Resource is busy, module has already been attached or connection error occured.");
    }
    if (!default_log.find_appender(DEFAULT_SOCKET_APPENDER_NAME)) {
        default_log.add_appender(appender);
    }
    ilog("attached debug console ${port}", ("port", port));

    m_debug_console_appender = appender;

    auto mdbctl_log = mc::log::logger::get(MC_LOG_MDBCTL_LOGGER);
    mdbctl_log.set_level(mc::log::level::debug);
    if (!mdbctl_log.find_appender(MDBCTL_SOCKET_APPENDER_NAME)) {
        mc::dict mdbctl_props{{"path", socket_path},
                              {"hb_path", hb_socket_path},
                              {"module_name", module_name_storage()},
                              {"type", mc::string("local")},
                              {"auto_connect", true},
                              {"heartbeat_interval_sec", static_cast<std::int64_t>(1)}};
        auto     mdbctl_socket =
            appender_factory::instance().get_or_create_appender(MDBCTL_SOCKET_APPENDER_NAME, "socket", mdbctl_props);
        if (mdbctl_socket) {
            mdbctl_log.add_appender(mdbctl_socket);
        }
    }
}

void mc_debug_interface::detach_debug_console()
{
    std::lock_guard<std::mutex> lock(m_debug_console_mutex);

    auto default_log  = mc::log::default_logger();
    this->m_dlog_type = DEFAULT_DLOG_TYPE;
    ilog("set debug log type to ${type}", ("type", DEFAULT_DLOG_TYPE));

    if (auto appender = m_debug_console_appender.lock()) {
        deactivate_debug_console_appender(appender, m_debug_socket_path, m_debug_hb_socket_path);
    }
    m_debug_console_appender.reset();

    if (auto appender = default_log.find_appender(DEFAULT_SOCKET_APPENDER_NAME)) {
        default_log.remove_appender(appender->get_name());
    }
    auto mdbctl_log = mc::log::logger::get(MC_LOG_MDBCTL_LOGGER);
    mdbctl_log.remove_appender(MDBCTL_SOCKET_APPENDER_NAME.view());

    m_debug_socket_path.clear();
    m_debug_hb_socket_path.clear();

    auto* service = get_service();
    service->on_detach_debug_console();
}

static void set_log_level(mc::log::level lvl, mc::string log_info)
{
    mc::log::log_manager::instance().set_dlog_level(lvl);
    operation_log("${info}", ("info", log_info));
}

void mc_debug_interface::set_dlog_level(mc::string level, uint8_t effective_hours)
{
    std::lock_guard<std::mutex> lock(m_dlog_level_mutex);

    auto lvl_opt = mc::log::to_level(level.view());
    if (lvl_opt == std::nullopt) {
        MC_THROW(mc::invalid_argument_exception, "Invalid log level: ${level}", ("level", level));
    }
    mc::log::level lvl      = *lvl_opt;
    mc::string     log_info = mc::string::concat("Set debug log level to ", level, " successfully");

    if (m_dlog_level_timer) {
        m_dlog_level_timer->stop();
        m_dlog_level_timer.reset();
    }

    if (lvl < mc::log::level::notice) {
        mc::string_view hour_str = (effective_hours == 1) ? "hour" : "hours";
        log_info                 = mc::string::concat(log_info, ", will set back to default level (notice) in ",
                                                      std::to_string(static_cast<int>(effective_hours)), " ", hour_str);
        set_log_level(lvl, log_info);
        this->m_dlog_level = level;

        m_dlog_level_timer = mc::timer::single_shot(mc::hours(effective_hours), [this]() {
            std::lock_guard<std::mutex> timer_lock(m_dlog_level_mutex);
            set_log_level(mc::log::level::notice,
                          mc::string("Set debug log level back to default level (notice) successfully"));
            this->m_dlog_level = DEFAULT_DLOG_LEVEL;
            m_dlog_level_timer.reset();
        });
    } else {
        set_log_level(lvl, log_info);
        this->m_dlog_level = level;
    }
}

static void set_log_limit_env(bool enabled, mc::string log_info)
{
    if (!enabled) {
        setenv("MCC_DEBUG", "1", 1);
    } else {
        unsetenv("MCC_DEBUG");
    }
    operation_log("${info}", ("info", log_info));
}

void mc_maintenance_interface::dlog_limit(bool enabled, uint8_t duration_mins)
{
    std::lock_guard<std::mutex> lock(m_dlog_limit_mutex);

    if (m_dlog_limit_timer) {
        m_dlog_limit_timer->stop();
        m_dlog_limit_timer.reset();
    }

    mc::string log_info = "Enable log limit successfully";
    if (enabled || duration_mins == 0) {
        set_log_limit_env(true, std::move(log_info));
        return;
    }
    log_info = mc::string::concat("Disable log limit successfully, will resume in ", std::to_string(duration_mins),
                                  duration_mins > 1 ? " mins" : " min");

    set_log_limit_env(false, log_info);
    auto duration = mc::minutes(static_cast<int64_t>(duration_mins));

    m_dlog_limit_timer = mc::timer::single_shot(duration, [this]() {
        std::lock_guard<std::mutex> timer_lock(m_dlog_limit_mutex);
        set_log_limit_env(true, mc::string("Resume log limit successfully"));
        m_dlog_limit_timer.reset();
    });
}

void mc_debug_interface::set_dlog_type(mc::string type)
{
    if (m_debug_socket_path.empty() || m_debug_hb_socket_path.empty()) {
        operation_log("set debug log output type to ${type} failed, appender not found", ("type", type));
        return;
    }
    mc::dict properties = make_debug_console_properties(m_debug_socket_path, m_debug_hb_socket_path, type, true);
    auto     appender =
        appender_factory::instance().get_or_create_appender(DEFAULT_SOCKET_APPENDER_NAME, "socket", properties);
    if (appender) {
        operation_log("set debug log output type to ${type} successfully", ("type", type));
    } else {
        operation_log("set debug log output type to ${type} failed, appender not found", ("type", type));
    }
}

void micro_component_object::init(mc::string_view service_name)
{
    const auto pos = service_name.find_last_of('.');
    auto       app_name =
        (pos == mc::string_view::npos) ? mc::string(service_name) : mc::string(service_name.substr(pos + 1));
    auto object_name = mc::string::concat("MicroComponent_", app_name);
    auto object_path = mc::string::concat("/bmc/kepler/", app_name, "/MicroComponent");
    set_object_name(object_name);
    set_object_path(object_path);
    m_mc_iface.m_name             = app_name;
    module_name_storage()         = app_name;
    m_mc_iface.m_pid              = static_cast<int32_t>(getpid());
    m_mc_iface.m_status           = "InitCompleted";
    m_mc_debug_iface.m_dlog_level = DEFAULT_DLOG_LEVEL;
    m_mc_debug_iface.m_dlog_type  = DEFAULT_DLOG_TYPE;
    m_mc_debug_iface.property_changed().connect(
        [this](const mc::variant& value, const mc::engine::property_base& prop) {
        if (prop.get_name() == "DlogType") {
            m_mc_debug_iface.set_dlog_type(mc::string(value.get_string()));
        }
    });
}

} // namespace mc::app

MC_REFLECT(mc::app::micro_component_interface,
           ((m_pid, "Pid"))((m_author, "Author"))((m_description, "Description"))((m_license, "License"))(
               (m_name, "Name"))((m_status, "Status"))((m_version, "Version"))((health_check, "HealthCheck")))
MC_REFLECT(mc::app::mc_config_manage_interface,
           ((backup, "Backup"))((export_config, "Export"))((import_config, "Import"))((recover, "Recover"))(
               (verify, "Verify"))((get_preserved_config, "GetPreservedConfig"))((get_trusted_config,
                                                                                  "GetTrustedConfig")))
MC_REFLECT(mc::app::mc_debug_interface,
           ((m_dlog_level, "DlogLevel"))((m_dlog_type, "DlogType"))((attach_debug_console, "AttachDebugConsole"))(
               (detach_debug_console, "DetachDebugConsole"))((dump, "Dump"))((set_dlog_level, "SetDlogLevel")))
MC_REFLECT(mc::app::mc_reboot_interface,
           ((prepare, "Prepare"))((process, "Process"))((action, "Action"))((cancel, "Cancel")))
MC_REFLECT(mc::app::mc_reset_interface, ((prepare, "Prepare"))((action, "Action"))((cancel, "Cancel")))
MC_REFLECT(mc::app::mc_maintenance_interface, ((dlog_limit, "DlogLimit")))
MC_REFLECT(
    mc::app::micro_component_object,
    ((m_mc_iface, "bmc.kepler.MicroComponent"))((m_mc_config_manage_iface, "bmc.kepler.MicroComponent.ConfigManage"))(
        (m_mc_debug_iface, "bmc.kepler.MicroComponent.Debug"))((m_mc_reboot_iface, "bmc.kepler.MicroComponent.Reboot"))(
        (m_mc_reset_iface, "bmc.kepler.MicroComponent.Reset"))((m_mc_maintenance_iface,
                                                                "bmc.kepler.Release.Maintenance")))
