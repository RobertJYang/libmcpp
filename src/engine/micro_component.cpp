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

void mc_debug_interface::attach_debug_console(std::map<std::string, std::string> context, uint32_t port) {
}

void mc_debug_interface::detach_debug_console(std::map<std::string, std::string> context) {
}

void mc_debug_interface::dump(std::map<std::string, std::string> context, std::string filepath) {
}

void mc_debug_interface::set_dlog_level(std::map<std::string, std::string> context, std::string level,
                                        uint8_t effective_hours) {
}

int32_t mc_reboot_interface::prepare(std::map<std::string, std::string> context) {
    return 0;
}

int32_t mc_reboot_interface::action(std::map<std::string, std::string> context) {
    return 0;
}

void mc_reboot_interface::cancel(std::map<std::string, std::string> context) {
}

int32_t mc_reset_interface::prepare(std::map<std::string, std::string> context, std::string reset_type) {
    return 0;
}

int32_t mc_reset_interface::action(std::map<std::string, std::string> context, std::string reset_type) {
    return 0;
}

void mc_reset_interface::cancel(std::map<std::string, std::string> context, std::string reset_type) {
}

void mc_maintenance_interface::dlog_limit(std::map<std::string, std::string> context, bool enabled,
                                          uint8_t duration_mins) {
}

void micro_component_object::init(std::string_view service_name) {
    size_t pos      = service_name.find_last_of('.');
    auto   app_name = service_name.substr(pos + 1);
    set_object_name(app_name);
    m_mc_iface.m_name             = std::string(app_name);
    m_mc_iface.m_pid              = getpid();
    m_mc_iface.m_status           = "InitCompleted";
    m_mc_debug_iface.m_dlog_level = "notice";
    m_mc_debug_iface.m_dlog_type  = "file";
}

} // namespace mc::engine