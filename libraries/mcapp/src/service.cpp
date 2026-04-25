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

#include <mc/app/service.h>

#include <mc/app/app_proto.h>
#include <mc/dbus/validator.h>
#include <mc/exception.h>
#include <mc/log/log.h>

namespace mc::app {

service::service(mc::string name) : mc::engine::service(name)
{
    if (!mc::dbus::validator::is_valid_bus_name(name)) {
        MC_THROW(mc::exception, "invalid app service name '" + name +
                                    "': must be a valid DBus bus name "
                                    "(dot-separated segments of [A-Za-z_][A-Za-z0-9_]*, at least 2 segments)");
    }
}

service::~service() = default;

mc::dbus::connection& service::connection() noexcept
{
    return m_connection;
}

app_proto* service::proto() noexcept
{
    return m_proto.get();
}

const mc::string& service::path() const noexcept
{
    return m_path;
}

service_state service::state() const noexcept
{
    return m_state;
}

const mc::dict& service::properties() const noexcept
{
    return m_properties;
}

bool service::configure(const service_context& context, mc::dict properties)
{
    m_context     = context;
    m_has_context = true;
    return init(std::move(properties));
}

bool service::start()
{
    if (m_state == service_state::running) {
        return true;
    }

    m_state = service_state::starting;

    if (!bring_up_dbus_transport()) {
        tear_down_dbus_transport();
        m_state = service_state::failed;
        return false;
    }

    if (!mc::engine::service::start()) {
        tear_down_dbus_transport();
        m_state = service_state::failed;
        return false;
    }

    m_state = service_state::running;
    return true;
}

bool service::stop()
{
    m_state = service_state::stopping;

    bool engine_ok = mc::engine::service::stop();

    tear_down_dbus_transport();

    if (!engine_ok) {
        m_state = service_state::failed;
        return false;
    }

    cleanup();
    m_state = service_state::stopped;
    return true;
}

bool service::bring_up_dbus_transport()
{
    if (!m_has_context) {
        return true;
    }

    try {
        m_connection = mc::dbus::connection::open_session_bus(m_context.runtime().io());
    } catch (const std::exception& ex) {
        wlog("mcapp service ${name} session DBus 不可用: ${error}，降级为无 DBus 通路模式",
             ("name", mc::string(name()))("error", mc::string(ex.what())));
        m_connection = mc::dbus::connection{};
        return true;
    }

    if (!m_connection.start() || !m_connection.is_connected()) {
        wlog("mcapp service ${name} 无法启动 DBus connection，降级为无 DBus 通路模式", ("name", mc::string(name())));
        m_connection = mc::dbus::connection{};
        return true;
    }

    auto [success, _] = m_connection.request_name(name());
    if (!success) {
        wlog("mcapp service ${name} request_name 失败，降级为无 DBus 通路模式", ("name", mc::string(name())));
        m_connection.disconnect();
        m_connection = mc::dbus::connection{};
        return true;
    }

    m_proto = std::make_unique<app_proto>(mc::string(name()), m_connection, nullptr);
    set_proto(m_proto.get());
    return true;
}

void service::tear_down_dbus_transport()
{
    if (m_proto) {
        set_proto(nullptr);
        m_proto.reset();
    }
    if (m_connection.is_connected()) {
        m_connection.disconnect();
    }
    m_connection = mc::dbus::connection{};
}

void service::set_path(mc::string path)
{
    m_path = std::move(path);
}

bool service::on_configure()
{
    return true;
}

bool service::on_init(mc::dict properties)
{
    m_properties = std::move(properties);
    m_state      = service_state::configured;
    if (on_configure()) {
        return true;
    }

    m_state = service_state::failed;
    return false;
}

const service_context& service::context() const
{
    MC_ASSERT(m_has_context, "service context is not initialized");
    return m_context;
}

void service::set_state(service_state state) noexcept
{
    m_state = state;
}

} // namespace mc::app
