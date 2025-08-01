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

#ifndef MICRO_COMPONENT_H
#define MICRO_COMPONENT_H

#include <mc/dict.h>
#include <mc/engine.h>
#include <mc/variant.h>

namespace mc::engine {

struct micro_component_interface : public mc::engine::interface<micro_component_interface> {
    MC_INTERFACE("bmc.kepler.MicroComponent")

    ~micro_component_interface() override = default;

    int32_t health_check(std::map<std::string, std::string> context) const;

    property<int32_t>     m_pid;
    property<std::string> m_author;
    property<std::string> m_description;
    property<std::string> m_license;
    property<std::string> m_name;
    property<std::string> m_status;
    property<std::string> m_version;
};

struct mc_config_manage_interface : public mc::engine::interface<mc_config_manage_interface> {
    MC_INTERFACE("bmc.kepler.MicroComponent.ConfigManage")

    ~mc_config_manage_interface() override = default;

    std::vector<std::tuple<std::string, std::string>> backup(std::map<std::string, std::string> context,
                                                             std::string                        filepath);

    std::string export_config(std::map<std::string, std::string> context, std::string type);
    void        import_config(std::map<std::string, std::string> context, std::string data, std::string type);
    void        recover(std::map<std::string, std::string> context, std::map<std::string, std::string> preserve_list);
    std::string verify(std::map<std::string, std::string> context, std::string data);
    std::string get_preserved_config(std::map<std::string, std::string> context,
                                     std::map<std::string, std::string> preserve_flag);
    std::string get_trusted_config(std::map<std::string, std::string> context);
};

struct mc_debug_interface : public mc::engine::interface<mc_debug_interface> {
    MC_INTERFACE("bmc.kepler.MicroComponent.Debug")

    ~mc_debug_interface() override = default;

    void attach_debug_console(std::map<std::string, std::string> context, uint32_t port);
    void detach_debug_console(std::map<std::string, std::string> context);
    void dump(std::map<std::string, std::string> context, std::string filepath);
    void set_dlog_level(std::map<std::string, std::string> context, std::string level, uint8_t effective_hours);

    property<std::string> m_dlog_level;
    property<std::string> m_dlog_type;
};

struct mc_reboot_interface : public mc::engine::interface<mc_reboot_interface> {
    MC_INTERFACE("bmc.kepler.MicroComponent.Reboot")

    ~mc_reboot_interface() override = default;

    int32_t prepare(std::map<std::string, std::string> context);
    int32_t action(std::map<std::string, std::string> context);
    void    cancel(std::map<std::string, std::string> context);
};

struct mc_reset_interface : public mc::engine::interface<mc_reset_interface> {
    MC_INTERFACE("bmc.kepler.MicroComponent.Reset")

    ~mc_reset_interface() override = default;

    int32_t prepare(std::map<std::string, std::string> context, std::string reset_type);
    int32_t action(std::map<std::string, std::string> context, std::string reset_type);
    void    cancel(std::map<std::string, std::string> context, std::string reset_type);
};

struct mc_maintenance_interface : public mc::engine::interface<mc_maintenance_interface> {
    MC_INTERFACE("bmc.kepler.Release.Maintenance")

    ~mc_maintenance_interface() override = default;

    void dlog_limit(std::map<std::string, std::string> context, bool enabled, uint8_t duration_mins);
};

class micro_component_object : public mc::engine::object<micro_component_object> {
public:
    MC_OBJECT(
        micro_component_object, "MicroComponent", "/bmc/kepler/${object_name}/MicroComponent",
        (micro_component_interface)(mc_config_manage_interface)(mc_debug_interface)(mc_reboot_interface)(mc_reset_interface)(mc_maintenance_interface))

    void init(std::string_view service_name);

    micro_component_interface  m_mc_iface;
    mc_config_manage_interface m_mc_config_manage_iface;
    mc_debug_interface         m_mc_debug_iface;
    mc_reboot_interface        m_mc_reboot_iface;
    mc_reset_interface         m_mc_reset_iface;
    mc_maintenance_interface   m_mc_maintenance_iface;
};

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
MC_REFLECT(mc::engine::mc_reboot_interface, ((prepare, "Prepare"))((action, "Action"))((cancel, "Cancel")))
MC_REFLECT(mc::engine::mc_reset_interface, ((prepare, "Prepare"))((action, "Action"))((cancel, "Cancel")))
MC_REFLECT(mc::engine::mc_maintenance_interface, ((dlog_limit, "DlogLimit")))
MC_REFLECT(
    mc::engine::micro_component_object,
    ((m_mc_iface, "bmc.kepler.MicroComponent"))((m_mc_config_manage_iface, "bmc.kepler.MicroComponent.ConfigManage"))(
        (m_mc_debug_iface, "bmc.kepler.MicroComponent.Debug"))((m_mc_reboot_iface, "bmc.kepler.MicroComponent.Reboot"))(
        (m_mc_reset_iface, "bmc.kepler.MicroComponent.Reset"))((m_mc_maintenance_iface,
                                                                "bmc.kepler.Release.Maintenance")))

#endif // MICRO_COMPONENT_H