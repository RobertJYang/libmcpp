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

#include <mc/engine/endpoint_service.h>

#include <mc/engine/dispatcher.h>
#include <mc/engine/engine.h>
#include <mc/engine/engine_codec.h>
#include <mc/engine/payload.h>
#include <mc/engine/service_proto.h>
#include <mc/future.h>
#include <mc/log/log.h>
#include <mc/runtime.h>
#include <mc/shm/default_runtime.h>
#include <mc/shm/message_queue/mq_channel.h>
#include <mc/shm/message_queue/mq_proto.h>
#include <mc/shm/message_queue/mq_transport_proto.h>
#include <mc/shm/shm_runtime.h>

#include "match/shared_table.h"

#include <atomic>
#include <optional>
#include <unistd.h>

namespace mc::engine {
namespace {

inline constexpr mc::string_view k_mq_reply_endpoint_id = "mc.mq.reply.endpoint_id";
inline constexpr mc::string_view k_mq_reply_instance_id = "mc.mq.reply.instance_id";

message make_mq_error(const message& request, mc::string_view name, mc::string_view text)
{
    message response;
    response.header.type         = message_type::error;
    response.header.destination  = request.header.sender;
    response.header.sender       = request.header.destination;
    response.header.reply_serial = request.header.serial;
    response.header.error_name   = mc::string(name);
    response.body                = make_payload<error_payload>(name, text);
    return response;
}

std::optional<std::pair<std::uint16_t, std::uint32_t>> reply_endpoint_from_context(const message& msg)
{
    auto endpoint_it = msg.header.context.find(k_mq_reply_endpoint_id);
    auto instance_it = msg.header.context.find(k_mq_reply_instance_id);
    if (endpoint_it == msg.header.context.end() || instance_it == msg.header.context.end()) {
        return std::nullopt;
    }
    return std::pair{static_cast<std::uint16_t>(endpoint_it->value.as<std::uint64_t>()),
                     static_cast<std::uint32_t>(instance_it->value.as<std::uint64_t>())};
}

} // namespace

struct endpoint_service::endpoint_service_impl {
    explicit endpoint_service_impl(mc::string_view name, std::shared_ptr<mc::shm::shm_runtime> rt)
        : m_endpoint_name(name), m_runtime(std::move(rt))
    {}

    mc::string                            m_endpoint_name;
    mc::string                            m_effective_name;
    std::shared_ptr<mc::shm::shm_runtime> m_runtime;

    endpoint_name_conflict_policy m_conflict_policy{endpoint_name_conflict_policy::refuse};

    mc::shm::endpoint           m_endpoint{};
    mc::engine::service_proto   m_service_proto;
    mc::shm::mq_proto           m_mq_proto;
    mc::shm::mq_transport_proto m_transport;
    mc::shm::mq_channel         m_channel;

    bool m_channel_started{false};
    bool m_endpoint_registered{false};

