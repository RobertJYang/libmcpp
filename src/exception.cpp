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

#include <mc/exception.h>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <ctime>

namespace mc {

namespace detail {

/**
 * @brief 异常实现类
 * 
 * 存储异常的详细信息
 */
class exception_impl {
public:
    std::string     m_name;       // 异常名称
    std::string     m_what;       // 异常描述
    int64_t         m_code;       // 异常代码
    log_messages    m_logs;       // 日志消息列表
};

} // namespace detail

// 异常基类实现

exception::exception(int64_t code, const std::string& name_value, const std::string& what_value)
    : m_impl(new detail::exception_impl())
{
    m_impl->m_name = name_value;
    m_impl->m_what = what_value;
    m_impl->m_code = code;
}

exception::exception(log_message&& msg, int64_t code, const std::string& name_value, const std::string& what_value)
    : exception(code, name_value, what_value)
{
    m_impl->m_logs.emplace_back(std::move(msg));
}

exception::exception(log_messages&& msgs, int64_t code, const std::string& name_value, const std::string& what_value)
    : exception(code, name_value, what_value)
{
    m_impl->m_logs = std::move(msgs);
}

exception::exception(const log_messages& msgs, int64_t code, const std::string& name_value, const std::string& what_value)
    : exception(code, name_value, what_value)
{
    m_impl->m_logs = msgs;
}

exception::exception(const exception& e)
    : m_impl(new detail::exception_impl(*e.m_impl))
{
}

exception::exception(exception&& e)
    : m_impl(std::move(e.m_impl))
{
}

exception::~exception()
{
}

int64_t exception::code() const noexcept
{
    return m_impl->m_code;
}

const char* exception::name() const noexcept
{
    return m_impl->m_name.c_str();
}

const char* exception::what() const noexcept
{
    return m_impl->m_what.c_str();
}

void exception::append_log(log_message msg)
{
    m_impl->m_logs.emplace_back(std::move(msg));
}

std::string exception::to_detail_string(log_level ll) const
{
    std::stringstream ss;
    ss << m_impl->m_code << " " << m_impl->m_name << ": " << m_impl->m_what << "\n";
    
    for (const auto& log : m_impl->m_logs) {
        // 根据日志级别过滤
        if (static_cast<int>(log.m_level) < static_cast<int>(ll)) {
            continue;
        }
        
        // 格式化时间戳
        auto time_t = std::chrono::system_clock::to_time_t(log.m_timestamp);
        auto tm = std::localtime(&time_t);
        
        ss << std::put_time(tm, "%Y-%m-%d %H:%M:%S") << " ";
        
        // 添加日志级别
        switch (log.m_level) {
            case log_level::debug: ss << "DEBUG "; break;
            case log_level::info:  ss << "INFO  "; break;
            case log_level::warn:  ss << "WARN  "; break;
            case log_level::error: ss << "ERROR "; break;
            default:               ss << "      "; break;
        }
        
        // 添加文件和行号信息
        if (!log.m_file.empty()) {
            ss << log.m_file << ":" << log.m_line << " ";
        }
        
        // 添加日志消息
        ss << log.m_message << "\n";
    }
    
    return ss.str();
}

std::string exception::to_string(log_level ll) const
{
    std::stringstream ss;
    ss << m_impl->m_name << ": " << m_impl->m_what;
    
    // 只显示最后一条日志消息
    if (!m_impl->m_logs.empty()) {
        const auto& log = m_impl->m_logs.back();
        if (static_cast<int>(log.m_level) >= static_cast<int>(ll)) {
            ss << " (" << log.m_message;
            
            // 添加文件名和行号信息
            if (!log.m_file.empty()) {
                ss << " at " << log.m_file;
                if (log.m_line > 0) {
                    ss << ":" << log.m_line;
                }
            }
            
            ss << ")";
        }
    }
    
    return ss.str();
}

std::string exception::top_message() const
{
    if (m_impl->m_logs.empty()) {
        return m_impl->m_what;
    }
    
    return m_impl->m_logs.back().m_message;
}

void exception::dynamic_rethrow_exception() const
{
    throw *this;
}

std::shared_ptr<exception> exception::dynamic_copy_exception() const
{
    return std::make_shared<exception>(*this);
}

// 未处理异常实现

unhandled_exception::unhandled_exception(log_message&& msg, std::exception_ptr e)
    : exception(std::move(msg), external_error_code, "unhandled", "未处理的异常"),
      m_inner(e)
{
}

unhandled_exception::unhandled_exception(log_messages msgs)
    : exception(std::move(msgs), external_error_code, "unhandled", "未处理的异常")
{
}

unhandled_exception::unhandled_exception(const exception& e)
    : exception(e)
{
    m_impl->m_code = external_error_code;
    m_impl->m_name = "unhandled";
    m_impl->m_what = "未处理的异常";
}

std::exception_ptr unhandled_exception::get_inner_exception() const
{
    return m_inner;
}

void unhandled_exception::dynamic_rethrow_exception() const
{
    if (m_inner) {
        std::rethrow_exception(m_inner);
    }
    throw *this;
}

std::shared_ptr<exception> unhandled_exception::dynamic_copy_exception() const
{
    return std::make_shared<unhandled_exception>(*this);
}

// 标准异常包装类实现

std_exception_wrapper::std_exception_wrapper(
    log_message&& msg,
    std::exception_ptr e,
    const std::string& name_value,
    const std::string& what_value)
    : exception(std::move(msg), system_error_code, name_value, what_value),
      m_inner(e)
{
}

std::exception_ptr std_exception_wrapper::get_inner_exception() const
{
    return m_inner;
}

std_exception_wrapper std_exception_wrapper::from_current_exception(const std::exception& e)
{
    return std_exception_wrapper(
        log_message(log_level::error, e.what()),
        std::current_exception(),
        "std_exception",
        e.what()
    );
}

void std_exception_wrapper::dynamic_rethrow_exception() const
{
    if (m_inner) {
        std::rethrow_exception(m_inner);
    }
    throw *this;
}

std::shared_ptr<exception> std_exception_wrapper::dynamic_copy_exception() const
{
    return std::make_shared<std_exception_wrapper>(*this);
}

std::string std_exception_wrapper::to_detail_string(log_level ll) const
{
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

// 异常工厂实现

exception_factory& exception_factory::instance()
{
    static exception_factory instance;
    return instance;
}

void exception_factory::rethrow(const exception& e) const
{
    auto itr = m_registered_exceptions.find(e.code());
    if (itr != m_registered_exceptions.end()) {
        itr->second->rethrow(e);
    }
    throw e;
}

// 辅助函数实现

std::string except_str()
{
    try {
        throw;
    } catch (const exception& e) {
        return e.to_string();
    } catch (const std::exception& e) {
        return std::string("标准异常: ") + e.what();
    } catch (...) {
        return "未知异常";
    }
}

void record_assert_trip(const char* filename, uint32_t lineno, const char* expr)
{
    std::cerr << "断言失败: " << expr << " 在 " << filename << ":" << lineno << std::endl;
}

// 常用异常类实现

// 超时异常
timeout_exception::timeout_exception(log_message&& msg)
    : exception(std::move(msg), time_limit_error_code, "timeout", "操作超时")
{
}

timeout_exception::timeout_exception(const timeout_exception& e)
    : exception(e)
{
}

timeout_exception::timeout_exception(timeout_exception&& e)
    : exception(std::move(e))
{
}

// 从基类构造的构造函数实现
timeout_exception::timeout_exception(const exception& e)
    : exception(e)
{
    m_impl->m_code = time_limit_error_code;
    m_impl->m_name = "timeout";
    m_impl->m_what = "操作超时";
}

std::shared_ptr<exception> timeout_exception::dynamic_copy_exception() const
{
    return std::make_shared<timeout_exception>(*this);
}

void timeout_exception::dynamic_rethrow_exception() const
{
    throw *this;
}

// 文件未找到异常
file_not_found_exception::file_not_found_exception(log_message&& msg)
    : exception(std::move(msg), missing_file_error_code, "file_not_found", "文件未找到")
{
}

file_not_found_exception::file_not_found_exception(const file_not_found_exception& e)
    : exception(e)
{
}

file_not_found_exception::file_not_found_exception(file_not_found_exception&& e)
    : exception(std::move(e))
{
}

// 从基类构造的构造函数实现
file_not_found_exception::file_not_found_exception(const exception& e)
    : exception(e)
{
    m_impl->m_code = missing_file_error_code;
    m_impl->m_name = "file_not_found";
    m_impl->m_what = "文件未找到";
}

std::shared_ptr<exception> file_not_found_exception::dynamic_copy_exception() const
{
    return std::make_shared<file_not_found_exception>(*this);
}

void file_not_found_exception::dynamic_rethrow_exception() const
{
    throw *this;
}

// 解析错误异常
parse_error_exception::parse_error_exception(log_message&& msg)
    : exception(std::move(msg), syntax_error_code, "parse_error", "解析错误")
{
}

parse_error_exception::parse_error_exception(const parse_error_exception& e)
    : exception(e)
{
}

parse_error_exception::parse_error_exception(parse_error_exception&& e)
    : exception(std::move(e))
{
}

// 从基类构造的构造函数实现
parse_error_exception::parse_error_exception(const exception& e)
    : exception(e)
{
    m_impl->m_code = syntax_error_code;
    m_impl->m_name = "parse_error";
    m_impl->m_what = "解析错误";
}

std::shared_ptr<exception> parse_error_exception::dynamic_copy_exception() const
{
    return std::make_shared<parse_error_exception>(*this);
}

void parse_error_exception::dynamic_rethrow_exception() const
{
    throw *this;
}

// 无效参数异常
invalid_arg_exception::invalid_arg_exception(log_message&& msg)
    : exception(std::move(msg), argument_error_code, "invalid_arg", "无效参数")
{
}

invalid_arg_exception::invalid_arg_exception(const invalid_arg_exception& e)
    : exception(e)
{
}

invalid_arg_exception::invalid_arg_exception(invalid_arg_exception&& e)
    : exception(std::move(e))
{
}

// 从基类构造的构造函数实现
invalid_arg_exception::invalid_arg_exception(const exception& e)
    : exception(e)
{
    m_impl->m_code = argument_error_code;
    m_impl->m_name = "invalid_arg";
    m_impl->m_what = "无效参数";
}

std::shared_ptr<exception> invalid_arg_exception::dynamic_copy_exception() const
{
    return std::make_shared<invalid_arg_exception>(*this);
}

void invalid_arg_exception::dynamic_rethrow_exception() const
{
    throw *this;
}

// 键未找到异常
key_not_found_exception::key_not_found_exception(log_message&& msg)
    : exception(std::move(msg), missing_key_error_code, "key_not_found", "键未找到")
{
}

key_not_found_exception::key_not_found_exception(const key_not_found_exception& e)
    : exception(e)
{
}

key_not_found_exception::key_not_found_exception(key_not_found_exception&& e)
    : exception(std::move(e))
{
}

// 从基类构造的构造函数实现
key_not_found_exception::key_not_found_exception(const exception& e)
    : exception(e)
{
    m_impl->m_code = missing_key_error_code;
    m_impl->m_name = "key_not_found";
    m_impl->m_what = "键未找到";
}

std::shared_ptr<exception> key_not_found_exception::dynamic_copy_exception() const
{
    return std::make_shared<key_not_found_exception>(*this);
}

void key_not_found_exception::dynamic_rethrow_exception() const
{
    throw *this;
}

// 断言异常
assert_exception::assert_exception(log_message&& msg)
    : exception(std::move(msg), condition_error_code, "assert", "断言失败")
{
}

assert_exception::assert_exception(const assert_exception& e)
    : exception(e)
{
}

assert_exception::assert_exception(assert_exception&& e)
    : exception(std::move(e))
{
}

// 从基类构造的构造函数实现
assert_exception::assert_exception(const exception& e)
    : exception(e)
{
    m_impl->m_code = condition_error_code;
    m_impl->m_name = "assert";
    m_impl->m_what = "断言失败";
}

std::shared_ptr<exception> assert_exception::dynamic_copy_exception() const
{
    return std::make_shared<assert_exception>(*this);
}

void assert_exception::dynamic_rethrow_exception() const
{
    throw *this;
}

} // namespace mc 