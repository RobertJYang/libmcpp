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

#include "mc/mdb/mdb_access.h"

#include <mutex>

#include <mc/dbus/connection.h>
#include <mc/dbus/message.h>
#include <mc/dbus/sd_bus.h>
#include <mc/exception.h>
#include <mc/mdb_service.h>
#include <mc/variant.h>

#include "mc/introspection/introspection_parser.h"
#include "mc/mdb/proxy_object.h"

using mc::dbus::connection;
using mc::dbus::match;
using mc::dbus::message;

namespace {
constexpr const char* INTROSPECTION_INTERFACE =
    "org.freedesktop.DBus.introspectable";
constexpr const char* INTROSPECT_METHOD = "Introspect";
constexpr const char* OBJMGR_INTERFACE =
    "org.freedesktop.DBus.ObjectManager";
constexpr const char* SIGNAL_INTERFACES_REMOVED =
    "InterfaceRemoved";

constexpr mc::milliseconds MDB_SERVICE_TIMEOUT   = mc::milliseconds(10 * 60 * 1000);
constexpr std::string_view MDB_SERVICE_NAME      = "bmc.kepler.maca";
constexpr std::string_view MDB_SERVICE_PATH      = "/bmc/kepler/MdbService";
constexpr std::string_view MDB_SERVICE_INTERFACE = "bmc.kepler.Mdb";
} // namespace

mdb_access& mdb_access::instance(size_t max_cache_size) {
    static mdb_access* inst = nullptr;
    static std::once_flag  once_flag;

    std::call_once(once_flag, [max_cache_size]() {
        inst = new mdb_access(max_cache_size);
    });

    return *inst;
}

mdb_access::mdb_access(size_t max_cache_size)
    : m_cache(max_cache_size) {
}

std::shared_ptr<proxy_object> mdb_access::get_object_by_short_call(mc::dbus::sd_bus* bus,
                                                                   const std::string& path,
                                                                   const std::string& interface) {
    mc::variants interfaces = {mc::variant(interface)};
    mc::variant  rsp        = mc::mdb::service::get_object(bus, path, interfaces);
    auto         it         = rsp.get_object().begin();

    if (it == rsp.get_object().end()) {
        MC_THROW(mc::system_exception, "service not exists: path=${p}, interface=${i}",
                 ("p", path)("i", interface));
    }

    std::string           service = it->key.as_string();
    const interface_info& iface_info =
        introspection_cache::instance().get_interface(bus, service, path, interface);

    // 使用 shared_ptr 管理内存，避免内存泄漏
    // 返回 shared_ptr 以确保对象生命周期由调用者管理
    return std::make_shared<proxy_object>(bus, service, path, interface, iface_info);
}

std::shared_ptr<proxy_object> mdb_access::get_object(std::unique_ptr<mc::dbus::sd_bus> bus,
                                                       const std::string&                path,
                                                       const std::string&                interface) {
    std::string cache_key = path + interface;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (auto cached = m_cache.get(cache_key)) {
            // 如果是阻塞模式，跳过 is_connected() 判断
            // 如果是非阻塞模式，需要判断 is_connected()
            if (bus->is_blocking() || cached->get()->bus().is_connected()) {
                return cached->get(); // 返回 shared_ptr，cache 持有引用计数
            }
        }
    }

    mc::variants interfaces = {mc::variant(interface)};
    mc::variant  rsp        = mc::mdb::service::get_object(bus.get(), path, interfaces);

    auto it = rsp.get_object().begin();
    if (it == rsp.get_object().end()) {
        MC_THROW(mc::system_exception, "service not exists: path=${p}, interface=${i}",
                 ("p", path)("i", interface));
    }

    std::string           service = it->key.as_string();
    const interface_info& iface_info =
        introspection_cache::instance().get_interface(bus.get(), service, path, interface);

    // 在 move bus 之前保存是否为阻塞模式
    bool is_blocking = bus->is_blocking();

    // 使用 shared_ptr 管理内存，并将 unique_ptr 移入
    auto obj = std::make_shared<proxy_object>(std::move(bus), service, path, interface, iface_info);

    // 缓存对象（需要加锁）
    // 如果是阻塞模式，不缓存对象
    if (!is_blocking) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_cache.put(cache_key, obj);
    }
    // 返回 shared_ptr，调用者和 cache 都持有引用计数（如果是非阻塞模式）
    return obj;
}

