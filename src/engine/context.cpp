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

#include "mc/engine/context.h"
#include "mc/engine/error_engine.h"

namespace mc::engine {

context::context(object_base* object) : m_object(object) {
}

context::~context() {
}

mc::engine::error& context::get_error() {
    return m_error;
}

bool context::has_error() const {
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

object_base* context::get_object() const {
    return m_object;
}

call_info& context::get_call_info() {
    return m_call_info;
}

void context::set_call_info(call_info call_info) {
    m_call_info = std::move(call_info);
}

} // namespace mc::engine
