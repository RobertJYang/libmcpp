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

#include <ctime>
#include <iomanip>
#include <iostream>
#include <mc/exception.h>
#include <sstream>

namespace mc {

namespace detail {

/**
 * @brief 异常实现类
 *
 * 存储异常的详细信息
 */
class exception_impl {
public:
    std::string   m_name; // 异常名称
    std::string   m_what; // 异常描述
    int64_t       m_code; // 异常代码
    log::messages m_logs; // 日志消息列表
};

} // namespace detail

// 异常基类实现

exception::exception(int64_t code, const std::string& name_value, const std::string& what_value)
    : m_impl(new detail::exception_impl()) {
    m_impl->m_name = name_value;
    m_impl->m_what = what_value;
    m_impl->m_code = code;
}

exception::exception(mc::log::message&& msg, int64_t code, const std::string& name_value,
                     const std::string& what_value)
    : exception(code, name_value, what_value) {
    m_impl->m_logs.emplace_back(std::move(msg));
}

exception::exception(mc::log::messages&& msgs, int64_t code, const std::string& name_value,
                     const std::string& what_value)
    : exception(code, name_value, what_value) {
    m_impl->m_logs = std::move(msgs);
}

exception::exception(const mc::log::messages& msgs, int64_t code, const std::string& name_value,
                     const std::string& what_value)
    : exception(code, name_value, what_value) {
    m_impl->m_logs = msgs;
}

exception::exception(const exception& e) : m_impl(new detail::exception_impl(*e.m_impl)) {
}

exception::exception(exception&& e) : m_impl(std::move(e.m_impl)) {
}

exception::~exception() {
}

int64_t exception::code() const noexcept {
    return m_impl->m_code;
}

void exception::set_code(int64_t code) {
    m_impl->m_code = code;
}

std::string_view exception::name() const noexcept {
    return m_impl->m_name;
}

void exception::set_name(std::string_view name) {
    m_impl->m_name = name;
}

const char* exception::what() const noexcept {
    return top_message().c_str();
}

void exception::append_log(mc::log::message msg) const {
    detail::exception_impl& impl = *const_cast<exception*>(this)->m_impl;
    impl.m_logs.emplace_back(std::move(msg));
}

void exception::append_log(mc::log::messages msgs) const {
    if (msgs.empty()) {
        return;
    }

    detail::exception_impl& impl = *const_cast<exception*>(this)->m_impl;
    for (const auto& msg : msgs) {
        impl.m_logs.emplace_back(std::move(msg));
    }
}

std::string exception::to_detail_string(mc::log::level ll) const {
    std::stringstream ss;
    ss << m_impl->m_code << " " << m_impl->m_name << ": " << m_impl->m_what << "\n";

    for (const auto& log : m_impl->m_logs) {
        // 根据日志级别过滤
        if (static_cast<int>(log.get_level()) < static_cast<int>(ll)) {
            continue;
        }

        // 格式化时间戳
        auto time_t = std::chrono::system_clock::to_time_t(log.get_timestamp());
        auto tm     = std::localtime(&time_t);

        ss << std::put_time(tm, "%Y-%m-%d %H:%M:%S") << " ";

        // 添加线程ID
        ss << "[" << log.get_thread_id() << "] ";

        // 添加日志级别
        switch (log.get_level()) {
        case mc::log::level::debug:
            ss << "DEBUG ";
            break;
        case mc::log::level::info:
            ss << "INFO  ";
            break;
        case mc::log::level::warn:
            ss << "WARN  ";
            break;
        case mc::log::level::error:
            ss << "ERROR ";
            break;
        case mc::log::level::notice:
            ss << "NOTICE ";
            break;
        default:
            ss << "      ";
            break;
        }

        // 添加文件和行号信息
        auto ctx = log.get_context();
        if (!ctx.m_file.empty()) {
            ss << ctx.m_file << ":" << ctx.m_line << " ";
        }

        // 添加日志消息
        ss << log.get_message() << "\n";
    }

    return ss.str();
}

std::string exception::to_string(mc::log::level ll) const {
    std::stringstream ss;
    ss << m_impl->m_name << ": " << m_impl->m_what;

    // 只显示最后一条日志消息
    if (!m_impl->m_logs.empty()) {
        const auto& log = m_impl->m_logs.back();
        if (static_cast<int>(log.get_level()) >= static_cast<int>(ll)) {
            ss << " (" << log.get_message();

            // 添加文件名和行号信息
            auto ctx = log.get_context();
            if (!ctx.m_file.empty()) {
                ss << " at " << ctx.m_file;
                if (ctx.m_line > 0) {
                    ss << ":" << ctx.m_line;
                }
            }

            ss << ")";
        }
    }

    return ss.str();
}

const std::string& exception::top_message() const {
    if (m_impl->m_logs.empty()) {
        return m_impl->m_what;
    }

    return m_impl->m_logs.back().get_message();
}

void exception::dynamic_rethrow_exception() const {
    throw *this;
}

std::shared_ptr<exception> exception::dynamic_copy_exception() const {
    return std::make_shared<exception>(*this);
}

const mc::log::messages& exception::messages() const {
    return m_impl->m_logs;
}

mc::log::messages exception::take_messages() const {
    return std::move(m_impl->m_logs);
}

// 未处理异常实现

unhandled_exception::unhandled_exception(mc::log::message&& msg, std::exception_ptr e)
    : exception(std::move(msg), unhandled_exception_code, "unhandled", "未处理的异常"), m_inner(e) {
}

unhandled_exception::unhandled_exception(mc::log::messages msgs)
    : exception(std::move(msgs), unhandled_exception_code, "unhandled", "未处理的异常") {
}

unhandled_exception::unhandled_exception(const exception& e) : exception(e) {
    m_impl->m_code = unhandled_exception_code;
    m_impl->m_name = "unhandled";
    m_impl->m_what = "未处理的异常";
}

std::exception_ptr unhandled_exception::get_inner_exception() const {
    return m_inner;
}

void unhandled_exception::dynamic_rethrow_exception() const {
    if (m_inner) {
        std::rethrow_exception(m_inner);
    }
    throw *this;
}

std::shared_ptr<exception> unhandled_exception::dynamic_copy_exception() const {
    return std::make_shared<unhandled_exception>(*this);
}

// 标准异常包装类实现

std_exception_wrapper::std_exception_wrapper(mc::log::message&& msg, std::exception_ptr e,
                                             const std::string& name_value,
                                             const std::string& what_value)
    : exception(std::move(msg), std_exception_code, name_value, what_value), m_inner(e) {
}

std::exception_ptr std_exception_wrapper::get_inner_exception() const {
    return m_inner;
}

std_exception_wrapper std_exception_wrapper::from_current_exception(const std::exception& e) {
    return std_exception_wrapper(mc::log::message(mc::log::level::error, e.what()),
                                 std::current_exception(), "std_exception", e.what());
}

void std_exception_wrapper::dynamic_rethrow_exception() const {
    if (m_inner) {
        std::rethrow_exception(m_inner);
    }
    throw *this;
}

std::shared_ptr<exception> std_exception_wrapper::dynamic_copy_exception() const {
    return std::make_shared<std_exception_wrapper>(*this);
}

std::string std_exception_wrapper::to_detail_string(mc::log::level ll) const {
    std::string result = exception::to_detail_string(ll);

    // 添加内部异常信息
    if (m_inner) {
        try {
            std::rethrow_exception(m_inner);
        } catch (const std::exception& e) {
            result += "内部异常: " + std::string(e.what()) + "\n";
        } catch (...) {
            result += "内部异常: 未知类型\n";
        }
    }

    return result;
}

MC_STD_EXCEPTION_CLASS(MC_IMPLEMENT_EXCEPTION_CLASS)

} // namespace mc