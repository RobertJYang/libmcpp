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

#ifndef MC_APP_APP_PROTO_H
#define MC_APP_APP_PROTO_H

#include <mc/app/dbus_proto.h>
#include <mc/dbus/connection.h>
#include <mc/engine/service_proto.h>
#include <mc/shm/message_queue/mq_channel.h>
#include <mc/string.h>

#include <cstdint>
#include <memory>

namespace mc::app {

inline constexpr mc::string_view message_source_key = "mcapp.source";

class MC_API app_proto : public mc::engine::service_proto {
public:
    app_proto(mc::string service_name, mc::dbus::connection conn, mc::shm::mq_channel* mq_channel);
    ~app_proto() override;

    app_proto(const app_proto&)            = delete;
    app_proto& operator=(const app_proto&) = delete;
    app_proto(app_proto&&)                 = delete;
    app_proto& operator=(app_proto&&)      = delete;

    mc::string_view service_name() const noexcept;

    dbus_proto& dbus() noexcept;

    mc::future<mc::engine::message> async_send_with_reply(
        mc::engine::message request, mc::milliseconds timeout = mc::milliseconds(5000)) override;

protected:
    mc::proto::execution_state on_push(mc::proto::proto_request& req) override;
    mc::proto::execution_state on_pop(mc::proto::proto_request& req) override;

private:
    mc::string           m_service_name;
    dbus_proto           m_dbus;
    mc::shm::mq_channel* m_mq_channel{nullptr};
};

using app_proto_ptr = std::shared_ptr<app_proto>;

} // namespace mc::app

#endif // MC_APP_APP_PROTO_H
