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
#ifndef MC_INTROSPECTION_CACHE_H
#define MC_INTROSPECTION_CACHE_H

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include <mc/dbus/sd_bus.h>

#include "introspection_types.h"

struct NodeEntry {
    node_info node;
};

class introspection_cache {
public:
    static introspection_cache& instance();

    // 获取接口introspection信息

    const interface_info& get_interface(mc::dbus::sd_bus* bus, const std::string& service, const std::string& path,
                                        const std::string& interface);

    // 清理指定 service/path 的缓存
    void invalidate(const std::string& service, const std::string& path);

    introspection_cache(const introspection_cache&) = delete;

private:
    introspection_cache()  = default;
    ~introspection_cache() = default;

    std::string make_key(const std::string& service, const std::string& path) const;

    node_info fetch_from_dbus(mc::dbus::sd_bus* bus, const std::string& service, const std::string& path);

private:
    std::mutex                                 m_mutex;
    std::unordered_map<std::string, NodeEntry> m_cache;
};

#endif // MC_INTROSPECTION_CACHE_H