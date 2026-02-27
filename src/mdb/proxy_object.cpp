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
#include "mc/mdb/proxy_object.h"

#include "mc/mdb/mdb_access.h"
#include "mc/mdb/mdb_validator.h"
#include <mc/dbus/sd_bus.h>
#include <mc/engine/context.h>
#include <mc/exception.h>
#include <mc/runtime.h>

static constexpr const char*      PROP_IFACE      = "bmc.kepler.Object.Properties";
static constexpr mc::milliseconds DEFAULT_TIMEOUT = mc::milliseconds(30000); // 30秒

// 接收裸指针的构造函数（不拥有 bus 所有权）
proxy_object::proxy_object(mc::dbus::sd_bus*     bus,
                           std::string           service,
                           std::string           path,
                           std::string           interface,
                           const interface_info& iface_info)
    : m_owned_bus(nullptr),
      m_bus(bus),
      m_service(std::move(service)),
      m_path(std::move(path)),
      m_interface(std::move(interface)),
      m_iface_info(iface_info) {
}

// 接收 shared_ptr 的构造函数
proxy_object::proxy_object(std::shared_ptr<mc::dbus::sd_bus> bus,
                           std::string                       service,
                           std::string                       path,
                           std::string                       interface,
                           const interface_info&             iface_info)
    : m_owned_bus(std::move(bus)),
      m_bus(m_owned_bus.get()),
      m_service(std::move(service)),
      m_path(std::move(path)),
      m_interface(std::move(interface)),
      m_iface_info(iface_info) {
}

const interface_info& proxy_object::get_interface_info() const {
    return m_iface_info;
}

bool proxy_object::has_property(const std::string& name) const {
    return m_iface_info.properties.count(name) != 0;
}

bool proxy_object::has_method(const std::string& name) const {
    return m_iface_info.methods.count(name) != 0;
}

const property_info* proxy_object::get_property_info(const std::string& name) const {
    auto it = m_iface_info.properties.find(name);
    return it == m_iface_info.properties.end() ? nullptr : &it->second;
}

const method_info* proxy_object::get_method_info(const std::string& name) const {
    auto it = m_iface_info.methods.find(name);
    return it == m_iface_info.methods.end() ? nullptr : &it->second;
}

mc::variant proxy_object::get_property(const std::string& name) {
    auto* prop = get_property_info(name);
    if (!prop) {
        MC_THROW(mc::exception, "invalid property");
    }

    mc::dict ctx{};
    auto*    engine_ctx = mc::engine::context::get_current_context_ptr();
    if (engine_ctx) {
        ctx = engine_ctx->get_args();
    }

    mc::variants args = {ctx, m_interface, name};

    try {
        mc::dbus::method_call_params params{m_service, m_path, PROP_IFACE, "GetWithContext", "a{ss}ss", args};
        mc::variants                 results = m_bus->timeout_call(DEFAULT_TIMEOUT, params);
        return mdb_utils::convert_method_result(results);
    } catch (const std::exception& e) {
        throw;
    }
}

void proxy_object::set_property(const std::string& name, const mc::variant& value) {
    auto* prop = get_property_info(name);
    if (!prop) {
        MC_THROW(mc::exception, "invalid property");
    }

    mdb_validator::check(name, prop->type, value);

    mc::dict ctx{};
    auto*    engine_ctx = mc::engine::context::get_current_context_ptr();
    if (engine_ctx) {
        ctx = engine_ctx->get_args();
    }

    mc::variants args = {ctx, m_interface, name, value};

    try {
        mc::dbus::method_call_params params{m_service, m_path, PROP_IFACE, "SetWithContext", "a{ss}ssv", args};
        m_bus->timeout_call(DEFAULT_TIMEOUT, params);
    } catch (const std::exception& e) {
        throw;
    }
}

