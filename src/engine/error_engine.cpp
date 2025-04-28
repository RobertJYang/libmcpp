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
#include <mc/engine/errors.h>
#include <mc/engine/utils.h>
#include <mc/log.h>
#include <mc/string.h>

#include <string_view>
#include <unordered_map>

namespace mc::engine {

error_engine& error_engine::get_instance() {
    return mc::singleton<error_engine>::instance();
}

// TODO:: 后续应该放到共享内存中全局有效
struct error_engine::error_engine_impl {
    using error_names = std::list<std::string>;
    using error_map   = std::unordered_map<std::string_view, std::string>;

    // c++17 不支持透明查询，而我们希望使用 std::string_view 作为查询条件，所以只能将
    // 错误名称单独存储在 error_names 中，error_map 使用存储的名字引用构造 std::string_view
    // 作为 key
    // 注意：这里必须保证 error_names 容器不能移动已经构造的名字对象，否则会导致 error_map 中的 key
    // 失效
    error_names m_names;
    error_map   m_errors;

    std::mutex m_mutex;
};

#define REGISTER_ERROR(info) register_const_error(info);

error_engine::error_engine() : m_impl(std::make_unique<error_engine_impl>()) {
    // 注册标准错误
    REGISTER_ERRORS(REGISTER_ERROR)
}

error_engine::~error_engine() {
}

void error_engine::register_const_error(const error_info& info) {
    register_const_error(info.name, info.format);
}

void error_engine::register_const_error(std::string_view name, std::string_view format) {
    std::lock_guard lock(m_impl->m_mutex);

    if (m_impl->m_errors.find(name) != m_impl->m_errors.end()) {
        wlog("注册错误失败，错误名称 ${name} 已存在", ("name", name));
        return;
    }

    m_impl->m_errors.emplace(name, std::move(format));
}

void error_engine::register_error(std::string name, std::string format) {
    std::lock_guard lock(m_impl->m_mutex);

    if (m_impl->m_errors.find(name) != m_impl->m_errors.end()) {
        wlog("注册错误失败，错误名称 ${name} 已存在", ("name", name));
        return;
    }

    // name 移动到 error_names 中
    auto& name_ref = m_impl->m_names.emplace_back(std::move(name));

    // 再使用 name_ref 作为 key
    m_impl->m_errors.emplace(name_ref, std::move(format));
}

std::string_view error_engine::get_error_format(std::string_view name) {
    std::lock_guard lock(m_impl->m_mutex);

    auto it = m_impl->m_errors.find(name);
    if (it == m_impl->m_errors.end()) {
        return {};
    }

    return it->second;
}

static thread_local error m_last_error;

error& error_engine::report_error(std::string_view name) {
    std::lock_guard lock(m_impl->m_mutex);

    auto it = m_impl->m_errors.find(name);
    MC_ASSERT(it != m_impl->m_errors.end(), "错误名称 ${name} 未注册", ("name", name));

    m_last_error.set_name(it->first);
    m_last_error.set_format(it->second);
    return m_last_error;
}

void error_engine::set_last_error(const error& error) {
    m_last_error = error;
}

void error_engine::reset_error() {
    m_last_error.reset();
}

error& error_engine::last_error() {
    return m_last_error;
}

error& error_engine::report_error(const error_info& info) {
    return report_error(info.name);
}

bool error_engine::is_registered(std::string_view name) {
    std::lock_guard lock(m_impl->m_mutex);

    return m_impl->m_errors.find(name) != m_impl->m_errors.end();
}

error error_engine::make_error(std::string_view name, std::string_view format) {
    MC_ASSERT(is_valid_error_name(name), "错误名称 ${name} 无效", ("name", name));

    return error(name, format);
}

error error_engine::make_error(const error_info& info) {
    return error(info.name, info.format);
}

constexpr std::string_view PLACEHOLDER_START = mc::string::PLACEHOLDER_START;
constexpr char             PLACEHOLDER_END   = mc::string::PLACEHOLDER_END;

bool error_engine::get_format_args(std::string_view format, mc::dict& arg_names, error& error) {
    if (format.empty()) {
        return true;
    }

    std::size_t      pos = 0;
    mc::mutable_dict args;
    while (pos < format.size()) {
        size_t start_pos = format.find(PLACEHOLDER_START, pos);
        if (start_pos == std::string_view::npos) {
            break;
        }

        size_t end_pos = format.find(PLACEHOLDER_END, start_pos + PLACEHOLDER_START.size());
        if (end_pos == std::string_view::npos) {
            dlog("错误格式 ${format} 缺少结束括号，位置 ${pos}",
                 ("format", format)("pos", start_pos));
            return false;
        }

        std::string_view key = format.substr(start_pos + PLACEHOLDER_START.size(),
                                             end_pos - (start_pos + PLACEHOLDER_START.size()));
        if (key.empty()) {
            dlog("错误格式 ${format} 缺少占位符，位置 ${pos}",
                 ("format", format)("pos", start_pos));
            return false;
        }

        args[key] = true;

        pos = end_pos + 1;
    }

    arg_names = std::move(args);
    return true;
}

bool error_engine::is_valid_error_name(std::string_view name) {
    return detail::is_valid_interface_name(name);
}

} // namespace mc::engine
