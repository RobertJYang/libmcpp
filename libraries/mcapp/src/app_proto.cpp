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

#include <mc/app/app_proto.h>

namespace mc::app {

app_proto::app_proto(mc::string service_name, mc::dbus::connection conn, mc::shm::mq_channel* mq_channel)
    : m_service_name(std::move(service_name)), m_dbus(m_service_name, std::move(conn)), m_mq_channel(mq_channel)
{
    add_child(m_dbus);
}

app_proto::~app_proto() = default;

mc::string_view app_proto::service_name() const noexcept
{
    return m_service_name;
}

dbus_proto& app_proto::dbus() noexcept
{
    return m_dbus;
}

mc::proto::execution_state app_proto::on_push(mc::proto::proto_request& req)
{
    auto* ctx = req.find_context<mc::engine::service_proto::message_context>();
    if (ctx == nullptr) {
        return fail(req, "missing_message_context", "app_proto 缺少 message_context");
    }

    const auto& msg = ctx->msg;

    // 按 context 中的来源标记路由
    auto source_it = msg.header.context.find(message_source_key);
    if (source_it != msg.header.context.end()) {
        const auto source = source_it->value.as<mc::string>();
        if (source == "mq") {
            if (m_mq_channel != nullptr) {
                auto mq_req = std::make_unique<mc::proto::proto_request>();
                encode_message(*mq_req, msg);
                m_mq_channel->send_owned(std::move(mq_req));
            }
            return complete(req);
        }
        if (source == "dbus") {
            return push_to(req, m_dbus);
        }
    }

    // 无来源时默认走 dbus；配置了 mq_channel 时再发 MQ
    auto state = push_to(req, m_dbus);
    if (state == mc::proto::execution_state::failed) {
        return state;
    }

    if (m_mq_channel != nullptr) {
        auto mq_req = std::make_unique<mc::proto::proto_request>();
        encode_message(*mq_req, msg);
        m_mq_channel->send_owned(std::move(mq_req));
    }

    return complete(req);
}

mc::proto::execution_state app_proto::on_pop(mc::proto::proto_request& req)
{
    return mc::engine::service_proto::on_pop(req);
}

} // namespace mc::app
