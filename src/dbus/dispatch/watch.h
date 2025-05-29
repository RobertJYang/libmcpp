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

#include "dbus/connection_impl.h"

namespace mc::dbus {

class watch : public mc::ref_base<watch> {
public:
    template <typename Executor>
    watch(Executor& executor, DBusWatch* watch)
        : m_watch(watch), m_socket(executor, dbus_watch_get_unix_fd(watch)) {
    }

    ~watch();

    void start(connection_weak_ptr conn);
    void stop();

private:
    void watch_readable(connection_weak_ptr conn, watch::ref_ptr self);
    void watch_writable(connection_weak_ptr conn, watch::ref_ptr self);
    bool handle_watch_ready(connection_ptr& conn, uint32_t flags);

    using socket_type = boost::asio::posix::stream_descriptor;
    DBusWatch*  m_watch;
    socket_type m_socket;
};

} // namespace mc::dbus
#endif // MC_DBUS_DISPATCH_WATCH_H