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
#include <mc/engine/error_engine.h>
#include <mc/exception.h>

namespace mc::engine {

context::context(service& s, abstract_object& object) : m_service(s), m_object(object) {
}

context::~context() {
}

mc::engine::error& context::get_error() {
    return m_error;
}

bool context::is_error() const {
    return m_error.is_set();
}

void context::report_error(std::string_view error_name, mc::dict args) {
    m_error = error_engine::get_instance().report_error(error_name, std::move(args));
}

void context::set_arg(std::string_view key, mc::variant value) {
    m_args[key] = std::move(value);
}

mc::variant context::get_arg(std::string_view key) const {
    if (m_args.find(key) == m_args.end()) {
        return mc::variant();
    }

    return m_args[key];
}

const mc::dict& context::get_args() const {
    return m_args;
}

mc::dict& context::get_args() {
    return m_args;
}

void context::set_args(mc::dict args) {
    m_args = std::move(args);
}

service& context::get_service() const {
    return m_service;
}

abstract_object& context::get_object() const {
    return m_object;
}

detail::call_info& context::get_call_info() {
    return m_call_info;
}

void context::set_call_info(detail::call_info call_info) {
    m_call_info = std::move(call_info);
}

std::string_view context::get_path() const {
    if (std::holds_alternative<detail::dbus_call>(m_call_info)) {
        return std::get<detail::dbus_call>(m_call_info).request.get_path();
    } else if (std::holds_alternative<detail::variants_call>(m_call_info)) {
        return std::get<detail::variants_call>(m_call_info).path;
    }

    return {};
}

std::string_view context::get_method_name() const {
    if (std::holds_alternative<detail::dbus_call>(m_call_info)) {
        return std::get<detail::dbus_call>(m_call_info).request.get_member();
    } else if (std::holds_alternative<detail::variants_call>(m_call_info)) {
        return std::get<detail::variants_call>(m_call_info).method_name;
    }

    return {};
}

std::string_view context::get_interface_name() const {
    if (std::holds_alternative<detail::dbus_call>(m_call_info)) {
        return std::get<detail::dbus_call>(m_call_info).request.get_interface();
    } else if (std::holds_alternative<detail::variants_call>(m_call_info)) {
        return std::get<detail::variants_call>(m_call_info).interface_name;
    }

    return {};
}

std::string_view context::get_sender() const {
    if (std::holds_alternative<detail::dbus_call>(m_call_info)) {
        return std::get<detail::dbus_call>(m_call_info).request.get_sender();
    } else if (std::holds_alternative<detail::variants_call>(m_call_info)) {
        return std::get<detail::variants_call>(m_call_info).sender;
    }

    return {};
}

void context::throw_error(std::string_view error_name, mc::dict args) {
    auto* ctx = context_stack::top_value();
    if (!ctx) {
        MC_THROW(mc::method_call_exception, "context not found");
    }

    ctx->report_error(error_name, std::move(args));
    MC_THROW(mc::method_call_exception, "call method ${method} at ${path} failed: ${error}",
             ("method", ctx->get_method_name())("path", ctx->get_path())("error", error_name));
}

void context::throw_error(const error_info& error, mc::dict args) {
    throw_error(error.name, std::move(args));
}

} // namespace mc::engine
