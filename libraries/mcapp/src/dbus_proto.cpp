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

#include <mc/app/dbus_proto.h>

namespace mc::app {

dbus_proto::dbus_proto(mc::string service_name, mc::dbus::connection conn)
    : m_service_name(std::move(service_name)), m_connection(std::move(conn))
{}

dbus_proto::~dbus_proto() = default;

mc::string_view dbus_proto::service_name() const noexcept
{
    return m_service_name;
}

mc::dbus::connection& dbus_proto::connection() noexcept
{
    return m_connection;
}

std::size_t dbus_proto::outbound_count() const noexcept
{
    return m_outbound_count;
}

mc::proto::execution_state dbus_proto::on_push(mc::proto::proto_request& req)
{
    ++m_outbound_count;
    return complete(req);
}

mc::proto::execution_state dbus_proto::on_pop(mc::proto::proto_request& req)
{
    return complete(req);
}

} // namespace mc::app
