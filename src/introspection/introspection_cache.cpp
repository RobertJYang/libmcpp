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

#include "mc/introspection/introspection_cache.h"
#include "mc/introspection/introspection_parser.h"

#include "mc/mdb/mdb_access.h"
#include <mc/dbus/message.h>
#include <mc/dbus/sd_bus.h>
#include <mc/exception.h>
#include <mc/runtime.h>

namespace {
constexpr const char*      INTROSPECTION_INTERFACE = "org.freedesktop.DBus.Introspectable";
constexpr const char*      INTROSPECT_METHOD       = "Introspect";
constexpr mc::milliseconds INTROSPECTION_TIMEOUT   = mc::milliseconds(3000); // 3秒
} // namespace

introspection_cache& introspection_cache::instance()
{
    static introspection_cache inst;
    return inst;
}

const interface_info& introspection_cache::get_interface(mc::dbus::sd_bus* bus, const std::string& service,
                                                         const std::string& path, const std::string& interface)
{
    const auto                  key = make_key(service, path);
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_cache.find(key);
    if (it == m_cache.end()) {
        node_info node = fetch_from_dbus(bus, service, path);
        it             = m_cache.emplace(key, NodeEntry{std::move(node)}).first;
    }

    auto iface_it = it->second.node.ifaces.find(interface);
    if (iface_it == it->second.node.ifaces.end()) {
        throw std::runtime_error("Interface not found in introspection");
    }

    return iface_it->second;
}

void introspection_cache::invalidate(const std::string& service, const std::string& path)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_cache.erase(make_key(service, path));
}

std::string introspection_cache::make_key(const std::string& service, const std::string& path) const
{
    return service + ":" + path;
}

node_info introspection_cache::fetch_from_dbus(mc::dbus::sd_bus* bus, const std::string& service,
                                               const std::string& path)
{
    // 使用 timeout_call 调用 Introspect 方法
    mc::variants                 args;
    mc::dbus::method_call_params params{service, path, INTROSPECTION_INTERFACE, INTROSPECT_METHOD, "", args};
    mc::variants                 results = bus->timeout_call(INTROSPECTION_TIMEOUT, params);

    // 转换返回结果：如果只有一个返回值，直接返回；多个返回值作为数组返回
    mc::variant rsp = mdb_utils::convert_method_result(results);

    // 解析返回的 XML 字符串
    std::string xml = rsp.as_string();

    return introspection_parser::parse(xml);
}