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

#ifndef MC_DBUS_DISPATCH_PENDING_CALL_H
#define MC_DBUS_DISPATCH_PENDING_CALL_H

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <dbus/dbus.h>

#include <mc/dbus/message.h>
#include <mc/time.h>

namespace mc::dbus {

class pending_call {
public:
    using reply_cb = std::function<void(mc::dbus::message)>;

    pending_call(DBusPendingCall* pending_call, reply_cb cb);
    ~pending_call();

    pending_call(const pending_call&)            = delete;
    pending_call& operator=(const pending_call&) = delete;

    pending_call(pending_call&&);
    pending_call& operator=(pending_call&&);

    void start();
    void stop();
    void release();

    bool operator==(const pending_call& other) const {
        return m_pending_call == other.m_pending_call;
    }

    bool operator!=(const pending_call& other) const {
        return m_pending_call != other.m_pending_call;
    }

private:
    static void notify(DBusPendingCall* pending_call, void* data);

    void handle_reply();

    DBusPendingCall* m_pending_call;
    reply_cb         on_reply;
};

} // namespace mc::dbus

#endif // MC_DBUS_DISPATCH_PENDING_CALL_H
