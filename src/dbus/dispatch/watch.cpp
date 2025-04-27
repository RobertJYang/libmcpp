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
#include <mc/dbus/dispatch/watch.h>
#include <mc/log.h>

namespace mc::dbus {
constexpr auto wait_read  = boost::asio::posix::stream_descriptor::wait_read;
constexpr auto wait_write = boost::asio::posix::stream_descriptor::wait_write;

watch::watch(strand_type& strand, DBusWatch* watch)
    : m_watch(watch), m_socket(strand, dbus_watch_get_unix_fd(watch)) {
}

watch::~watch() {
    stop();
}

void watch::start(connection* conn) {
    if (!dbus_watch_get_enabled(m_watch)) {
        return;
    }

    unsigned int flags = dbus_watch_get_flags(m_watch);
    if (flags & DBUS_WATCH_READABLE) {
        watch_readable(conn);
    }

    if (flags & DBUS_WATCH_WRITABLE) {
        watch_writable(conn);
    }
}

void watch::stop() {
    m_watch = nullptr;
    m_socket.close();
}

void watch::watch_readable(connection* conn) {
    connection_weak_ptr conn_weak = conn->weak_from_this();
    m_socket.async_wait(wait_read, [this, conn_weak](const auto& ec) {
        if (ec) {
            if (ec == boost::asio::error::operation_aborted) {
                return;
            }

            elog("dbus watch 读取错误: ${error}", ("error", ec.message()));
            return;
        }

        auto conn = conn_weak.lock();
        if (!conn) {
            return;
        }

        connection* p = conn.get();
        if (handle_watch_ready(p, DBUS_WATCH_READABLE)) {
            watch_readable(p);
        }
    });
}

void watch::watch_writable(connection* conn) {
    connection_weak_ptr conn_weak = conn->weak_from_this();
    m_socket.async_wait(wait_write, [this, conn_weak](const auto& ec) {
        if (ec) {
            if (ec == boost::asio::error::operation_aborted) {
                return;
            }

            elog("dbus watch 写入错误: ${error}", ("error", ec.message()));
            return;
        }

        auto conn = conn_weak.lock();
        if (!conn) {
            return;
        }

        connection* p = conn.get();
        if (handle_watch_ready(p, DBUS_WATCH_WRITABLE)) {
            watch_writable(p);
        }
    });
}

bool watch::handle_watch_ready(connection* conn, uint32_t flags) {
    dbus_watch_handle(m_watch, flags);
    conn->dispatch();
    if (!m_watch) {
        return false;
    }

    if (dbus_watch_get_enabled(m_watch) && (dbus_watch_get_flags(m_watch) & flags)) {
        return true;
    }
    return false;
}

} // namespace mc::dbus