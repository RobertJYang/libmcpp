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
#include <mc/json.h>
#include <mc/log.h>
#include <mc/string.h>

#include <string_view>
#include <unordered_map>

namespace mc::engine {

error::error(const error& other) : error_info(other.name, other.format, other.level) {
    this->args = other.args;

    if (other.prev_error) {
        this->prev_error.reset(new error(*other.prev_error));
    }
}

error& error::operator=(const error& other) {
    if (this != &other) {
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

    return mc::string::format(this->format, args);
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

void error::set_prev_error(error other) {
    this->prev_error.reset(new error(std::move(other)));
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

error_with_owner::error_with_owner() {
}

error_with_owner::error_with_owner(std::string name, std::string format)
    : m_name_owner(std::move(name)), m_format_owner(std::move(format)) {
    this->name   = m_name_owner;
    this->format = m_format_owner;
}

} // namespace mc::engine
