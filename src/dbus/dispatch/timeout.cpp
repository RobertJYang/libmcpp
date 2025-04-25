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

#include <mc/dbus/dispatch/timeout.h>
#include <mc/log.h>

namespace mc::dbus {

timeout::timeout(strand_type& strand, DBusTimeout* timeout) : m_timeout(timeout), m_timer(strand) {
}

timeout::~timeout() {
    stop();
}

void timeout::start() {
    if (!dbus_timeout_get_enabled(m_timeout)) {
        return;
    }

    int interval = dbus_timeout_get_interval(m_timeout);
    m_timer.expires_after(std::chrono::milliseconds(interval));
    m_timer.async_wait([this](const boost::system::error_code& ec) {
        if (ec) {
            if (ec == boost::asio::error::operation_aborted) {
                return;
            }

            elog("dbus 定时器错误: ${error}", ("error", ec.message()));
            return;
        }

        dbus_timeout_handle(m_timeout);
    });
}

void timeout::stop() {
    m_timer.cancel();
}

} // namespace mc::dbus
