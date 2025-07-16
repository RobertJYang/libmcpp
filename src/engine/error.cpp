/*
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

#include <mc/engine/error.h>
#include <mc/engine/error_engine.h>
#include <mc/exception.h>
#include <mc/fmt/format_dict.h>
#include <mc/json.h>
#include <mc/log.h>
#include <mc/string.h>

#include <string_view>
#include <unordered_map>

namespace mc::engine {

error::error() = default;

error::error(const error_info& info) : mc::enable_shared_from_this<error>(), error_info(info) {
}

error::error(std::string_view name, std::string_view format, error_level level)
    : mc::enable_shared_from_this<error>(), error_info(name, format, level) {
}

error::error(const error& other)
    : mc::enable_shared_from_this<error>(other),
      error_info(other.name, other.format, other.level) {
    this->args = other.args;

    if (other.prev_error) {
        this->prev_error.reset(new error(*other.prev_error));
    }
}

error::error(error&& other) noexcept            = default;
error& error::operator=(error&& other) noexcept = default;

error_ptr error::from_exception(std::exception_ptr e) {
    try {
        std::rethrow_exception(e);
    } catch (mc::exception& e) {
        return from_exception(e);
    } catch (std::exception& e) {
        return from_exception(e);
    } catch (...) {
        return from_exception(mc::exception());
    }
}

error_ptr error::from_exception(const mc::exception& e) {
    auto err = mc::make_shared<error>();

    err->set_name(e.name());
    auto& msgs = e.messages();
    if (!msgs.empty()) {
        auto& msg = msgs.back();
        err->set_format(msg.get_format_template());
        err->set_level(msg.get_level());
        err->set_args(msg.get_args());
    }

    return err;
}

error_ptr error::from_exception(const std::exception& e) {
    return from_exception(mc::std_exception_wrapper::from_current_exception(e));
}

void error::to_exception(mc::exception& e) const {
    e.set_name(this->name);
    e.append_log(this->to_log_message());
}

error& error::operator=(const error& other) {
    if (this != &other) {
        mc::enable_shared_from_this<error>::operator=(other);

        this->name   = other.name;
        this->format = other.format;
        this->level  = other.level;
        this->args   = other.args;

        if (other.prev_error) {
            this->prev_error.reset(new error(*other.prev_error));
        }
    }

    return *this;
}

std::string_view error::get_name() const {
    return this->name;
}

std::string_view error::get_format() const {
    return this->format;
}

const mc::dict& error::get_args() const {
    return args;
}

std::string error::get_message() const {
    if (this->format.empty()) {
        return {};
    }

    return mc::format_dict(this->format, args);
}

error_level error::get_level() const {
    return this->level;
}

void error::set_level(error_level level) {
    this->level = level;
}

void error::set_name(std::string_view name) {
    this->name = name;
}

void error::set_format(std::string_view format) {
    this->format = format;
}

void error::set_prev_error(error_ptr other) {
    this->prev_error = std::move(other);
}

void error::reset() {
    this->name   = {};
    this->format = {};
    this->args.clear();
    this->prev_error = nullptr;
}

error& error::set_args(const mc::dict& args) {
    this->args = args;
    return *this;
}

std::string error::to_string() const {
    return mc::to_string(*this);
}

bool error::is_set() const {
    if (!this->name.empty()) {
        return true;
    }

    if (this->prev_error) {
        return this->prev_error->is_set();
    }

    return false;
}

bool error::has_error(std::string_view name) const {
    if (this->name == name) {
        return true;
    }

    if (this->prev_error) {
        return this->prev_error->has_error(name);
    }

    return false;
}

bool error::operator==(const error& other) const {
    return this->name == other.name && this->format == other.format && this->args == other.args;
}

bool error::operator!=(const error& other) const {
    return !(*this == other);
}

mc::log::message error::to_log_message() const {
    return mc::log::message(
        this->level,
        mc::log::context("", std::string(this->name), 0),
        std::string(this->format),
        this->args);
}

error_with_owner::error_with_owner() {
}

error_with_owner::error_with_owner(std::string name, std::string format)
    : m_name_owner(std::move(name)), m_format_owner(std::move(format)) {
    this->name   = m_name_owner;
    this->format = m_format_owner;
}

bool get_error_format_args(std::string_view format, mc::dict& arg_names) {
    return mc::fmt::get_format_args(format, arg_names);
}

} // namespace mc::engine
