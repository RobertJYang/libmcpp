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

#include <mc/dbus/dispatch/pending_call.h>

namespace mc::dbus {

pending_call::pending_call(boost::asio::io_context& io_context, DBusPendingCall* pending_call)
    : m_pending_call(pending_call) {
}

pending_call::~pending_call() {
    release();
}

pending_call::pending_call(const pending_call& other) : m_pending_call(other.m_pending_call) {
    if (m_pending_call) {
        dbus_pending_call_ref(m_pending_call);
    }
}

pending_call& pending_call::operator=(const pending_call& other) {
    if (this != &other) {
        release();

        m_pending_call = other.m_pending_call;
        if (m_pending_call) {
            dbus_pending_call_ref(m_pending_call);
        }
    }
    return *this;
}

pending_call::pending_call(pending_call&& other) : m_pending_call(other.m_pending_call) {
    other.m_pending_call = nullptr;
}

pending_call& pending_call::operator=(pending_call&& other) {
    if (this != &other) {
        m_pending_call       = other.m_pending_call;
        other.m_pending_call = nullptr;
    }
    return *this;
}

void pending_call::release() {
    if (m_pending_call) {
        dbus_pending_call_unref(m_pending_call);
        m_pending_call = nullptr;
    }
}

void pending_call::start() {
    dbus_pending_call_set_notify(m_pending_call, notify, this, nullptr);
}

void pending_call::stop() {
    dbus_pending_call_set_notify(m_pending_call, nullptr, nullptr, nullptr);
}

void pending_call::notify(DBusPendingCall*, void* data) {
    static_cast<pending_call*>(data)->handle_reply();
}

void pending_call::handle_reply() {
    DBusMessage* msg = dbus_pending_call_steal_reply(m_pending_call);
    stop();

    if (msg) {
        on_reply(message(msg));
    }
}

void pending_call::handle_timeout() {
    stop();
    on_reply(message::new_error_message(error_names::timeout));
}

} // namespace mc::dbus
