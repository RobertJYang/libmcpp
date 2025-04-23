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

#ifndef MC_DBUS_DISPATCH_TIMEOUT_H
#define MC_DBUS_DISPATCH_TIMEOUT_H
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <dbus/dbus.h>

namespace mc::dbus {
class connection;

class timeout {
public:
    timeout(boost::asio::io_context& io_context, DBusTimeout* timeout);
    ~timeout();

    void start();
    void stop();

private:
    DBusTimeout*              m_timeout;
    boost::asio::steady_timer m_timer;
};

} // namespace mc::dbus

#endif // MC_DBUS_DISPATCH_TIMEOUT_H
