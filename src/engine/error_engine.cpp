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

#include <mc/engine/error_engine.h>
#include <mc/engine/errors/std_errors.h>
#include <mc/engine/utils.h>
#include <mc/log.h>
#include <mc/string.h>

#include <string_view>
#include <unordered_map>

namespace mc::engine {

error_engine& error_engine::get_instance() {
    return mc::singleton<error_engine>::instance();
}

struct error_info_data {
    error_info_data(std::string name, std::string format, error_level level)
        : name(std::move(name)), format(std::move(format)), level(level) {
    }

    std::string name;
    std::string format;
    error_level level;
};

// TODO:: 后续应该放到共享内存中全局有效
struct error_engine::error_engine_impl {
    using error_infos = std::list<error_info_data>; // 非常量的错误名称和格式化字符串持有在这里
    using error_map   = std::unordered_map<std::string_view, error_info>;

    std::mutex  m_mutex;
    error_infos m_infos;
    error_map   m_errors;

    static thread_local error s_last_error;
};

thread_local error error_engine::error_engine_impl::s_last_error;

error_engine::error_engine() : m_impl(std::make_unique<error_engine_impl>()) {
}

error_engine::~error_engine() {
}

error_info error_engine::register_const_error(const error_info& info) {
    return register_const_error(info.name, info.format, info.level);
}

error_info error_engine::register_const_error(std::string_view name, std::string_view format,
                                              error_level level) {
    std::lock_guard lock(m_impl->m_mutex);

    if (m_impl->m_errors.find(name) != m_impl->m_errors.end()) {
        wlog("register error failed, error name ${name} already exists", ("name", name));
        return error_info();
    }

    auto ret = m_impl->m_errors.emplace(name, error_info(name, format, level));
    return ret.second ? ret.first->second : error_info();
}

error_info error_engine::register_error(std::string name, std::string format, error_level level) {
    std::lock_guard lock(m_impl->m_mutex);

    if (m_impl->m_errors.find(name) != m_impl->m_errors.end()) {
        wlog("register error failed, error name ${name} already exists", ("name", name));
        return error_info();
    }

    auto& info = m_impl->m_infos.emplace_back(std::move(name), std::move(format), level);
    auto  ret = m_impl->m_errors.emplace(info.name, error_info(info.name, info.format, info.level));
    return ret.second ? ret.first->second : error_info();
}

error_info error_engine::get_error_info(std::string_view name) {
    std::lock_guard lock(m_impl->m_mutex);

    auto it = m_impl->m_errors.find(name);
    if (it == m_impl->m_errors.end()) {
        return {};
    }

    return it->second;
}

const error& error_engine::report_error(std::string_view name, mc::dict args) {
    std::string_view format;
    error_level      level;
    {
        std::lock_guard lock(m_impl->m_mutex);

        auto it = m_impl->m_errors.find(name);
        MC_ASSERT(it != m_impl->m_errors.end(), "error name ${name} not registered",
                  ("name", name));
        format = it->second.format;
        level  = it->second.level;
    }

    return report_error(error_info(name, format, level), std::move(args));
}

const error& error_engine::report_error(const error_info& info, mc::dict args) {
    auto& last_error = m_impl->s_last_error;

    if (last_error.is_set()) {
        error new_error(info);
        new_error.set_args(std::move(args));
        new_error.set_prev_error(std::move(last_error));
        last_error = std::move(new_error);
    } else {
        last_error.set_name(info.name);
        last_error.set_format(info.format);
        last_error.set_args(std::move(args));
        last_error.set_level(info.level);
    }

    return last_error;
}

error error_engine::set_last_error(error new_error) {
    auto& err        = m_impl->s_last_error;
    auto  prev_error = std::move(err);
    err              = std::move(new_error);
    return prev_error;
}

void error_engine::reset_error() {
    m_impl->s_last_error.reset();
}

const error& error_engine::last_error() {
    return m_impl->s_last_error;
}

bool error_engine::is_registered(std::string_view name) {
    std::lock_guard lock(m_impl->m_mutex);

    return m_impl->m_errors.find(name) != m_impl->m_errors.end();
}

error make_error(std::string_view name, std::string_view format) {
    MC_ASSERT(is_valid_error_name(name), "error name ${name} is invalid", ("name", name));

    return error(name, format);
}

error make_error(const error_info& info) {
    return make_error(info.name, info.format);
}

bool is_valid_error_name(std::string_view name) {
    return detail::is_valid_interface_name(name);
}

} // namespace mc::engine
