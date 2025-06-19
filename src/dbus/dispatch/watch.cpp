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
#include <mc/log.h>

#include "dbus/connection_impl.h"
#include "watch.h"

namespace mc::dbus {
constexpr auto wait_read  = boost::asio::posix::stream_descriptor::wait_read;
constexpr auto wait_write = boost::asio::posix::stream_descriptor::wait_write;

watch::~watch() {
    stop();
}

void watch::start(connection_weak_ptr conn) {
    if (!dbus_watch_get_enabled(m_watch)) {
        return;
    }

    unsigned int flags = dbus_watch_get_flags(m_watch);
    if (flags & DBUS_WATCH_READABLE) {
        watch_readable(conn, this->shared_from_this());
    }

    if (flags & DBUS_WATCH_WRITABLE) {
        watch_writable(conn, this->shared_from_this());
    }
}

void watch::stop() {
    m_watch = nullptr;
    m_socket.close();
}

void watch::watch_readable(connection_weak_ptr conn, mc::shared_ptr<watch> self) {
    m_socket.async_wait(wait_read, [s = std::move(self), c = std::move(conn)](const auto& ec) {
        if (ec) {
            if (ec == boost::asio::error::operation_aborted) {
                return;
            }

            elog("dbus watch 读取错误: ${error}", ("error", ec.message()));
            return;
        }

        auto conn = c.lock();
        if (!conn) {
            return;
        }

        std::lock_guard lock(conn->m_mutex);
        if (!conn->is_connected()) {
            return;
        }

        watch* w = s.get();
        if (w->handle_watch_ready(conn, DBUS_WATCH_READABLE)) {
            w->watch_readable(std::move(c), std::move(s));
        }
    });
}

void watch::watch_writable(connection_weak_ptr conn, mc::shared_ptr<watch> self) {
    m_socket.async_wait(wait_write, [s = std::move(self), c = std::move(conn)](const auto& ec) {
        if (ec) {
            if (ec == boost::asio::error::operation_aborted) {
                return;
            }

            elog("dbus watch 写入错误: ${error}", ("error", ec.message()));
            return;
        }

        auto conn = c.lock();
        if (!conn) {
            return;
        }

        std::lock_guard lock(conn->m_mutex);
        if (!conn->is_connected()) {
            return;
        }

        watch* w = s.get();
        if (w->handle_watch_ready(conn, DBUS_WATCH_WRITABLE) && conn->is_connected()) {
            w->watch_writable(std::move(c), std::move(s));
        }
    });
}

bool watch::handle_watch_ready(connection_ptr& conn, uint32_t flags) {
    dbus_watch_handle(m_watch, flags);

    boost::asio::post(conn->m_executor, [conn]() {
        std::lock_guard lock(conn->m_mutex);
        conn->dispatch();
    });

    if (m_watch && dbus_watch_get_enabled(m_watch) && (dbus_watch_get_flags(m_watch) & flags)) {
        return true;
    }
    return false;
}

} // namespace mc::dbus