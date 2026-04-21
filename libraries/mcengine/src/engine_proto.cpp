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

#include <mc/engine/engine_proto.h>

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

engine_proto::engine_proto() = default;

engine_proto::~engine_proto() = default;

void engine_proto::set_inbound_handler(inbound_handler handler)
{
    m_inbound_handler = std::move(handler);
}

void engine_proto::clear_inbound_handler()
{
    m_inbound_handler = nullptr;
}

bool engine_proto::has_inbound_handler() const noexcept
{
    return static_cast<bool>(m_inbound_handler);
}

void engine_proto::set_decode_options(message_decode_options options)
{
    m_decode_options = std::move(options);
}

const message_decode_options& engine_proto::decode_options() const noexcept
{
    return m_decode_options;
}

mc::proto::execution_state engine_proto::_encode_message(mc::proto::proto_request& req, const message& msg)
{
    const auto payload = msg.to_bytes();
    req.buffer().clear();
    req.buffer().append_payload(payload.data(), payload.size());
    return mc::proto::execution_state::running;
}

mc::proto::execution_state engine_proto::on_push(mc::proto::proto_request& req)
{
    auto* ctx = req.find_context<message_context>(this);
    if (ctx == nullptr) {
        return fail(req, "missing_message_context", "engine_proto 缺少 message_context");
    }
    _encode_message(req, ctx->msg);
    return push_next(req);
}

mc::proto::execution_state engine_proto::on_pop(mc::proto::proto_request& req)
{
    auto& ctx = ensure_context<message_context>(req);
    ctx.msg   = message::from_bytes(buffer_payload_view(req), m_decode_options);

    if (!m_inbound_handler) {
        return complete(req);
    }

    auto response = m_inbound_handler(ctx.msg);
    if (response.header.type == message_type::invalid) {
        return complete(req);
    }

    ctx.msg = std::move(response);
    _encode_message(req, ctx.msg);
    return push_next(req);
}

} // namespace mc::engine