    std::atomic<std::uint64_t>                             m_next_serial{1};
};

endpoint_service::endpoint_service(mc::string_view endpoint_name, std::shared_ptr<mc::shm::shm_runtime> runtime,
                                   mc::object* parent)
    : service(endpoint_name, parent),
      m_endpoint_impl(std::make_unique<endpoint_service_impl>(endpoint_name, std::move(runtime)))
{
    m_endpoint_impl->m_service_proto.add_child(m_endpoint_impl->m_mq_proto);
    m_endpoint_impl->m_mq_proto.add_child(m_endpoint_impl->m_transport);
    m_endpoint_impl->m_channel.set_protocol(&m_endpoint_impl->m_service_proto);

    m_endpoint_impl->m_service_proto.set_inbound_handler([this](message msg) -> message {
        mc::runtime::post([this, msg = std::move(msg)]() mutable {
            handle_inbound_message(std::move(msg));
        }, mc::runtime::io_executor);
        return {};
    });
}

endpoint_service::endpoint_service(mc::string_view endpoint_name, mc::object* parent)
    : endpoint_service(endpoint_name, nullptr, parent)
{}

endpoint_service::~endpoint_service()
{
    // service::~service() 会调 stop() → on_stop()，正常情况下资源已经释放。
    // 这里兜底补一次，避免异常退出时漏 stop。
    if (m_endpoint_impl && m_endpoint_impl->m_channel_started) {
        m_endpoint_impl->m_channel.stop();
        m_endpoint_impl->m_channel_started = false;
    }
}

std::uint16_t endpoint_service::endpoint_id() const noexcept
{
    return m_endpoint_impl ? m_endpoint_impl->m_endpoint.endpoint_id : static_cast<std::uint16_t>(0);
}

std::uint32_t endpoint_service::instance_id() const noexcept
{
    return m_endpoint_impl ? m_endpoint_impl->m_endpoint.instance_id : 0U;
}

mc::string_view endpoint_service::queue_name() const noexcept
{
    if (!m_endpoint_impl) {
        return {};
    }
    return m_endpoint_impl->m_endpoint.queue_name;
}

std::shared_ptr<mc::shm::shm_runtime> endpoint_service::get_runtime() const noexcept
{
    return m_endpoint_impl ? m_endpoint_impl->m_runtime : std::shared_ptr<mc::shm::shm_runtime>{};
}

mc::shm::mq_channel* endpoint_service::get_mq_channel() const noexcept
{
    if (!m_endpoint_impl || !m_endpoint_impl->m_channel_started) {
        return nullptr;
    }
    return &m_endpoint_impl->m_channel;
}

bool endpoint_service::send_to_endpoint(std::uint16_t endpoint_id, std::uint32_t instance_id, const message& msg)
{
    if (!m_endpoint_impl || !m_endpoint_impl->m_runtime) {
        return false;
    }

    try {
        auto receiver_info = m_endpoint_impl->m_runtime->get_endpoint(endpoint_id);
        if (!receiver_info.has_value()) {
            return false;
        }

        mc::shm::endpoint receiver;
        receiver.endpoint_id   = receiver_info->endpoint_id;
        receiver.instance_id   = instance_id;
        receiver.state         = receiver_info->state;
        receiver.slot_count    = receiver_info->slot_count;
        receiver.endpoint_name = receiver_info->endpoint_name;
        receiver.queue_name    = receiver_info->queue_name;
        receiver.notifier_name = receiver_info->notifier_name;

        auto queue = m_endpoint_impl->m_runtime->open_queue(receiver);

        service_proto                 proto_root;
        mc::shm::mq_proto             mq_layer;
        mc::shm::mq_transport_proto   transport_layer;
        mc::proto::proto_request      req;
        proto_root.add_child(mq_layer);
        mq_layer.add_child(transport_layer);
        mq_layer.configure(m_endpoint_impl->m_endpoint.instance_id, m_endpoint_impl->m_endpoint.endpoint_id);
        transport_layer.configure(queue, m_endpoint_impl->m_endpoint.endpoint_id,
                                  m_endpoint_impl->m_endpoint.instance_id);

        auto& ctx = req.ensure_context<service_proto::message_context>(&proto_root);
        ctx.msg   = msg;
        auto payload = encode_message_bytes(msg);
        req.buffer().append_payload(payload.data(), payload.size());
        return proto_root.push(req) == mc::proto::execution_state::completed;
    } catch (const std::exception& ex) {
        elog("endpoint_service.send_to failed: ${what}", ("what", ex.what()));
        return false;
    }
}

mc::future<message> endpoint_service::send_with_reply_to_endpoint(std::uint16_t endpoint_id, std::uint32_t instance_id,
                                                                  message msg, mc::milliseconds timeout)
{
    if (!m_endpoint_impl || !m_endpoint_impl->m_runtime || !m_endpoint_impl->m_channel_started) {
        return mc::resolve(make_mq_error(msg, "mc.engine.not_supported", "mq endpoint is not available"));
    }

    if (msg.header.serial == 0) {
        msg.header.serial = m_endpoint_impl->m_next_serial.fetch_add(1, std::memory_order_relaxed);
    }
    msg.header.context[k_mq_reply_endpoint_id] = static_cast<std::uint64_t>(m_endpoint_impl->m_endpoint.endpoint_id);
    msg.header.context[k_mq_reply_instance_id] = static_cast<std::uint64_t>(m_endpoint_impl->m_endpoint.instance_id);

    auto timeout_msg = make_mq_error(msg, "mc.engine.timeout", "mq request timed out");
    auto future = m_endpoint_impl->m_channel
                      .register_pending_reply(msg.header.serial, timeout, encode_message_bytes(timeout_msg))
                      .then([](mc::string payload) {
        return message::from_bytes(payload);
    });

    if (!send_to_endpoint(endpoint_id, instance_id, msg)) {
        auto error = make_mq_error(msg, "mc.engine.transport_failed", "mq send failed");
        m_endpoint_impl->m_channel.cancel_pending_reply(msg.header.serial, encode_message_bytes(error));
    }

    return future;
}

bool endpoint_service::complete_pending_reply(const message& msg)
{
    if (msg.header.reply_serial == 0 || !m_endpoint_impl) {
        return false;
    }

    return m_endpoint_impl->m_channel.complete_pending_reply(msg.header.reply_serial, encode_message_bytes(msg));
}

void endpoint_service::handle_inbound_message(message msg)
{
    if (msg.header.type == message_type::method_return || msg.header.type == message_type::error) {
        if (complete_pending_reply(msg)) {
            return;
        }
    }

    if (msg.header.type == message_type::method_call) {
        if (auto reply_endpoint = reply_endpoint_from_context(msg); reply_endpoint.has_value()) {
            message response;
            if (auto* target = engine::find_service(msg.header.destination); target != nullptr) {
                response = mc::engine::dispatch(*target, msg);
            } else {
                response = make_mq_error(msg, "mc.engine.unknown_object", "mq target service is not local");
            }
            (void)send_to_endpoint(reply_endpoint->first, reply_endpoint->second, response);
            return;
        }
    }

    mc::engine::engine::route_inbound(msg);
}

match::table_ptr endpoint_service::create_match_table() const
{
    if (!m_endpoint_impl || !m_endpoint_impl->m_endpoint_registered || !m_endpoint_impl->m_runtime) {
        return {};
    }
    try {
        return mc::make_shared<match::shared_table>(*m_endpoint_impl->m_runtime,
                                                    m_endpoint_impl->m_endpoint.endpoint_id,
                                                    m_endpoint_impl->m_endpoint.instance_id);
    } catch (const std::exception& ex) {
        elog("endpoint_service.create_match_table failed: ${what}", ("what", ex.what()));
        return {};
    }
}

void endpoint_service::set_name_conflict_policy(endpoint_name_conflict_policy policy) noexcept
{
    if (m_endpoint_impl) {
        m_endpoint_impl->m_conflict_policy = policy;
    }
}

mc::string_view endpoint_service::effective_endpoint_name() const noexcept
{
    if (!m_endpoint_impl) {
        return {};
    }
    return m_endpoint_impl->m_effective_name;
}

void endpoint_service::push_outbound(const message& msg)
{
    if (!m_endpoint_impl || !m_endpoint_impl->m_channel_started) {
        return;
    }

    try {
        mc::proto::proto_request req;
        auto&                    ctx = req.ensure_context<service_proto::message_context>(
            &m_endpoint_impl->m_service_proto);
        ctx.msg = msg;

        auto payload = encode_message_bytes(msg);
        req.buffer().append_payload(payload.data(), payload.size());
        (void)m_endpoint_impl->m_service_proto.push(req);
    } catch (const std::exception& ex) {
        elog("endpoint_service.push_outbound failed: ${what}", ("what", ex.what()));
    }
}

bool endpoint_service::on_start()
{
    if (!m_endpoint_impl) {
        return false;
    }
    if (m_endpoint_impl->m_endpoint_name.empty()) {
        elog("endpoint_service.on_start: endpoint_name 为空，拒绝启动");
        return false;
    }
    if (!m_endpoint_impl->m_runtime) {
        try {
            m_endpoint_impl->m_runtime =
                std::shared_ptr<mc::shm::shm_runtime>(&mc::shm::default_runtime(), [](mc::shm::shm_runtime*) {
            });
        } catch (...) {
            elog("endpoint_service.on_start: default_runtime 不可用");
            return false;
        }
    }
    if (!m_endpoint_impl->m_runtime || !m_endpoint_impl->m_runtime->is_valid()) {
        elog("endpoint_service.on_start: shm_runtime 无效");
        return false;
    }

    auto& rt = *m_endpoint_impl->m_runtime;

    mc::string final_name(m_endpoint_impl->m_endpoint_name);

    if (auto existing = rt.find_endpoint_by_name(final_name); existing.has_value()) {
        if (rt.is_endpoint_alive(existing->endpoint_id)) {
            switch (m_endpoint_impl->m_conflict_policy) {
                case endpoint_name_conflict_policy::refuse:
                    elog("endpoint_service.on_start: endpoint '${name}' 已被活进程 pid=${pid} 占用",
                         ("name", final_name)("pid", existing->owner_pid));
                    return false;
                case endpoint_name_conflict_policy::append_pid: {
                    mc::string renamed = final_name;
                    renamed += '-';
                    renamed += mc::to_string(static_cast<std::int64_t>(::getpid()));
                    if (auto clash = rt.find_endpoint_by_name(renamed);
                        clash.has_value() && rt.is_endpoint_alive(clash->endpoint_id)) {
                        elog("endpoint_service.on_start: append_pid 兜底名 '${name}' 仍冲突 pid=${pid}",
                             ("name", renamed)("pid", clash->owner_pid));
                        return false;
                    }
                    final_name = std::move(renamed);
                    break;
                }
            }
        }
    }

    auto registered = rt.register_endpoint(final_name);
    if (!registered.has_value()) {
        elog("endpoint_service.on_start: register_endpoint 失败 name=${name}", ("name", final_name));
        return false;
    }
    m_endpoint_impl->m_endpoint            = *registered;
    m_endpoint_impl->m_effective_name      = std::move(final_name);
    m_endpoint_impl->m_endpoint_registered = true;

    if (!rt.mark_endpoint_running(m_endpoint_impl->m_endpoint)) {
        elog("endpoint_service.on_start: mark_endpoint_running 失败");
        rt.abort_endpoint(m_endpoint_impl->m_endpoint);
        m_endpoint_impl->m_endpoint_registered = false;
        return false;
    }

    try {
        m_endpoint_impl->m_channel.start(m_endpoint_impl->m_runtime, m_endpoint_impl->m_endpoint,
                                         m_endpoint_impl->m_endpoint.instance_id);
    } catch (const std::exception& ex) {
        elog("endpoint_service.on_start: mq_channel.start 抛异常: ${what}", ("what", ex.what()));
        rt.abort_endpoint(m_endpoint_impl->m_endpoint);
        m_endpoint_impl->m_endpoint_registered = false;
        return false;
    }
    m_endpoint_impl->m_channel_started = true;
    return true;
}

bool endpoint_service::on_stop()
{
    if (!m_endpoint_impl) {
        return true;
    }
    if (m_endpoint_impl->m_channel_started) {
        m_endpoint_impl->m_channel.stop();
        m_endpoint_impl->m_channel_started = false;
    }
    if (m_endpoint_impl->m_endpoint_registered && m_endpoint_impl->m_runtime) {
        m_endpoint_impl->m_runtime->abort_endpoint(m_endpoint_impl->m_endpoint);
        m_endpoint_impl->m_endpoint_registered = false;
    }
    return true;
}

} // namespace mc::engine
