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

#ifndef MC_APP_MICRO_COMPONENT_H
#define MC_APP_MICRO_COMPONENT_H

#include <mc/dict.h>
#include <mc/engine.h>
#include <mc/log/appender.h>
#include <mc/string.h>
#include <mc/string_view.h>
#include <mc/timer.h>
#include <mc/variant.h>

#include <cstdint>
#include <mutex>
#include <tuple>
#include <vector>

namespace mc::app {

struct micro_component_interface : public mc::engine::interface<micro_component_interface> {
    MC_INTERFACE("bmc.kepler.MicroComponent")

    ~micro_component_interface() override = default;

    int32_t health_check(mc::dict context) const;

    mc::engine::property<int32_t>    m_pid;
    mc::engine::property<mc::string> m_author;
    mc::engine::property<mc::string> m_description;
    mc::engine::property<mc::string> m_license;
    mc::engine::property<mc::string> m_name;
    mc::engine::property<mc::string> m_status;
    mc::engine::property<mc::string> m_version;
};

struct mc_config_manage_interface : public mc::engine::interface<mc_config_manage_interface> {
    MC_INTERFACE("bmc.kepler.MicroComponent.ConfigManage")

    ~mc_config_manage_interface() override = default;

    std::vector<std::tuple<mc::string, mc::string>> backup(mc::dict context, mc::string filepath);

    mc::string export_config(mc::dict context, mc::string type);
    void       import_config(mc::dict context, mc::string data, mc::string type);
    void       recover(mc::dict context, mc::dict preserve_list);
    mc::string verify(mc::dict context, mc::string data);
    mc::string get_preserved_config(mc::dict context, mc::dict preserve_flag);
    mc::string get_trusted_config(mc::dict context);
};

struct mc_debug_interface : public mc::engine::interface<mc_debug_interface> {
    MC_INTERFACE("bmc.kepler.MicroComponent.Debug")

    ~mc_debug_interface() override;

    void attach_debug_console(mc::dict context, uint32_t port);
    void detach_debug_console(mc::dict context);
    void dump(mc::dict context, mc::string filepath);
    void set_dlog_level(mc::dict context, mc::string level, uint8_t effective_hours);
    void set_dlog_type(mc::string type);

    mc::engine::property<mc::string> m_dlog_level;
    mc::engine::property<mc::string> m_dlog_type;

    mc::timer_ptr m_dlog_level_timer;
    /// 最近一次 attach 的 socket 路径，供 set_dlog_type 在无具体类型时重配工厂
    mc::string                       m_debug_socket_path;
    mc::string                       m_debug_hb_socket_path;
    std::weak_ptr<mc::log::appender> m_debug_console_appender;

    mutable std::mutex m_dlog_level_mutex;
    mutable std::mutex m_debug_console_mutex;
};

struct mc_reboot_interface : public mc::engine::interface<mc_reboot_interface> {
    MC_INTERFACE("bmc.kepler.MicroComponent.Reboot")

    ~mc_reboot_interface() override = default;

    int32_t prepare(mc::dict context);
    int32_t process(mc::dict context);
    int32_t action(mc::dict context);
    void    cancel(mc::dict context);
};

struct mc_reset_interface : public mc::engine::interface<mc_reset_interface> {
    MC_INTERFACE("bmc.kepler.MicroComponent.Reset")

    ~mc_reset_interface() override = default;

    int32_t prepare(mc::dict context, mc::string reset_type);
    int32_t action(mc::dict context, mc::string reset_type);
    void    cancel(mc::dict context, mc::string reset_type);
};

struct mc_maintenance_interface : public mc::engine::interface<mc_maintenance_interface> {
    MC_INTERFACE("bmc.kepler.Release.Maintenance")

    ~mc_maintenance_interface() override;

    void dlog_limit(mc::dict context, bool enabled, uint8_t duration_mins);

    mc::timer_ptr      m_dlog_limit_timer;
    mutable std::mutex m_dlog_limit_mutex;
};

class micro_component_object : public mc::engine::object<micro_component_object> {
public:
    MC_OBJECT(
        micro_component_object, "MicroComponent", "/bmc/kepler/${object_name}/MicroComponent",
        (micro_component_interface)(mc_config_manage_interface)(mc_debug_interface)(mc_reboot_interface)(mc_reset_interface)(mc_maintenance_interface))

    void init(mc::string_view service_name);

    micro_component_interface  m_mc_iface;
    mc_config_manage_interface m_mc_config_manage_iface;
    mc_debug_interface         m_mc_debug_iface;
    mc_reboot_interface        m_mc_reboot_iface;
    mc_reset_interface         m_mc_reset_iface;
    mc_maintenance_interface   m_mc_maintenance_iface;
};

} // namespace mc::app

#endif // MC_APP_MICRO_COMPONENT_H
