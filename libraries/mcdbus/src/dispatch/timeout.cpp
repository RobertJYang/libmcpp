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
#include <mc/log/log.h>

#include <system_error>

#include "connection_impl.h"
#include "timeout.h"

namespace mc::dbus {

timeout::~timeout()
{
    stop();
}

void timeout::start(connection_weak_ptr conn)
{
    if (!dbus_timeout_get_enabled(m_timeout)) {
        return;
    }

    int interval = dbus_timeout_get_interval(m_timeout);
    // 1. 先设置定时器的过期时间：将定时器的过期时间设置为当前时间加上指定的时间间隔
    //    注意：必须先调用 expires_after 设置过期时间，然后才能调用 async_wait
    m_timer.expires_after(std::chrono::milliseconds(interval));

    auto self = this->shared_from_this();
    // 2. 然后开始异步等待定时器到期：当定时器到期时，回调函数会被调用
    //    回调函数参数 ec 表示错误码，如果定时器被取消则为 operation_aborted
    m_timer.async_wait([s = std::move(self), c = std::move(conn)](const auto& ec) {
        if (ec) {
            if (ec == std::make_error_code(std::errc::operation_canceled)) {
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
}

void timeout::stop()
{
    m_timeout = nullptr;
    m_timer.cancel();
}

} // namespace mc::dbus
