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

#ifndef MC_DBUS_SERVICE_PROTOCOL_H
#define MC_DBUS_SERVICE_PROTOCOL_H

#include <mc/dbus/sd_bus.h>
#include <mc/engine/service_protocol.h>
#include <mc/string_view.h>

#include <map>
#include <memory>
#include <mutex>
#include <string>

namespace mc::dbus {

class MC_API dbus_service_protocol : public mc::engine::service_protocol {
public:
    explicit dbus_service_protocol(sd_bus& bus);

    mc::string_view name() const noexcept override;

    void attach(mc::engine::service& service) override;
    void detach() override;

    void register_object(mc::engine::abstract_object& obj) override;
    void unregister_object(mc::string_view path) override;

    mc::variant             request(const mc::engine::service_operation& operation) override;
    mc::result<mc::variant> async_request(const mc::engine::service_operation& operation) override;
    uint64_t                add_match(mc::dbus::match_rule& rule, std::function<void(mc::dbus::message&)>&& cb) override;
    void                    remove_match(uint64_t id) override;

private:
    struct exported_object_state;

    mc::variant perform_request(const mc::engine::service_operation& operation, mc::engine::abstract_object* obj);
    void        export_object(mc::engine::abstract_object& obj);
    void        unexport_object(mc::string_view path);
    void        publish_property_update(const mc::engine::update_property_operation& operation);
    DBusHandlerResult handle_exported_object_message(mc::string_view path, message& msg);

    sd_bus*                                                     m_bus{nullptr};
    mc::engine::service*                                        m_service{nullptr};
    std::map<std::string, std::shared_ptr<exported_object_state>> m_exported_objects;
    mutable std::mutex                                          m_mutex;
};

} // namespace mc::dbus

#endif // MC_DBUS_SERVICE_PROTOCOL_H
