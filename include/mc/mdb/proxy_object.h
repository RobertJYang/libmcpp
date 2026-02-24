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

#ifndef MC_PROXY_OBJECT_H
#define MC_PROXY_OBJECT_H

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

#include <mc/dbus/sd_bus.h>
#include <mc/dict.h>
#include <mc/variant.h>

#include "../introspection/introspection_types.h"

class MC_API proxy_object {
public:
    // 接收指针的构造函数（用于 get_object）
    proxy_object(
        mc::dbus::sd_bus*     bus,
        std::string           service,
        std::string           path,
        std::string           interface,
        const interface_info& iface_info);

    // 接收 unique_ptr 的构造函数（用于 get_object）
    proxy_object(
        std::unique_ptr<mc::dbus::sd_bus> bus,
        std::string                       service,
        std::string                       path,
        std::string                       interface,
        const interface_info&             iface_info);

    const interface_info& get_interface_info() const;

    bool has_property(const std::string& name) const;
    bool has_method(const std::string& name) const;

    const property_info* get_property_info(const std::string& name) const;
    const method_info*   get_method_info(const std::string& name) const;

    /* property */
    mc::variant                   get_property(const std::string& name);
    void                          set_property(const std::string& name, const mc::variant& value);
    mc::dict                      get_all_properties();
    std::pair<mc::dict, mc::dict> get_properties(const mc::variants& prop_names);

    /* method */
    mc::variants call_method(const std::string& name, const mc::variant& args);
    // 返回 (success, results)，success 为 true 表示成功，false 表示失败
    // 失败时 results 为空，成功时 results 包含返回值
    std::pair<bool, mc::variants> call_method_pcall(const std::string& name, const mc::variant& args,
                                                     std::optional<std::string>& error);

    bool is_volatile(const std::string& name) const;

    /* meta */
    mc::dbus::connection& bus();
    const std::string&    service() const;
    const std::string&    path() const;
    const std::string&    interface() const;

private:
    std::shared_ptr<mc::dbus::sd_bus> m_owned_bus; // 可选：拥有所有权时使用
    mc::dbus::sd_bus*                 m_bus;       // 实际使用的指针
    std::string                       m_service;
    std::string                       m_path;
    std::string                       m_interface;
    interface_info                    m_iface_info;
};

#endif // MC_PROXY_OBJECT_H
