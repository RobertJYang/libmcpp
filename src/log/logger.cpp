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

#include <algorithm>
#include <mc/log/console_appender.h>
#include <mc/log/logger.h>
#include <mutex>
#include <unordered_map>

namespace mc {
namespace log {

// 全局日志记录器管理
namespace {
std::unordered_map<std::string, logger> g_loggers;
std::mutex g_logger_mutex;

// 确保默认日志记录器存在
void ensure_default_logger() {
    std::lock_guard<std::mutex> lock(g_logger_mutex);
    if (g_loggers.find(DEFAULT_LOGGER) == g_loggers.end()) {
        logger default_logger;
        default_logger.set_name(DEFAULT_LOGGER);

        // 添加默认控制台追加器
        auto console = std::make_shared<console_appender>();
        default_logger.add_appender(console);

        g_loggers[DEFAULT_LOGGER] = default_logger;
    }
}
} // namespace

// 日志记录器实现类
class logger::impl {
public:
    logger_config m_config;                                                 // 日志记录器配置
    std::unordered_map<std::string, std::shared_ptr<appender>> m_appenders; // 日志追加器映射
    std::weak_ptr<impl> m_parent;                                           // 父日志记录器的弱引用

    impl() : m_config(DEFAULT_LOGGER) {
    }

    impl(const std::string& name, const std::weak_ptr<impl>& parent = std::weak_ptr<impl>())
        : m_config(name), m_parent(parent) {
    }

    impl(const impl& other)
        : m_config(other.m_config), m_appenders(other.m_appenders), m_parent(other.m_parent) {
    }

    impl(impl&& other) noexcept
        : m_config(std::move(other.m_config)), m_appenders(std::move(other.m_appenders)),
          m_parent(std::move(other.m_parent)) {
    }
};

// 静态方法实现
logger logger::get(const std::string& name) {
    ensure_default_logger();

    std::lock_guard<std::mutex> lock(g_logger_mutex);
    auto it = g_loggers.find(name);
    if (it != g_loggers.end()) {
        return it->second;
    }

    // 创建新的日志记录器
    logger new_logger(name, get(DEFAULT_LOGGER));
    g_loggers[name] = new_logger;
    return new_logger;
}

void logger::update(const std::string& name, logger& log) {
    std::lock_guard<std::mutex> lock(g_logger_mutex);
    g_loggers[name] = log;
}

// 构造函数实现
logger::logger() : m_impl(std::make_shared<impl>()) {
}

logger::logger(const std::string& name, const logger& parent)
    : m_impl(std::make_shared<impl>(name, parent.m_impl)) {
}

logger::logger(std::nullptr_t) : m_impl(nullptr) {
}

logger::logger(const logger& other)
    : m_impl(other.m_impl ? std::make_shared<impl>(*other.m_impl) : nullptr) {
}

logger::logger(logger&& other) noexcept : m_impl(std::move(other.m_impl)) {
}

// 赋值运算符实现
logger& logger::operator=(const logger& other) {
    if (this != &other) {
        m_impl = other.m_impl ? std::make_shared<impl>(*other.m_impl) : nullptr;
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
    if (m_impl) {
        m_impl->m_config.m_level = lvl;
    }
    return *this;
}

level logger::get_level() const {
    if (m_impl) {
        return m_impl->m_config.m_level;
    }
    return level::info;
}

logger logger::get_parent() const {
    if (m_impl) {
        auto parent_impl = m_impl->m_parent.lock();
        if (parent_impl) {
            // 创建一个新的logger对象，共享父logger的impl
            logger parent_logger;
            parent_logger.m_impl = parent_impl;
            return parent_logger;
        }
    }
    return logger(nullptr);
}

void logger::set_name(const std::string& name) {
    if (m_impl) {
        m_impl->m_config.m_name = name;
    }
}

bool logger::is_enabled(level lvl) const {
    if (m_impl) {
        return static_cast<int>(lvl) >= static_cast<int>(m_impl->m_config.m_level);
    }
    return false;
}

void logger::log(message msg) {
    if (!m_impl || !is_enabled(msg.m_level)) {
        return;
    }
    
    // 分发日志消息到所有追加器
    for (const auto& appender_pair : m_impl->m_appenders) {
        appender_pair.second->append(msg);
    }
    
    // 如果有父日志记录器，也分发给父日志记录器
    auto parent_impl = m_impl->m_parent.lock();
    if (parent_impl) {
        // 创建一个临时的logger对象，共享父logger的impl
        logger parent_logger;
        parent_logger.m_impl = parent_impl;
        parent_logger.log(std::move(msg));
    }
}

void logger::add_appender(const std::shared_ptr<appender>& a) {
    if (m_impl && a) {
        m_impl->m_appenders[a->name()] = a;
    }
}

void logger::remove_appender(const std::string& name) {
    if (m_impl) {
        m_impl->m_appenders.erase(name);
    }
}

std::shared_ptr<appender> logger::get_appender(const std::string& name) const {
    if (m_impl) {
        auto it = m_impl->m_appenders.find(name);
        if (it != m_impl->m_appenders.end()) {
            return it->second;
        }
    }
    return nullptr;
}

std::vector<std::shared_ptr<appender>> logger::get_appenders() const {
    std::vector<std::shared_ptr<appender>> result;
    if (m_impl) {
        result.reserve(m_impl->m_appenders.size());
        for (const auto& pair : m_impl->m_appenders) {
            result.push_back(pair.second);
        }
    }
    return result;
}

} // namespace log
} // namespace mc