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
#include "timeout.h"

namespace mc::dbus {

timeout::~timeout() {
    stop();
}

void timeout::start(connection_weak_ptr conn) {
    if (!dbus_timeout_get_enabled(m_timeout)) {
        return;
    }

    auto self = this->from_this();
    m_timer.async_wait([s = std::move(self), c = std::move(conn)](const auto& ec) {
        if (ec) {
            if (ec == boost::asio::error::operation_aborted) {
                return;
            }

            elog("dbus 定时器错误: ${error}", ("error", ec.message()));
            return;
        }

        auto conn = c.lock();
        if (!conn) {
            return;
        }

        timeout*        t = s.get();
        std::lock_guard lock(conn->m_mutex);
        if (t->m_timeout && conn->is_connected()) {
            dbus_timeout_handle(t->m_timeout);
        }
    });

    int interval = dbus_timeout_get_interval(m_timeout);
    m_timer.expires_after(std::chrono::milliseconds(interval));
}

void timeout::stop() {
    m_timeout = nullptr;
    m_timer.cancel();
}

} // namespace mc::dbus
