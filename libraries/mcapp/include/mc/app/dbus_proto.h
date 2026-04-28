/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef MC_APP_DBUS_PROTO_H
#define MC_APP_DBUS_PROTO_H

#include <mc/dbus/connection.h>
#include <mc/dbus/message.h>
#include <mc/future.h>
#include <mc/protocol.h>
#include <mc/signal/connection.h>
#include <mc/string.h>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>

namespace mc::engine {
struct message;
} // namespace mc::engine

namespace mc::app {

class MC_API dbus_proto : public mc::proto::protocol {
public:
    dbus_proto(mc::string service_name, mc::dbus::connection conn);
    ~dbus_proto() override;

    dbus_proto(const dbus_proto&)            = delete;
    dbus_proto& operator=(const dbus_proto&) = delete;
    dbus_proto(dbus_proto&&)                 = delete;
    dbus_proto& operator=(dbus_proto&&)      = delete;

    mc::string_view       service_name() const noexcept;
    mc::dbus::connection& connection() noexcept;

    std::size_t outbound_count() const noexcept;
    std::size_t inbound_count() const noexcept;

    mc::future<mc::engine::message> async_send_with_reply(mc::engine::message msg, mc::milliseconds timeout);

protected:
    mc::proto::execution_state on_push(mc::proto::proto_request& req) override;
    mc::proto::execution_state on_pop(mc::proto::proto_request& req) override;

private:
    struct inbound_call_context : public mc::proto::proto_context {
        mc::dbus::message original_call;
    };

    struct async_state {
        bool                    stopped{false};
        std::size_t             active{0};
        std::mutex              mutex;
        std::condition_variable cv;

        bool try_enter();
        void leave();
        void stop_and_wait();
    };

    DBusHandlerResult on_filter(mc::dbus::message& wire_msg);
    void              handle_inbound_call(mc::dbus::message wire_msg);
    void              deliver_reply(mc::dbus::message reply_msg);

    bool send_method_call_and_wait_reply(mc::engine::message& msg);
    bool send_method_return(mc::proto::proto_request& req, mc::engine::message& msg);
    bool send_error_reply(mc::proto::proto_request& req, mc::engine::message& msg);
    bool send_signal(mc::engine::message& msg);

    mc::string                   m_service_name;
    mc::dbus::connection         m_connection;
    mc::connection               m_filter_slot;
    std::shared_ptr<async_state> m_async_state;
    std::atomic<std::size_t>     m_outbound_count{0};
    std::atomic<std::size_t>     m_inbound_count{0};
};

using dbus_proto_ptr = std::shared_ptr<dbus_proto>;

} // namespace mc::app

#endif // MC_APP_DBUS_PROTO_H