std::shared_ptr<proxy_object> mdb_access::get_object_with_service(
    std::unique_ptr<mc::dbus::sd_bus> bus,
    const std::string&                service,
    const std::string&                path,
    const std::string&                interface) {
    // 对 service 名称进行判空检查
    if (service.empty()) {
        MC_THROW(mc::invalid_arg_exception, "service name cannot be empty");
    }

    std::string cache_key = path + interface;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (auto cached = m_cache.get(cache_key)) {
            // 如果是阻塞模式，跳过 is_connected() 判断
            // 如果是非阻塞模式，需要判断 is_connected()
            if (bus->is_blocking() || cached->get()->bus().is_connected()) {
                return cached->get(); // 返回 shared_ptr，cache 持有引用计数
            }
        }
    }

    // 直接使用提供的 service 名称，不需要查找
    const interface_info& iface_info =
        introspection_cache::instance().get_interface(bus.get(), service, path, interface);

    // 在 move bus 之前保存是否为阻塞模式
    bool is_blocking = bus->is_blocking();

    // 使用 shared_ptr 管理内存，并将 unique_ptr 移入
    auto obj = std::make_shared<proxy_object>(std::move(bus), service, path, interface, iface_info);

    // 缓存对象（需要加锁）
    // 如果是阻塞模式，不缓存对象
    if (!is_blocking) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_cache.put(cache_key, obj);
    }
    // 返回 shared_ptr，调用者和 cache 都持有引用计数（如果是非阻塞模式）
    return obj;
}

std::map<std::string, std::shared_ptr<proxy_object>> mdb_access::get_sub_objects(
    std::unique_ptr<mc::dbus::sd_bus> bus,
    const std::string&                path,
    const std::string&                interface,
    int32_t                           depth) {
    // 在 move bus 之前保存 connection 和 is_blocking 信息
    mc::dbus::connection conn = bus->get_connection();
    bool                  is_blocking = bus->is_blocking();

    // 调用 mdb_service.get_sub_paths 获取子路径列表
    mc::variants interfaces = {mc::variant(interface)};
    mc::variant  rsp        = mc::mdb::service::get_sub_paths(bus.get(), path, depth, interfaces);

    // 解析返回结果：可能是数组，也可能是包含 SubPaths 字段的对象
    const mc::variants* path_list = nullptr;
    if (rsp.is_array()) {
        // 直接返回数组
        path_list = &rsp.get_array();
    } else if (rsp.is_object()) {
        // 返回的是包含 SubPaths 字段的对象
        const mc::dict& result_dict = rsp.get_object();
        auto            sub_paths_it = result_dict.find("SubPaths");
        if (sub_paths_it == result_dict.end()) {
            MC_THROW(mc::system_exception, "SubPaths field not found in result");
        }

        const mc::variant& sub_paths_variant = sub_paths_it->value;
        if (!sub_paths_variant.is_array()) {
            MC_THROW(mc::system_exception, "SubPaths is not an array");
        }

        path_list = &sub_paths_variant.get_array();
    } else {
        MC_THROW(mc::system_exception, "get_sub_paths returned invalid result: expected array or object");
    }

    // 创建结果 map，以路径为键，对象为值
    std::map<std::string, std::shared_ptr<proxy_object>> result_map;
    for (const auto& path_variant : *path_list) {
        if (!path_variant.is_string()) {
            continue; // 跳过非字符串的路径
        }

        std::string sub_path = path_variant.as_string();
        try {
            // 为每个路径创建新的 sd_bus 对象（因为 get_object 需要 unique_ptr）
            // connection 内部使用 shared_ptr，拷贝会增加引用计数
            auto sub_bus = std::make_unique<mc::dbus::sd_bus>(conn, is_blocking);
            auto obj     = get_object(std::move(sub_bus), sub_path, interface);
            if (obj) {
                result_map.emplace(sub_path, obj);
            }
        } catch (const std::exception& e) {
            // 获取对象失败，跳过该路径
            continue;
        }
    }

    return result_map;
}

void mdb_access::clear_cache() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_cache.clear();
}

void mdb_access::set_max_cache_size(size_t max_size) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_cache.set_max_size(max_size);
}

size_t mdb_access::cache_size() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_cache.size();
}
