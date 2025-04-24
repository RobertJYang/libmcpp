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

watch::watch(boost::asio::io_context& io_context, DBusWatch* watch)
    : m_watch(watch), m_socket(io_context, dbus_watch_get_unix_fd(watch)) {
}

watch::~watch() {
    stop();
}

void watch::start() {
    if (!dbus_watch_get_enabled(m_watch)) {
        return;
    }

    unsigned int flags = dbus_watch_get_flags(m_watch);
    if (flags & DBUS_WATCH_READABLE) {
        watch_readable();
    }

    if (flags & DBUS_WATCH_WRITABLE) {
        watch_writable();
    }
}

void watch::stop() {
    m_watch = nullptr;
    on_ready.disconnect_all();
    m_socket.close();
}

void watch::watch_readable() {
    mc::im::ref_ptr<watch> self(this);
    m_socket.async_wait(wait_read, [self = std::move(self)](const auto& ec) {
        if (ec) {
            if (ec == boost::asio::error::operation_aborted) {
                return;
            }

            elog("dbus watch 读取错误: ${error}", ("error", ec.message()));
            return;
        }

        if (self->handle_watch_ready(DBUS_WATCH_READABLE)) {
            self->watch_readable();
        }
    });
}

void watch::watch_writable() {
    mc::im::ref_ptr<watch> self(this);
    m_socket.async_wait(wait_write, [self = std::move(self)](const auto& ec) {
        if (ec) {
            if (ec == boost::asio::error::operation_aborted) {
                return;
            }

            elog("dbus watch 写入错误: ${error}", ("error", ec.message()));
            return;
        }

        if (self->handle_watch_ready(DBUS_WATCH_WRITABLE)) {
            self->watch_writable();
        }
    });
}

bool watch::handle_watch_ready(uint32_t flags) {
    if (m_watch.load() == nullptr) {
        return false;
    }

    dbus_watch_handle(m_watch, flags);
    on_ready(flags);
    if (dbus_watch_get_enabled(m_watch) && (dbus_watch_get_flags(m_watch) & flags)) {
        return true;
    }
    return false;
}

} // namespace mc::dbus