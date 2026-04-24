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

#include <mc/engine/service_proto.h>

#include <mc/engine/engine_codec.h>

#include <utility>

namespace mc::engine {

namespace {

mc::string_view buffer_payload_view(const mc::proto::proto_request& req) noexcept
{
    const auto& buffer = req.buffer();
    const auto  base   = buffer.payload_base();
    const auto  length = buffer.length();
    if (length <= base) {
        return {};
    }
    return {reinterpret_cast<const char*>(buffer.data() + base), length - base};
}

} // namespace

service_proto::service_proto() = default;

service_proto::~service_proto() = default;

void service_proto::set_inbound_handler(inbound_handler handler)
{
    m_inbound_handler = std::move(handler);
}

void service_proto::clear_inbound_handler()
{
    m_inbound_handler = nullptr;
}

bool service_proto::has_inbound_handler() const noexcept
{
    return static_cast<bool>(m_inbound_handler);
}

void service_proto::set_decode_options(message_decode_options options)
{
    m_codec.set_decode_options(std::move(options));
}

const message_decode_options& service_proto::decode_options() const noexcept
{
    return m_codec.decode_options();
}

const engine_codec& service_proto::codec() const noexcept
{
    return m_codec;
}

mc::proto::execution_state service_proto::encode_message(mc::proto::proto_request& req, const message& msg)
{
    const auto payload = m_codec.encode(msg);
    req.buffer().clear();
    req.buffer().append_payload(payload.data(), payload.size());
    return mc::proto::execution_state::running;
}

mc::proto::execution_state service_proto::on_push(mc::proto::proto_request& req)
{
    return push_next(req);
}

mc::proto::execution_state service_proto::on_pop(mc::proto::proto_request& req)
{
    auto* ctx = req.find_context<message_context>();
    if (ctx == nullptr) {
        ctx       = &ensure_context<message_context>(req);
        ctx->msg  = m_codec.decode(buffer_payload_view(req));
    }

    if (!m_inbound_handler) {
        return complete(req);
    }

    message response;
    try {
        response = m_inbound_handler(ctx->msg);
    } catch (const std::exception& ex) {
        return fail(req, "inbound_handler_exception", ex.what());
    } catch (...) {
        return fail(req, "inbound_handler_exception", "unknown exception in inbound handler");
    }

    if (response.header.type == message_type::invalid) {
        return complete(req);
    }

    ctx->msg = std::move(response);
    encode_message(req, ctx->msg);
    return push_next(req);
}

} // namespace mc::engine
