/**
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

#include <mc/engine/context.h>
#include <mc/error_engine.h>
#include <mc/exception.h>
#include <mc/reflect/type_code.h>
#include <mc/string.h>

#include <cstdlib>
#include <limits>

namespace mc::engine {
namespace {

constexpr mc::string_view k_auth_key           = "Auth";
constexpr mc::string_view k_privilege_key      = "Privilege";
constexpr mc::string_view k_username_key       = "username";
constexpr mc::string_view k_client_addr_key    = "client_addr";
constexpr mc::string_view k_interface_name_key = "interface_name";
constexpr mc::string_view k_override_mode_key  = "OverrideMode";

// 环境变量 MC_CONTEXT_SIGNATURE：见 context_signature_mode；主要影响 context_signature_string()，
// 不驱动内部 RPC 快照的 dict 内容及归一逻辑。
context_signature_mode init_context_signature_mode() noexcept
{
    const char* env = std::getenv("MC_CONTEXT_SIGNATURE");
    if (env == nullptr || env[0] == '\0' || env[1] != '\0') {
        return context_signature_mode::a_ss;
    }
    switch (env[0]) {
        case '0':
            return context_signature_mode::none;
        case '2':
            return context_signature_mode::a_sv;
        default:
            return context_signature_mode::a_ss;
    }
}

const context_signature_mode g_context_signature_mode = init_context_signature_mode();

bool is_local_only_arg_key(mc::string_view key)
{
    return key == k_override_mode_key;
}

[[noreturn]] void throw_context_cast_error(mc::string_view key)
{
    MC_THROW(mc::bad_cast_exception, "context field ${field} type invalid", ("field", key));
}

std::optional<auth_state> parse_auth_value(const mc::variant& value)
{
    if (value.is_null()) {
        return std::nullopt;
    }

    int64_t raw_value = 0;
    try {
        if (value.is_string()) {
            raw_value = mc::string::to_number<int64_t>(value.as_string());
        } else {
            raw_value = value.as_int64();
        }
    } catch (const std::exception&) {
        throw_context_cast_error(k_auth_key);
    }

    switch (raw_value) {
        case static_cast<int64_t>(auth_state::no_auth):
            return auth_state::no_auth;
        case static_cast<int64_t>(auth_state::auth_required):
            return auth_state::auth_required;
        default:
            throw_context_cast_error(k_auth_key);
    }
}

std::optional<uint32_t> parse_privilege_value(const mc::variant& value)
{
    if (value.is_null()) {
        return std::nullopt;
    }

    try {
        if (value.is_string()) {
            return mc::string::to_number<uint32_t>(value.as_string());
        }
        if (value.is_uint32() || value.is_uint64()) {
            return value.as_uint32();
        }

        auto raw_value = value.as_int64();
        if (raw_value < 0 || raw_value > std::numeric_limits<uint32_t>::max()) {
            throw_context_cast_error(k_privilege_key);
        }
        return static_cast<uint32_t>(raw_value);
    } catch (const std::exception&) {
        throw_context_cast_error(k_privilege_key);
    }
}

std::optional<mc::string> parse_string_value(const mc::variant& value, mc::string_view key)
{
    if (value.is_null()) {
        return std::nullopt;
    }
    if (!value.is_string()) {
        throw_context_cast_error(key);
    }
    return value.as_string();
}

void set_or_erase(mc::dict& dict, mc::string_view key, mc::variant value)
{
    if (value.is_null()) {
        dict.erase(key);
        return;
    }
    dict[key] = std::move(value);
}

void merge_if_absent(mc::dict& dict, mc::string_view key, mc::variant value)
{
    if (!value.is_null() && !dict.contains(key)) {
        dict[key] = std::move(value);
    }
}

mc::dict build_internal_rpc_context_snapshot(const context& ctx)
{
    mc::dict args;

    for (const auto& entry : ctx.extensions()) {
        if (!entry.key.is_string()) {
            continue;
        }
        args[entry.key.as_string()] = entry.value;
    }

    if (ctx.auth().has_value()) {
        args[k_auth_key] = mc::variant{static_cast<int64_t>(*ctx.auth())};
    }
    if (ctx.privilege().has_value()) {
        args[k_privilege_key] = mc::variant{*ctx.privilege()};
    }
    if (ctx.username().has_value()) {
        args[k_username_key] = *ctx.username();
    }
    if (ctx.client_addr().has_value()) {
        args[k_client_addr_key] = *ctx.client_addr();
    }
    if (ctx.interface_name().has_value()) {
        args[k_interface_name_key] = *ctx.interface_name();
    }

    return args;
}

} // namespace

context_signature_mode current_context_signature_mode() noexcept
{
    return g_context_signature_mode;
}

mc::string_view context_signature_string() noexcept
{
    if (g_context_signature_mode == context_signature_mode::a_sv) {
        return mc::reflect::container::dict_string_var;
    }
    return mc::reflect::container::dict_string_string;
}

mc::string_view internal_context_wire_signature_string() noexcept
{
    return mc::reflect::container::dict_string_var;
}

context::context() : m_state(mc::make_shared<detail::context_state>())
{}

context::context(service& s, abstract_object& object) : context()
{
    m_frame.emplace();
    m_frame->service_ptr = &s;
    m_frame->object_ptr  = &object;
}

context::~context()
{}

context::context(mc::shared_ptr<detail::context_state> state) : m_state(std::move(state))
{
    if (!m_state) {
        m_state = mc::make_shared<detail::context_state>();
    }
}

context::context(const context& rhs)
    : m_state(rhs.m_state ? rhs.m_state : mc::make_shared<detail::context_state>()), m_frame(rhs.m_frame)
{}

context::context(context&& rhs) noexcept : m_state(std::move(rhs.m_state)), m_frame(std::move(rhs.m_frame))
{
    if (!m_state) {
        m_state = mc::make_shared<detail::context_state>();
    }
}

context& context::operator=(const context& rhs)
{
    if (this == &rhs) {
        return *this;
    }

    m_state = rhs.m_state ? rhs.m_state : mc::make_shared<detail::context_state>();
    m_frame = rhs.m_frame;
    return *this;
}

context& context::operator=(context&& rhs) noexcept
{
    if (this == &rhs) {
        return *this;
    }

    m_state = std::move(rhs.m_state);
    m_frame = std::move(rhs.m_frame);
    if (!m_state) {
        m_state = mc::make_shared<detail::context_state>();
    }
    return *this;
}

mc::shared_ptr<detail::context_state>& context::ensure_state()
{
    if (!m_state) {
        m_state = mc::make_shared<detail::context_state>();
    }
    return m_state;
}

const mc::shared_ptr<detail::context_state>& context::ensure_state() const
{
    return const_cast<context*>(this)->ensure_state();
}

detail::context_frame& context::ensure_frame()
{
    if (!m_frame.has_value()) {
        m_frame.emplace();
    }
    return *m_frame;
}

const detail::context_frame* context::frame_ptr() const noexcept
{
    return m_frame.has_value() ? &*m_frame : nullptr;
}

context context::current()
{
    return get_current_context();
}

context make_method_context_arg()
{
    return context::current();
}

context context::clone() const
{
    auto        cloned_state     = mc::make_shared<detail::context_state>();
    const auto& state            = *ensure_state();
    cloned_state->auth           = state.auth;
    cloned_state->privilege      = state.privilege;
    cloned_state->username       = state.username;
    cloned_state->client_addr    = state.client_addr;
    cloned_state->interface_name = state.interface_name;
    cloned_state->extensions     = state.extensions.copy();
    cloned_state->local          = state.local.copy();
    context cloned_ctx(std::move(cloned_state));
    cloned_ctx.m_frame = m_frame;
    return cloned_ctx;
}

std::optional<auth_state> context::auth() const
{
    return ensure_state()->auth;
}

void context::set_auth(std::optional<auth_state> value)
{
    ensure_state()->auth = std::move(value);
}

std::optional<uint32_t> context::privilege() const
{
    return ensure_state()->privilege;
}

void context::set_privilege(std::optional<uint32_t> value)
{
    ensure_state()->privilege = std::move(value);
}

std::optional<mc::string> context::username() const
{
    return ensure_state()->username;
}

void context::set_username(std::optional<mc::string> value)
{
    ensure_state()->username = std::move(value);
}

std::optional<mc::string> context::client_addr() const
{
    return ensure_state()->client_addr;
}

void context::set_client_addr(std::optional<mc::string> value)
{
    ensure_state()->client_addr = std::move(value);
}

std::optional<mc::string> context::interface_name() const
{
    return ensure_state()->interface_name;
}

void context::set_interface_name(std::optional<mc::string> value)
{
    ensure_state()->interface_name = std::move(value);
}

mc::dict& context::extensions()
{
    return ensure_state()->extensions;
}

const mc::dict& context::extensions() const
{
    return ensure_state()->extensions;
}

mc::dict& context::local()
{
    return ensure_state()->local;
}

const mc::dict& context::local() const
{
    return ensure_state()->local;
}

mc::variant context::export_auth_arg() const
{
    if (!auth().has_value()) {
        return {};
    }
    return static_cast<int64_t>(*auth());
}

mc::variant context::export_privilege_arg() const
{
    if (!privilege().has_value()) {
        return {};
    }
    return *privilege();
}

mc::error_ptr context::get_error()
{
    auto* frame = frame_ptr();
    return frame ? frame->error : error_ptr{};
}

bool context::is_error() const
{
    auto* frame = frame_ptr();
    return frame && frame->error && frame->error->is_set();
}

void context::report_error(mc::string_view error_name, mc::dict args)
{
    ensure_frame().error = error_engine::get_instance().report_error(error_name, std::move(args));
}

void context::report_error(const error_info& error, mc::dict args)
{
    ensure_frame().error = error_engine::get_instance().report_error(error, std::move(args));
}

void context::set_arg(mc::string_view key, mc::variant value)
{
    if (key == k_auth_key) {
        set_auth(parse_auth_value(value));
        return;
    }
    if (key == k_privilege_key) {
        set_privilege(parse_privilege_value(value));
        return;
    }
    if (key == k_username_key) {
        set_username(parse_string_value(value, key));
        return;
    }
    if (key == k_client_addr_key) {
        set_client_addr(parse_string_value(value, key));
        return;
    }
    if (key == k_interface_name_key) {
        set_interface_name(parse_string_value(value, key));
        return;
    }
    if (is_local_only_arg_key(key)) {
        set_or_erase(local(), key, std::move(value));
        return;
    }

    set_or_erase(extensions(), key, std::move(value));
}

mc::variant context::get_arg(mc::string_view key) const
{
    if (local().contains(key)) {
        return local()[key];
    }
    if (key == k_auth_key) {
        return export_auth_arg();
    }
    if (key == k_privilege_key) {
        return export_privilege_arg();
    }
    if (key == k_username_key) {
        return username().has_value() ? mc::variant(*username()) : mc::variant();
    }
    if (key == k_client_addr_key) {
        return client_addr().has_value() ? mc::variant(*client_addr()) : mc::variant();
    }
    if (key == k_interface_name_key) {
        return interface_name().has_value() ? mc::variant(*interface_name()) : mc::variant();
    }
    if (extensions().contains(key)) {
        return extensions()[key];
    }
    return {};
}

mc::dict context::get_args() const
{
    mc::dict args = local().copy();
    merge_if_absent(args, k_auth_key, export_auth_arg());
    merge_if_absent(args, k_privilege_key, export_privilege_arg());
    merge_if_absent(args, k_username_key, username().has_value() ? mc::variant(*username()) : mc::variant());
    merge_if_absent(args, k_client_addr_key, client_addr().has_value() ? mc::variant(*client_addr()) : mc::variant());
    merge_if_absent(args, k_interface_name_key,
                    interface_name().has_value() ? mc::variant(*interface_name()) : mc::variant());

    for (const auto& entry : extensions()) {
        if (!args.contains(entry.key)) {
            args[entry.key] = entry.value;
        }
    }
    return args;
}

void context::set_args(const mc::dict& args)
{
    auto& state = *ensure_state();
    state.auth.reset();
    state.privilege.reset();
    state.username.reset();
    state.client_addr.reset();
    state.interface_name.reset();
    state.extensions = {};
    state.local      = {};

    for (const auto& entry : args) {
        if (entry.key.is_string()) {
            auto key = entry.key.as_string();
            set_arg(key, entry.value);
            continue;
        }
        state.extensions[entry.key] = entry.value;
    }
}

service& context::get_service() const
{
    auto* service_ptr = get_service_ptr();
    if (service_ptr == nullptr) {
        MC_THROW(mc::runtime_exception, "service not found in current context");
    }
    return *service_ptr;
}

abstract_object& context::get_object() const
{
    auto* object_ptr = get_object_ptr();
    if (object_ptr == nullptr) {
        MC_THROW(mc::runtime_exception, "object not found in current context");
    }
    return *object_ptr;
}

service* context::get_service_ptr() const noexcept
{
    auto* frame = frame_ptr();
    return frame ? frame->service_ptr : nullptr;
}

abstract_object* context::get_object_ptr() const noexcept
{
    auto* frame = frame_ptr();
    return frame ? frame->object_ptr : nullptr;
}

detail::call_info& context::get_call_info()
{
    return ensure_frame().call_info;
}

void context::set_call_info(detail::call_info call_info)
{
    ensure_frame().call_info = std::move(call_info);
}

mc::string_view context::get_path() const
{
    auto* frame = frame_ptr();
    return frame ? frame->call_info.path : mc::string_view{};
}

mc::string_view context::get_method_name() const
{
    auto* frame = frame_ptr();
    return frame ? frame->call_info.method_name : mc::string_view{};
}

mc::string_view context::get_interface_name() const
{
    auto* frame = frame_ptr();
    return frame ? frame->call_info.interface_name : mc::string_view{};
}

mc::string_view context::get_sender() const
{
    auto* frame = frame_ptr();
    return frame ? frame->call_info.sender : mc::string_view{};
}

void context::throw_error(mc::string_view error_name, mc::dict args)
{
    auto& ctx = get_current_context();
    ctx.report_error(error_name, std::move(args));
    MC_THROW(mc::method_call_exception, "call method ${method} at ${path} failed: ${error}",
             ("method", ctx.get_method_name())("path", ctx.get_path())("error", error_name));
}

void context::throw_error(const error_info& error, mc::dict args)
{
    auto& ctx = get_current_context();
    ctx.report_error(error, std::move(args));
    MC_THROW(mc::method_call_exception, "call method ${method} at ${path} failed: ${error}",
             ("method", ctx.get_method_name())("path", ctx.get_path())("error", error.name));
}

context& context::get_current_context()
{
    auto* ctx = context_stack::top_value();
    if (!ctx) {
        MC_THROW(mc::method_call_exception, "context not found, maybe you are not in a method call");
    }

    return *ctx;
}

context* context::get_current_context_ptr()
{
    return context_stack::top_value();
}

const method_type_info* context::get_method() const
{
    return get_method_ptr();
}

const method_type_info* context::get_method_ptr() const noexcept
{
    auto* frame = frame_ptr();
    return frame ? frame->method : nullptr;
}

void context::set_method(const method_type_info* method)
{
    ensure_frame().method = method;
}

void context::ignore()
{
    ensure_frame().status = handler_status::ignored;
}

void context::accept()
{
    ensure_frame().status = handler_status::accepted;
}

handler_status context::get_status() const
{
    auto* frame = frame_ptr();
    return frame ? frame->status : handler_status::accepted;
}

mc::dict context::snapshot_for_internal_rpc() const
{
    return build_internal_rpc_context_snapshot(*this);
}

} // namespace mc::engine
