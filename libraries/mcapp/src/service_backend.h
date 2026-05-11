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

#ifndef MC_APP_SERVICE_BACKEND_H
#define MC_APP_SERVICE_BACKEND_H

#if defined(MCDBUS_USE_OLD_SHM) && MCDBUS_USE_OLD_SHM

#include <mc/engine/service_backend.h>
#include <mc/string.h>

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace mc::dbus {
class connection;
class message;
class shm_tree;
} // namespace mc::dbus

namespace mc::engine {
class abstract_object;
class service;
} // namespace mc::engine

namespace mc::app {

class service_backend : public mc::engine::service_backend {
public:
    service_backend(mc::engine::service& svc, mc::dbus::connection& conn);
    ~service_backend() override;

    service_backend(const service_backend&)            = delete;
    service_backend& operator=(const service_backend&) = delete;
    service_backend(service_backend&&)                 = delete;
    service_backend& operator=(service_backend&&)      = delete;

    bool install();
    void uninstall();

    bool installed() const noexcept
    {
        return m_installed;
    }

    bool emit(const mc::engine::message& msg) override;
    void on_object_added(mc::engine::abstract_object& obj) override;
    void on_object_removed(mc::engine::abstract_object& obj) override;
    void on_match_added(mc::engine::match_id id, const mc::engine::match_rule& rule) override;
    void on_match_removed(mc::engine::match_id id) override;

private:
    void register_to_shm(mc::engine::abstract_object& obj);
    void unregister_from_shm(mc::string_view path);
    void unregister_match_from_shm(mc::engine::match_id id);
    void dispatch_signal_to_match(mc::engine::match_id id, mc::dbus::message& wire_msg);

    mc::engine::service&                               m_service;
    mc::dbus::connection&                              m_connection;
    std::string                                        m_service_name;
    std::string                                        m_unique_name;
    std::unique_ptr<mc::dbus::shm_tree>                m_shm_tree;
    bool                                               m_installed{false};
    std::mutex                                         m_match_mutex;
    std::unordered_map<mc::engine::match_id, uint64_t> m_shm_match_ids;
};

} // namespace mc::app

#endif // MCDBUS_USE_OLD_SHM

#endif // MC_APP_SERVICE_BACKEND_H
