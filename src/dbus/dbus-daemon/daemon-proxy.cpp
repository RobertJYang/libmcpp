/*
 * Copyright (c) 2024 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <mc/dbus/connection.h>
#include <mc/dbus/dbus-daemon/daemon-proxy.h>
#include <mc/dbus/message.h>

namespace mc::dbus {

daemon_proxy::daemon_proxy(connection_ptr conn) : m_conn(conn) {
}

daemon_proxy::~daemon_proxy() {
}

void daemon_proxy::hello() {
    variants_message msg;
    msg.destination = "org.freedesktop.DBus";
    msg.set_method_call("/org/freedesktop/DBus", "org.freedesktop.DBus", "Hello");
    m_conn->send(msg);
}

} // namespace mc::dbus
