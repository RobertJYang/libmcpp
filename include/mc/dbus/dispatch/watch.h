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

#ifndef MC_DBUS_DISPATCH_WATCH_H
#define MC_DBUS_DISPATCH_WATCH_H
#include <boost/asio/io_context.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>
#include <dbus/dbus.h>
#include <mc/signal_slot.h>

namespace mc::dbus {

class watch {
public:
    watch(boost::asio::io_context& io_context, DBusWatch* watch);
    ~watch();

    void start();
    void stop();

    mc::signal<void(uint32_t)> on_ready;

private:
    void watch_readable();
    void watch_writable();
    bool handle_watch_ready(uint32_t flags);

    using socket_type = boost::asio::posix::stream_descriptor;
    DBusWatch*  m_watch;
    socket_type m_socket;
};

} // namespace mc::dbus
#endif // MC_DBUS_DISPATCH_WATCH_H