mc::dict proxy_object::get_all_properties() {
    mc::dict ctx{};
    auto*    engine_ctx = mc::engine::context::get_current_context_ptr();
    if (engine_ctx) {
        ctx = engine_ctx->get_args();
    }

    mc::variants args = {ctx, m_interface};

    try {
        mc::dbus::method_call_params params{m_service, m_path, PROP_IFACE, "GetAllWithContext", "a{ss}s", args};
        mc::variants                 results = m_bus->timeout_call(DEFAULT_TIMEOUT, params);
        mc::variant                  result  = mdb_utils::convert_method_result(results);
        if (result.is_dict()) {
            return result.as_dict();
        } else {
            return mc::dict();
        }
    } catch (const std::exception& e) {
        throw;
    }
}

std::pair<mc::dict, mc::dict> proxy_object::get_properties(const mc::variants& prop_names) {
    mc::dict ctx{};
    auto*    engine_ctx = mc::engine::context::get_current_context_ptr();
    if (engine_ctx) {
        ctx = engine_ctx->get_args();
    }

    // 调用 GetPropertiesByNames 方法
    // 方法签名: a{ss}sas (context, interface, prop_names)
    mc::variants args = {ctx, mc::variant(m_interface), mc::variant(prop_names)};

    try {
        mc::dbus::method_call_params params{m_service, m_path, PROP_IFACE, "GetPropertiesByNames", "a{ss}sas", args};
        mc::variants                 results = m_bus->timeout_call(DEFAULT_TIMEOUT, params);

        // 处理返回值： (all_data, errs)
        mc::dict props_dict;
        mc::dict errs_dict;

        if (results.size() >= 2) {
            // 第一个返回值：属性数据 (a{sv})
            if (results[0].is_dict()) {
                props_dict = results[0].as_dict();
            }
            // 第二个返回值：错误信息 (a{ss})
            if (results[1].is_dict()) {
                errs_dict = results[1].as_dict();
            }
        } else if (results.size() == 1) {
            // 只有一个返回值，可能是属性数据
            if (results[0].is_dict()) {
                props_dict = results[0].as_dict();
            }
        }

        return std::make_pair(props_dict, errs_dict);
    } catch (const std::exception& e) {
        throw;
    }
}

mc::variants proxy_object::call_method(const std::string& name, const mc::variant& args) {
    auto* method = get_method_info(name);
    if (!method) {
        MC_THROW(mc::exception, "invalid method");
    }

    // 构建方法签名
    std::string sig;
    for (const auto& arg : method->args) {
        if (arg.direction == "in" || arg.direction.empty()) {
            sig += arg.type;
        }
    }

    // 构建参数列表
    mc::variants method_args;
    if (args.is_array()) {
        method_args = args.as_array();
    } else if (!args.is_null()) {
        method_args = {args};
    }

    mdb_validator::check_method_args(name, sig, mc::variant(method_args));

    mc::dbus::method_call_params params{m_service, m_path, m_interface, name, sig, method_args};
    mc::variants                 results = m_bus->timeout_call(DEFAULT_TIMEOUT, params);

    return results;
}

std::pair<bool, mc::variants> proxy_object::call_method_pcall(const std::string& name,
                                                                 const mc::variant& args,
                                                                 std::optional<std::string>& error) {
    try {
        error.reset();
        mc::variants results = call_method(name, args);
        return {true, std::move(results)};
    } catch (const std::exception& e) {
        error = e.what();
        return {false, {}};
    }
}

bool proxy_object::is_volatile(const std::string& name) const {
    auto* prop = get_property_info(name);
    if (!prop) {
        MC_THROW(mc::exception, "invalid property");
    }
    return prop->is_volatile();
}

mc::dbus::connection& proxy_object::bus() {
    return m_bus->get_connection();
}

const std::string& proxy_object::service() const {
    return m_service;
}

const std::string& proxy_object::path() const {
    return m_path;
}

const std::string& proxy_object::interface() const {
    return m_interface;
}
