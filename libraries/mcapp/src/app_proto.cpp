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

#include <mc/engine/engine.h>
#include <mc/engine/payload.h>
#if MCENGINE_USE_SHM
#include <mc/engine/endpoint_service.h>
#endif

#include <optional>

namespace mc::app {
namespace {

inline constexpr mc::string_view k_mq_target_endpoint_id_key = "mc.mq.target.endpoint_id";
inline constexpr mc::string_view k_mq_target_instance_id_key = "mc.mq.target.instance_id";

mc::engine::message make_route_error(const mc::engine::message& request, mc::string_view name, mc::string_view text)
{
    mc::engine::message response;
    response.header.type         = mc::engine::message_type::error;
    response.header.destination  = request.header.sender;
    response.header.sender       = request.header.destination;
    response.header.reply_serial = request.header.serial;
    response.header.error_name   = mc::string(name);
    response.body                = mc::engine::make_payload<mc::engine::error_payload>(name, text);
    return response;
}

#if MCENGINE_USE_SHM
std::optional<std::pair<std::uint16_t, std::uint32_t>> mq_target_from_context(const mc::engine::message& request)
{
    auto endpoint_it = request.header.context.find(k_mq_target_endpoint_id_key);
    auto instance_it = request.header.context.find(k_mq_target_instance_id_key);
    if (endpoint_it == request.header.context.end() || instance_it == request.header.context.end()) {
        return std::nullopt;
    }
    return std::pair{static_cast<std::uint16_t>(endpoint_it->value.as<std::uint64_t>()),
                     static_cast<std::uint32_t>(instance_it->value.as<std::uint64_t>())};
}
#endif

} // namespace

app_proto::app_proto(mc::string service_name, mc::dbus::connection conn, mc::shm::mq_channel* mq_channel)
    : m_service_name(std::move(service_name)), m_dbus(m_service_name, std::move(conn))
#if MCENGINE_USE_SHM
      ,
      m_mq_channel(mq_channel)
#endif
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

mc::future<mc::engine::message> app_proto::async_send_with_reply(mc::engine::message request, mc::milliseconds timeout)
{
    auto source_it = request.header.context.find(message_source_key);
    if (source_it != request.header.context.end()) {
        const auto source = source_it->value.as<mc::string>();
        if (source == "mq") {
#if MCENGINE_USE_SHM
            auto* endpoint = mc::engine::engine::get_endpoint_service();
            auto  target   = mq_target_from_context(request);
            if (m_mq_channel == nullptr || endpoint == nullptr || !target.has_value()) {
                return mc::resolve(make_route_error(request, "mc.engine.not_supported", "mq channel is not available"));
            }
            return endpoint->send_with_reply_to_endpoint(target->first, target->second, std::move(request), timeout);
#else
            return mc::resolve(make_route_error(request, "mc.engine.not_supported", "mq channel is not available"));
#endif
        }
        if (source == "dbus") {
            return m_dbus.async_send_with_reply(std::move(request), timeout);
        }
    }

    return m_dbus.async_send_with_reply(std::move(request), timeout);
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
#if MCENGINE_USE_SHM
            if (m_mq_channel != nullptr) {
                auto mq_req = std::make_unique<mc::proto::proto_request>();
                encode_message(*mq_req, msg);
                m_mq_channel->send_owned(std::move(mq_req));
            }
#endif
            return complete(req);
        }
        if (source == "dbus") {
            return push_to(req, m_dbus);
        }
    }

    // request/reply 只走单一 transport。
    if (msg.header.type == mc::engine::message_type::method_call) {
        return push_to(req, m_dbus);
    }

    // fire-and-forget 消息可同时投递到 DBus 和 MQ。
    auto state = push_to(req, m_dbus);
    if (state == mc::proto::execution_state::failed) {
        return state;
    }

#if MCENGINE_USE_SHM
    if (m_mq_channel != nullptr) {
        auto mq_req = std::make_unique<mc::proto::proto_request>();
        encode_message(*mq_req, msg);
        m_mq_channel->send_owned(std::move(mq_req));
    }
#endif

    return complete(req);
}

mc::proto::execution_state app_proto::on_pop(mc::proto::proto_request& req)
{
    return mc::engine::service_proto::on_pop(req);
}

} // namespace mc::app
