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

#include <mc/log/log_manager.h>
#include <mc/log/logger.h>

#include <algorithm>
#include <mutex>
#include <unordered_map>

namespace mc {
namespace log {

// 日志记录器实现类
class logger::impl {
public:
    logger_config             m_config;    // 日志记录器配置
    std::vector<appender_ptr> m_appenders; // 日志追加器映射

    impl(const std::string& name = MC_LOG_DEFAULT_LOGGER) : m_config(name) {
    }

    impl(const impl& other) : m_config(other.m_config), m_appenders(other.m_appenders) {
    }

    impl(impl&& other) noexcept
        : m_config(std::move(other.m_config)), m_appenders(std::move(other.m_appenders)) {
    }
};

// 静态方法实现
logger logger::get(const char* name) {
    return log_manager::instance().get_logger(name);
}

logger::logger() : m_impl(std::make_shared<impl>()) {
}

// 构造函数实现
logger::logger(const std::string& name) : m_impl(std::make_shared<impl>(name)) {
}

logger::logger(const logger& other)
    : m_impl(other.m_impl) {
}

logger::logger(logger&& other) noexcept : m_impl(std::move(other.m_impl)) {
}

// 赋值运算符实现
logger& logger::operator=(const logger& other) {
    if (this != &other) {
        m_impl = other.m_impl;
    }
    return *this;
}

logger& logger::operator=(logger&& other) noexcept {
    if (this != &other) {
        m_impl = std::move(other.m_impl);
    }
    return *this;
}

// 方法实现
logger& logger::set_level(level lvl) {
    m_impl->m_config.level = lvl;
    return *this;
}

level logger::get_level() const {
    return m_impl->m_config.level;
}

void logger::set_name(const std::string& name) {
    m_impl->m_config.name = name;
}

const std::string& logger::get_name() const {
    return m_impl->m_config.name;
}

bool logger::is_enabled(level lvl) const {
    return static_cast<int>(lvl) >= static_cast<int>(m_impl->m_config.level);
}

bool logger::is_debug_log(log_category category) const {
    return category == log_category::debug;
}

void logger::log(message msg) {
    if (is_debug_log(msg.get_category()) && !is_enabled(msg.get_level())) {
        return;
    }

    for (const auto& appender : m_impl->m_appenders) {
        appender->append(msg);
    }
}

void logger::add_appender(const appender_ptr& a) {
    if (a) {
        m_impl->m_appenders.push_back(a);
    }
}

bool logger::remove_appender(const std::string& name) {
    auto& appenders = m_impl->m_appenders;
    auto  it        = std::find_if(appenders.begin(), appenders.end(), [&name](const appender_ptr& a) {
        return a && a->get_name() == name;
    });

    if (it != appenders.end()) {
        appenders.erase(it);
        return true;
    }

    return false;
}

appender_ptr logger::find_appender(const std::string& name) const {
    const auto& appenders = m_impl->m_appenders;
    auto        it        = std::find_if(appenders.begin(), appenders.end(), [&name](const appender_ptr& a) {
        return a && a->get_name() == name;
    });

    return (it != appenders.end()) ? *it : nullptr;
}

const std::vector<appender_ptr>& logger::get_appenders() const {
    return m_impl->m_appenders;
}

void logger::clear_appenders() {
    m_impl->m_appenders.clear();
}

} // namespace log
} // namespace mc

MC_REFLECT_ENUM(mc::log::level, (all)(trace)(debug)(info)(notice)(warn)(error)(fatal)(off))
