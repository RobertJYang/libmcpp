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

#include <mc/app/dbus_proto.h>

#include <mc/dbus/error.h>
#include <mc/engine/message.h>
#include <mc/engine/payload.h>
#include <mc/engine/service_proto.h>
#include <mc/log/log.h>
#include <mc/runtime.h>

#include <string_view>
#include <utility>

namespace mc::app {
namespace {

inline const mc::string mc_err_internal_error   = mc::string::from_quark("mc.engine.internal_error"_q);
inline const mc::string mc_err_invalid_args     = mc::string::from_quark("mc.engine.invalid_args"_q);
inline const mc::string mc_err_not_supported    = mc::string::from_quark("mc.engine.not_supported"_q);
inline const mc::string mc_err_property_ro      = mc::string::from_quark("mc.engine.property_read_only"_q);
inline const mc::string mc_err_unknown_iface    = mc::string::from_quark("mc.engine.unknown_interface"_q);
inline const mc::string mc_err_unknown_method   = mc::string::from_quark("mc.engine.unknown_method"_q);
inline const mc::string mc_err_unknown_object   = mc::string::from_quark("mc.engine.unknown_object"_q);
inline const mc::string mc_err_unknown_property = mc::string::from_quark("mc.engine.unknown_property"_q);

mc::string_view map_engine_error_to_dbus(const mc::string& engine_name) noexcept
{
    if (engine_name == mc_err_internal_error)   return dbus::error_names::failed;
    if (engine_name == mc_err_invalid_args)     return dbus::error_names::invalid_args;
    if (engine_name == mc_err_not_supported)    return dbus::error_names::not_supported;
    if (engine_name == mc_err_property_ro)      return dbus::error_names::property_read_only;
    if (engine_name == mc_err_unknown_iface)    return dbus::error_names::unknown_interface;
    if (engine_name == mc_err_unknown_method)   return dbus::error_names::unknown_method;
    if (engine_name == mc_err_unknown_object)   return dbus::error_names::unknown_object;
    if (engine_name == mc_err_unknown_property) return dbus::error_names::unknown_property;
    // 非 mc.engine.* 前缀的错误名直接透传
    return engine_name.view();
}

mc::engine::message_type wire_type_to_engine(mc::dbus::message_type type) noexcept
{
    switch (type) {
        case mc::dbus::message_type::method_call:
            return mc::engine::message_type::method_call;
        case mc::dbus::message_type::method_return:
            return mc::engine::message_type::method_return;
        case mc::dbus::message_type::error:
            return mc::engine::message_type::error;
        case mc::dbus::message_type::signal:
            return mc::engine::message_type::signal;
        default:
            return mc::engine::message_type::invalid;
    }
}

mc::variants read_wire_args(mc::dbus::message& wire_msg)
{
    try {
        return wire_msg.read_args();
    } catch (const std::exception& ex) {
        elog("dbus_proto read wire args failed: ${err}", ("err", ex.what()));
        return {};
    }
}

std::size_t count_signature_fields(mc::string_view signature) noexcept
{
    std::size_t count = 0;
    for (mc::dbus::signature_iterator it(signature); !it.at_end(); it.next()) {
        ++count;
    }
    return count;
}

void write_wire_args(mc::dbus::message& msg, const mc::variants& args)
{
    auto writer = msg.writer();
    for (const auto& arg : args) {
        writer.write_variant_value(arg);
    }
}

mc::engine::message build_inbound_method_call(mc::dbus::message& wire_msg)
{
    mc::engine::message msg;
    msg.header.type           = mc::engine::message_type::method_call;
    msg.header.destination    = mc::string(wire_msg.get_destination());
    msg.header.sender         = mc::string(wire_msg.get_sender());
    msg.header.path           = mc::string(wire_msg.get_path());
    msg.header.interface_name = mc::string(wire_msg.get_interface());
    msg.header.member_name    = mc::string(wire_msg.get_member());
    msg.header.serial         = wire_msg.get_serial();
    msg.header.reply_serial   = 0;

    auto sig  = mc::string(wire_msg.get_signature());
    auto args = read_wire_args(wire_msg);
    msg.body  = mc::engine::make_payload<mc::engine::method_call_payload>(sig, std::move(args));
    return msg;
}

mc::engine::message build_reply_message(mc::dbus::message& reply_msg)
{
    mc::engine::message msg;
    msg.header.type           = wire_type_to_engine(reply_msg.get_type());
    msg.header.destination    = mc::string(reply_msg.get_destination());
    msg.header.sender         = mc::string(reply_msg.get_sender());
    msg.header.path           = mc::string(reply_msg.get_path());
    msg.header.interface_name = mc::string(reply_msg.get_interface());
    msg.header.member_name    = mc::string(reply_msg.get_member());
    msg.header.serial         = reply_msg.get_serial();
    msg.header.reply_serial   = reply_msg.get_reply_serial();

    if (reply_msg.is_method_return()) {
        auto        sig  = mc::string(reply_msg.get_signature());
        auto        args = read_wire_args(reply_msg);
        mc::variant value;
        if (args.size() == 1U) {
            value = std::move(args.front());
        } else if (!args.empty()) {
            value = mc::variant(std::move(args));
        }
        msg.body = mc::engine::make_payload<mc::engine::method_return_payload>(std::move(value), sig);
    } else if (reply_msg.is_error()) {
        auto       name = mc::string(reply_msg.get_error_name());
        mc::string text;
        try {
            text = mc::string(reply_msg.get_error_message());
        } catch (const std::exception&) {
            text.clear();
        }
        msg.header.error_name = name;
        msg.body              = mc::engine::make_payload<mc::engine::error_payload>(name, text);
    } else {
        msg.header.type = mc::engine::message_type::invalid;
    }
    return msg;
}

} // namespace

bool dbus_proto::async_state::try_enter()
{
    std::lock_guard lock(mutex);
    if (stopped) {
        return false;
    }
    ++active;
    return true;
}

void dbus_proto::async_state::leave()
{
    std::lock_guard lock(mutex);
    if (active != 0) {
        --active;
    }
    if (stopped && active == 0) {
        cv.notify_all();
    }
}

void dbus_proto::async_state::stop_and_wait()
{
    std::unique_lock lock(mutex);
    stopped = true;
    cv.wait(lock, [this]() {
        return active == 0;
    });
}

dbus_proto::dbus_proto(mc::string service_name, mc::dbus::connection conn)
    : m_service_name(std::move(service_name)), m_connection(std::move(conn)),
      m_async_state(std::make_shared<async_state>())
{
    if (m_connection.get_connection() != nullptr && m_connection.is_connected()) {
        m_filter_slot = m_connection.filter_message().connect([this](mc::dbus::message& wire_msg) {
            return on_filter(wire_msg);
        });
    }
}

dbus_proto::~dbus_proto()
{
    if (m_filter_slot.connected()) {
        m_filter_slot.disconnect();
    }
    if (m_async_state != nullptr) {
        m_async_state->stop_and_wait();
    }
}

mc::string_view dbus_proto::service_name() const noexcept
{
    return m_service_name;
}

mc::dbus::connection& dbus_proto::connection() noexcept
{
    return m_connection;
}

std::size_t dbus_proto::outbound_count() const noexcept
{
    return m_outbound_count.load(std::memory_order_relaxed);
}

std::size_t dbus_proto::inbound_count() const noexcept
{
    return m_inbound_count.load(std::memory_order_relaxed);
}

mc::future<mc::engine::message> dbus_proto::async_send_with_reply(mc::engine::message msg, mc::milliseconds timeout)
{
    if (msg.header.destination.empty() || msg.header.path.empty() || msg.header.interface_name.empty() ||
        msg.header.member_name.empty()) {
        mc::engine::message error;
        error.header.type         = mc::engine::message_type::error;
        error.header.reply_serial = msg.header.serial;
        error.header.error_name   = "mc.engine.invalid_args";
        error.body = mc::engine::make_payload<mc::engine::error_payload>("mc.engine.invalid_args",
                                                                         "dbus proto method_call 缺少目标字段");
        return mc::resolve(std::move(error));
    }

    auto wire = mc::dbus::message::new_method_call(msg.header.destination, msg.header.path, msg.header.interface_name,
                                                   msg.header.member_name);
    if (msg.header.serial != 0) {
        wire.set_serial(msg.header.serial);
    }
    if (const auto* call_payload = msg.try_as<mc::engine::method_call_payload>()) {
        write_wire_args(wire, call_payload->args);
    }

    auto future = m_connection.async_send_with_reply(std::move(wire), timeout);
    m_outbound_count.fetch_add(1, std::memory_order_relaxed);
    return std::move(future).then([](mc::dbus::message reply) {
        return build_reply_message(reply);
    });
}

DBusHandlerResult dbus_proto::on_filter(mc::dbus::message& wire_msg)
{
    // 入站只接 method_call；method_return 走 pending，signal 走 m_match。
    if (!wire_msg.is_method_call()) {
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    auto retained_msg = wire_msg;
    auto state        = m_async_state;
    m_inbound_count.fetch_add(1, std::memory_order_relaxed);
    mc::runtime::post([this, state, wire_msg = std::move(retained_msg)]() mutable {
        if (state == nullptr || !state->try_enter()) {
            return;
        }
        struct scope_exit {
            std::shared_ptr<async_state> state;
            ~scope_exit()
            {
                state->leave();
            }
        } guard{state};
        handle_inbound_call(std::move(wire_msg));
    }, mc::runtime::io_executor);

    return DBUS_HANDLER_RESULT_HANDLED;
}

void dbus_proto::handle_inbound_call(mc::dbus::message wire_msg)
{
    mc::engine::message msg = build_inbound_method_call(wire_msg);

    auto  request_ptr      = std::make_unique<mc::proto::proto_request>();
    auto& req              = *request_ptr;
    auto& call_ctx         = req.add_context<inbound_call_context>(this);
    call_ctx.original_call = wire_msg;
    auto& msg_ctx          = ensure_context<mc::engine::service_proto::message_context>(req);
    msg_ctx.msg            = std::move(msg);

    try {
        const auto state = pop(req);
        if (state == mc::proto::execution_state::failed) {
            elog("dbus_proto inbound pop failed: service=${s} error=${e}",
                 ("s", m_service_name)("e", mc::string(req.error().name)));
        }
    } catch (const std::exception& ex) {
        elog("dbus_proto inbound pop threw: ${err}", ("err", ex.what()));
    }
}

void dbus_proto::deliver_reply(mc::dbus::message reply_msg)
{
    mc::engine::message msg = build_reply_message(reply_msg);
    if (msg.header.type == mc::engine::message_type::invalid) {
        return;
    }

    mc::proto::proto_request req;
    auto&                    ctx = ensure_context<mc::engine::service_proto::message_context>(req);
    ctx.msg                      = std::move(msg);

    try {
        const auto state = pop(req);
        if (state == mc::proto::execution_state::failed) {
            elog("dbus_proto outbound reply pop failed: service=${s} error=${e}",
                 ("s", m_service_name)("e", mc::string(req.error().name)));
        }
    } catch (const std::exception& ex) {
        elog("dbus_proto outbound reply pop threw: ${err}", ("err", ex.what()));
    }
}

bool dbus_proto::send_method_call_and_wait_reply(mc::engine::message& msg)
{
    if (msg.header.destination.empty() || msg.header.path.empty() || msg.header.interface_name.empty() ||
        msg.header.member_name.empty()) {
        return false;
    }

    auto wire = mc::dbus::message::new_method_call(msg.header.destination, msg.header.path, msg.header.interface_name,
                                                   msg.header.member_name);
    if (const auto* call_payload = msg.try_as<mc::engine::method_call_payload>()) {
        write_wire_args(wire, call_payload->args);
    }

    auto timeout = msg.header.timeout.count() > 0 ? msg.header.timeout : mc::dbus::DBUS_TIMEOUT_DEFAULT;
    auto future  = m_connection.async_send_with_reply(std::move(wire), timeout);
    std::move(future).then([this](mc::dbus::message reply) {
        deliver_reply(std::move(reply));
    });
    m_outbound_count.fetch_add(1, std::memory_order_relaxed);
    return true;
}

bool dbus_proto::send_method_return(mc::proto::proto_request& req, mc::engine::message& msg)
{
    auto* call_ctx = req.find_context<inbound_call_context>();
    if (call_ctx == nullptr) {
        elog("dbus_proto send method_return missing inbound context: service=${s}", ("s", m_service_name));
        return false;
    }

    auto wire = mc::dbus::message::new_method_return(call_ctx->original_call);
    if (const auto* ret_payload = msg.try_as<mc::engine::method_return_payload>()) {
        auto       writer      = wire.writer();
        const auto field_count = count_signature_fields(ret_payload->signature);
        if (field_count == 0U) {
            if (!ret_payload->signature.empty()) {
                return false;
            }
        } else if (field_count == 1U) {
            if (!ret_payload->signature.empty()) {
                writer.write_variant(mc::dbus::signature_iterator(ret_payload->signature), ret_payload->value, 0);
            } else {
                writer.write_variant_value(ret_payload->value);
            }
        } else {
            if (!ret_payload->value.is_array()) {
                return false;
            }
            const auto& values = ret_payload->value.get_array();
            if (values.size() != field_count) {
                return false;
            }
            auto sig_it = mc::dbus::signature_iterator(ret_payload->signature);
            for (const auto& value : values) {
                if (sig_it.at_end()) {
                    return false;
                }
                writer.write_variant(sig_it, value, 0);
                sig_it.next();
            }
        }
    }
    if (!m_connection.send(std::move(wire))) {
        return false;
    }
    m_connection.flush();
    m_outbound_count.fetch_add(1, std::memory_order_relaxed);
    return true;
}

bool dbus_proto::send_error_reply(mc::proto::proto_request& req, mc::engine::message& msg)
{
    auto* call_ctx = req.find_context<inbound_call_context>();
    if (call_ctx == nullptr) {
        elog("dbus_proto send error_reply missing inbound context: service=${s}", ("s", m_service_name));
        return false;
    }

    mc::string name = msg.header.error_name;
    name.try_quarkize();
    mc::string text;
    if (const auto* err_payload = msg.try_as<mc::engine::error_payload>()) {
        if (name.empty()) {
            name = err_payload->name;
        }
        text = err_payload->message;
    }
    if (name.empty()) {
        name = mc::string(mc::dbus::error_names::failed);
    }
    auto dbus_name = map_engine_error_to_dbus(name);
    auto wire = mc::dbus::message::new_error(call_ctx->original_call, dbus_name, text);
    if (!m_connection.send(std::move(wire))) {
        return false;
    }
    m_connection.flush();
    m_outbound_count.fetch_add(1, std::memory_order_relaxed);
    return true;
}

bool dbus_proto::send_signal(mc::engine::message& msg)
{
    if (msg.header.path.empty() || msg.header.interface_name.empty() || msg.header.member_name.empty()) {
        return false;
    }
    auto wire = mc::dbus::message::new_signal(msg.header.path, msg.header.interface_name, msg.header.member_name);
    if (const auto* sig_payload = msg.try_as<mc::engine::signal_payload>()) {
        write_wire_args(wire, sig_payload->args);
    }
    if (!m_connection.send(std::move(wire))) {
        return false;
    }
    m_connection.flush();
    m_outbound_count.fetch_add(1, std::memory_order_relaxed);
    return true;
}

mc::proto::execution_state dbus_proto::on_push(mc::proto::proto_request& req)
{
    if (m_connection.get_connection() == nullptr || !m_connection.is_connected()) {
        return fail(req, "dbus_proto_not_connected", "dbus proto 未连接到 DBus");
    }

    auto* ctx = req.find_context<mc::engine::service_proto::message_context>();
    if (ctx == nullptr) {
        return fail(req, "dbus_proto_missing_message_context", "dbus proto 缺少 message_context");
    }

    auto& msg = ctx->msg;
    switch (msg.header.type) {
        case mc::engine::message_type::method_call:
            if (!send_method_call_and_wait_reply(msg)) {
                return fail(req, "dbus_proto_method_call_failed", "dbus proto 发起 method_call 失败");
            }
            return complete(req);
        case mc::engine::message_type::method_return:
            if (!send_method_return(req, msg)) {
                return fail(req, "dbus_proto_method_return_failed", "dbus proto 发送 method_return 失败");
            }
            return complete(req);
        case mc::engine::message_type::error:
            if (!send_error_reply(req, msg)) {
                return fail(req, "dbus_proto_error_reply_failed", "dbus proto 发送 error 回包失败");
            }
            return complete(req);
        case mc::engine::message_type::signal:
            if (!send_signal(msg)) {
                return fail(req, "dbus_proto_signal_failed", "dbus proto 发送 signal 失败");
            }
            return complete(req);
        default:
            return fail(req, "dbus_proto_unsupported_message", "dbus proto 不支持该消息类型");
    }
}

mc::proto::execution_state dbus_proto::on_pop(mc::proto::proto_request& req)
{
    return pop_next(req);
}

} // namespace mc::app
