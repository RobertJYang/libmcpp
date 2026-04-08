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

#include <cerrno>
#include <cstring>
#include <dlfcn.h>
#include <mc/engine/context.h>
#include <mc/engine/service.h>
#include <mc/filesystem.h>
#include <mc/log/appenders/file_appender.h>
#include <mc/log/log_level.h>
#include <ostream>
#include <stdarg.h>
#include <stdio.h>
#include <string>
#include <sys/stat.h>
#include <syslog.h>
#include <unistd.h>

#if __has_include(<logging_internal.h>)
// 优先使用依赖库 liblogger 提供的头
#include <logging_internal.h>
#else
// fallback：工程内 stub/测试用实现（native/UT 环境）
#include <log/logging_internal.h>
#endif

#if __has_include(<logging.h>)
#include <logging.h>
#else
typedef enum { DLOG_ERROR, DLOG_WARN, DLOG_NOTICE, DLOG_INFO, DLOG_DEBUG } DLOG_LEVEL_E;
#endif

typedef void (*debug_log_func_t)(DLOG_LEVEL_E, const char*, int, const char*, ...);
typedef void (*set_log_module_name_func_t)(const char*);
typedef const char* (*get_log_time_str_func_t)(int);
typedef DLOG_LEVEL_E (*set_log_level_func_t)(DLOG_LEVEL_E);
static debug_log_func_t           debug_log_ptr           = nullptr;
static set_log_module_name_func_t set_log_module_name_ptr = nullptr;
static get_log_time_str_func_t    get_log_time_str_ptr    = nullptr;
static set_log_level_func_t       set_log_level_ptr       = nullptr;
static std::string                g_module_name{"Unknown"}; // 全局模块名称，所有 file_appender 实例共享
constexpr uint32_t                LOG_US_TIME = 0x01;

// 将 mc::log::level 映射到 DLOG_LEVEL_E
// 对于没有对应级别的（all, trace, fatal, off），返回 DLOG_DEBUG 作为默认值
static DLOG_LEVEL_E log_level_to_dlog_level(mc::log::level lvl)
{
    switch (lvl) {
        case mc::log::level::error:
            return DLOG_ERROR;
        case mc::log::level::warn:
            return DLOG_WARN;
        case mc::log::level::notice:
            return DLOG_NOTICE;
        case mc::log::level::info:
            return DLOG_INFO;
        case mc::log::level::debug:
            return DLOG_DEBUG;
        default:
            // all, trace, fatal, off 没有对应级别，返回默认值
            return DLOG_DEBUG;
    }
}

namespace mc {
namespace log {

file_appender::file_appender()
{}

bool file_appender::init(const variant& args)
{
    if (!args.is_object()) {
        return false;
    }
    // 只在 LOGGING_PATH 非空且符号指针未初始化时尝试加载
    if (LOGGING_PATH[0] != '\0' && debug_log_ptr == nullptr) {
        void* handle = dlopen(LOGGING_PATH, RTLD_NOW);
        if (!handle) {
            // TODO:: 不打印错误了，干扰单元测试，后续 file_appender 也不应该是默认加载的
            // 应该在程序启动的时候配置
            // fprintf(stderr, "dlopen failed: %s\n", dlerror());
        } else {
            debug_log_ptr = (debug_log_func_t)dlsym(handle, "debug_log");
            if (!debug_log_ptr) {
                fprintf(stderr, "dlsym debug_log failed: %s\n", dlerror());
            }
            set_log_module_name_ptr = (set_log_module_name_func_t)dlsym(handle, "set_log_module_name");
            if (!set_log_module_name_ptr) {
                fprintf(stderr, "dlsym set_log_module_name failed: %s\n", dlerror());
            }
            get_log_time_str_ptr = (get_log_time_str_func_t)dlsym(handle, "get_log_time_str_c");
            if (!get_log_time_str_ptr) {
                fprintf(stderr, "dlsym get_log_time_str failed: %s\n", dlerror());
            }
            set_log_level_ptr = (set_log_level_func_t)dlsym(handle, "set_debug_log_level");
            if (!set_log_level_ptr) {
                fprintf(stderr, "dlsym set_debug_log_level failed: %s\n", dlerror());
            }
        }
    }

    // 从配置中获取模块名称并设置
    auto dict = args.as<mc::dict>();
    if (dict.contains("module_name")) {
        std::string module_name = dict["module_name"].as<std::string>();
        g_module_name           = module_name; // 存储到全局变量
        if (set_log_module_name_ptr) {
            set_log_module_name_ptr(module_name.c_str());
        }
    }
    if (dict.contains("truncate") && dict["truncate"].is_bool()) {
        m_file_config.truncate = dict["truncate"].as<bool>();
    }
    if (dict.contains("flush_on_write") && dict["flush_on_write"].is_bool()) {
        m_file_config.flush_on_write = dict["flush_on_write"].as<bool>();
    }
    open_file();
    return true;
}

file_appender::~file_appender()
{
    close_file();
}

// fallback_out：stub 环境下无 debug_log_ptr 时写入此流，供测试读文件断言
void append_debug(const message& msg, std::ostream* fallback_out)
{
    // 获取上下文信息
    const context& ctx = msg.get_context();

    DLOG_LEVEL_E level = log_level_to_dlog_level(msg.get_level());

    // 获取模块名：优先使用消息参数中的 module_name（Lua 接口第一个参数传入）
    // 注意：debug_log_ptr 由外部 liblogger 输出模块名，这里在输出前设置模块名
    std::string module_name = g_module_name;
    const auto& args        = msg.get_args();
    if (args.contains("module_name")) {
        try {
            module_name = args["module_name"].as<std::string>();
        } catch (...) {
            // 转换失败时使用默认模块名
        }
    }

    std::string file_str;
    file_str.reserve(64); // 预分配足够空间
    if (ctx.m_file.empty()) {
        file_str.append("unknown");
    } else {
        file_str.append(mc::filesystem::basename(ctx.m_file));
    }

    std::string message_str = msg.get_message();
    // 过滤无效字符，避免写入包含控制字符的内容
    logging::filter_invalid_chars(message_str);

    logging::LogRecord record = {}; // 使用空花括号进行零初始化，避免编译器警告
    record.lineno             = ctx.m_line;
    record.module_name        = module_name;
    record.level              = level;
    record.is_skynet          = 0;
    record.log                = message_str;
    record.filename           = file_str.c_str();

    // 第二个参数：true 限流，false 不限流（_easy）
    logging::internal_log_handler(&record, msg.get_limit());

    // 有文件流时始终写入，便于测试读文件断言（与 internal_log_handler 并行）
    if (fallback_out) {
        std::string level_str(mc::log::to_string(msg.get_level()));
        *fallback_out << module_name << " " << level_str << " " << file_str << "(" << ctx.m_line << "): " << message_str
                      << "\n";
        fallback_out->flush();
    }
}

void get_initiator(std::string& interface, std::string& username, std::string& client_addr)
{
    if (!mc::engine::context::get_current_context_ptr()) {
        return;
    }
    auto& ctx = mc::engine::context::get_current_context();

    if (ctx.get_arg("interface_name")) {
        interface = ctx.get_arg("interface_name").as<std::string_view>();
    }
    if (ctx.get_arg("username")) {
        username = ctx.get_arg("username").as<std::string_view>();
    }
    if (ctx.get_arg("client_addr")) {
        client_addr = ctx.get_arg("client_addr").as<std::string_view>();
    }
}

// 从 args 或 engine context 获取 initiator，各字段独立判断
void get_initiator_from_args(const mc::dict& args, std::string& interface_name, std::string& username,
                             std::string& client_addr, std::string& module_name)
{
    if (args.contains("interface_name")) {
        interface_name = args["interface_name"].as<std::string>();
    }

    if (args.contains("username")) {
        username = args["username"].as<std::string>();
    }

    if (args.contains("client_addr")) {
        client_addr = args["client_addr"].as<std::string>();
    }

    if (args.contains("module_name")) {
        module_name = args["module_name"].as<std::string>();
    }
}

void append_operation(const message& msg, std::ostream* fallback_out)
{
    // 获取上下文信息
    const context& ctx = msg.get_context();

    std::string message_str = msg.get_message();
    // 过滤无效字符，避免写入包含控制字符的内容
    logging::filter_invalid_chars(message_str);
    std::string interface_name = "N/A";
    std::string username       = "N/A";
    std::string client_addr    = "localhost";
    std::string module_name    = g_module_name;
    get_initiator(interface_name, username, client_addr);
    const auto& args = msg.get_args();
    get_initiator_from_args(args, interface_name, username, client_addr, module_name);

    // 获取模块名：优先使用消息参数中的 module_name（Lua 接口第一个参数传入）
    // 注意：debug_log_ptr 由外部 liblogger 输出模块名，这里在输出前设置模块名
    if (args.contains("module_name")) {
        try {
            module_name = args["module_name"].as<std::string>();
        } catch (...) {
            // 转换失败时使用默认模块名
        }
    }

    if (get_log_time_str_ptr) {
        const char* time_str = get_log_time_str_ptr(LOG_US_TIME);
        if (time_str) {
            syslog(LOG_LOCAL5 | LOG_INFO, "%s %s,%s@%s,%s,%s", time_str, interface_name.c_str(), username.c_str(),
                   client_addr.c_str(), module_name.c_str(), message_str.c_str());
        }
    }
    if (fallback_out) {
        *fallback_out << module_name << " " << message_str << "\n";
        fallback_out->flush();
    }
}

void append_hw_stream(const message& msg, std::ostream* fallback_out)
{
    const context& ctx = msg.get_context();

    std::string message_str = msg.get_message();
    logging::filter_invalid_chars(message_str);

    std::string module_name = g_module_name;
    const auto& args        = msg.get_args();
    if (args.contains("module_name")) {
        try {
            module_name = args["module_name"].as<std::string>();
        } catch (...) {
        }
    }

    std::string file_str;
    file_str.reserve(64);
    if (ctx.m_file.empty()) {
        file_str.append("unknown");
    } else {
        file_str.append(mc::filesystem::basename(ctx.m_file));
    }

    std::string level_str(mc::log::to_string(msg.get_level()));
    std::string line_str(std::to_string(ctx.m_line));

    if (get_log_time_str_ptr) {
        const char* time_str = get_log_time_str_ptr(LOG_US_TIME);
        if (time_str) {
            syslog(LOG_LOCAL6 | LOG_NOTICE, "%s %s %s: %s(%s): %s", time_str, module_name.c_str(), level_str.c_str(),
                   file_str.c_str(), line_str.c_str(), message_str.c_str());
        }
    }
    if (fallback_out) {
        *fallback_out << module_name << " " << level_str << " " << file_str << "(" << ctx.m_line << "): " << message_str
                      << "\n";
        fallback_out->flush();
    }
}

void append_running(const message& msg, std::ostream* fallback_out)
{
    std::string message_str = msg.get_message();
    logging::filter_invalid_chars(message_str);
    std::string level_str(mc::log::to_string(msg.get_level()));

    if (get_log_time_str_ptr) {
        const char* time_str = get_log_time_str_ptr(LOG_US_TIME);
        if (time_str) {
            syslog(LOG_LOCAL3 | LOG_INFO, "%s %-5s: %s", time_str, level_str.c_str(), message_str.c_str());
        }
    }
    if (fallback_out) {
        *fallback_out << level_str << ": " << message_str << "\n";
        fallback_out->flush();
    }
}

void append_security(const message& msg, std::ostream* fallback_out)
{
    std::string message_str = msg.get_message();
    logging::filter_invalid_chars(message_str);
    std::string level_str(mc::log::to_string(msg.get_level()));
    syslog(LOG_AUTHPRIV | LOG_INFO, "%s", message_str.c_str());
    if (fallback_out) {
        *fallback_out << message_str << "\n";
        fallback_out->flush();
    }
}

void append_maintenance(const message& msg, std::ostream* fallback_out)
{
    std::string message_str = msg.get_message();
    logging::filter_invalid_chars(message_str);
    std::string level_str(mc::log::to_string(msg.get_level()));
    std::string error_code;
    const auto& args = msg.get_args();
    if (args.contains("error_code")) {
        try {
            error_code = args["error_code"].as<std::string>();
        } catch (...) {
        }
    }

    if (get_log_time_str_ptr) {
        const char* time_str = get_log_time_str_ptr(LOG_US_TIME);
        if (time_str) {
            if (error_code.empty()) {
                syslog(LOG_LOCAL6 | LOG_INFO, "%s %-5s: %s", time_str, level_str.c_str(), message_str.c_str());
            } else {
                syslog(LOG_LOCAL6 | LOG_INFO, "%s %-5s: %s,%s", time_str, level_str.c_str(), error_code.c_str(),
                       message_str.c_str());
            }
        }
    }
    if (fallback_out) {
        if (error_code.empty()) {
            *fallback_out << level_str << ": " << message_str << "\n";
        } else {
            *fallback_out << level_str << ": " << error_code << "," << message_str << "\n";
        }
        fallback_out->flush();
    }
}

void append_mc_stream(const message& msg, std::ostream* fallback_out)
{
    const context& ctx = msg.get_context();

    std::string message_str = msg.get_message();
    logging::filter_invalid_chars(message_str);

    std::string module_name = g_module_name;
    const auto& args        = msg.get_args();
    if (args.contains("module_name")) {
        try {
            module_name = args["module_name"].as<std::string>();
        } catch (...) {
        }
    }

    std::string file_str;
    file_str.reserve(64);
    if (ctx.m_file.empty()) {
        file_str.append("unknown");
    } else {
        file_str.append(mc::filesystem::basename(ctx.m_file));
    }

    std::string level_str(mc::log::to_string(msg.get_level()));
    std::string line_str(std::to_string(ctx.m_line));

    if (get_log_time_str_ptr) {
        const char* time_str = get_log_time_str_ptr(LOG_US_TIME);
        if (time_str) {
            syslog(LOG_LOCAL5 | LOG_NOTICE, "%s %s %s: %s(%s): %s", time_str, module_name.c_str(), level_str.c_str(),
                   file_str.c_str(), line_str.c_str(), message_str.c_str());
        }
    }
    if (fallback_out) {
        *fallback_out << module_name << " " << level_str << " " << file_str << "(" << ctx.m_line << "): " << message_str
                      << "\n";
        fallback_out->flush();
    }
}

void file_appender::append(const message& msg)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    log_category  category = msg.get_category();
    std::ostream* fallback = m_file.is_open() ? &m_file : nullptr;
    switch (category) {
        case log_category::debug:
            append_debug(msg, fallback);
            break;
        case log_category::operation:
            append_operation(msg, fallback);
            break;
        case log_category::running:
            append_running(msg, fallback);
            break;
        case log_category::maintenance:
            append_maintenance(msg, fallback);
            break;
        case log_category::security:
            append_security(msg, fallback);
            break;
        case log_category::hw_stream:
            append_hw_stream(msg, fallback);
            break;
        case log_category::mc_stream:
            append_mc_stream(msg, fallback);
            break;
        case log_category::serial_printf:
            // serial_printf 使用与 debug 相同的格式
            append_debug(msg, fallback);
            break;
        default:
            append_debug(msg, fallback);
            break;
    }
}

void file_appender::set_filename(const std::string& filename)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // 检查文件是否存在（如果文件流打开但文件被删除，需要重新打开）
    bool file_exists = false;
    if (!filename.empty()) {
        struct stat st;
        file_exists = (stat(filename.c_str(), &st) == 0);
    }

    // 如果 filename 改变，或者文件没有打开，或者文件流状态异常，或者文件不存在，都需要重新打开
    bool need_reopen = (m_file_config.filename != filename) || !m_file.is_open() || m_file.fail() || m_file.bad() ||
                       (!filename.empty() && !file_exists);
    if (need_reopen) {
        std::string reopen_reason;
        if (m_file_config.filename != filename) {
            reopen_reason = "filename_changed";
        } else if (!m_file.is_open()) {
            reopen_reason = "file_not_open";
        } else if (m_file.fail() || m_file.bad()) {
            reopen_reason = "stream_failed";
        } else if (!file_exists) {
            reopen_reason = "file_deleted";
        }

        close_file();
        m_file_config.filename = filename;
        open_file();
        // 如果文件打开失败，尝试以创建模式重新打开
        if (!m_file.is_open()) {
            m_file.clear();
            m_file.open(m_file_config.filename, std::ios::out | std::ios::trunc);
        }
    }
}

void file_appender::set_debug_log_level(level lvl)
{
    if (set_log_level_ptr) {
        set_log_level_ptr(log_level_to_dlog_level(lvl));
    }
}

const std::string& file_appender::get_filename() const
{
    return m_file_config.filename;
}

void file_appender::set_level(level lvl)
{
    set_debug_log_level(lvl);
}

void file_appender::set_flush_on_write(bool flush_on_write)
{
    m_file_config.flush_on_write = flush_on_write;
}

bool file_appender::get_flush_on_write() const
{
    return m_file_config.flush_on_write;
}

void file_appender::flush()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_file.is_open()) {
        m_file.flush();
    }
}

void file_appender::open_file()
{
    if (!m_file_config.filename.empty()) {
        std::ios_base::openmode mode = std::ios::out;
        if (m_file_config.truncate) {
            mode |= std::ios::trunc;
        } else {
            mode |= std::ios::app;
        }

        m_file.open(m_file_config.filename, mode);
        // 确保文件打开成功，如果失败则清除 badbit 并重试
        if (!m_file.is_open() || m_file.fail()) {
            m_file.clear();
            m_file.open(m_file_config.filename, mode);
        }
    }
}

void file_appender::close_file()
{
    if (m_file.is_open()) {
        m_file.flush();
        m_file.close();
    }
}

void file_appender::set_debug_log_ptr(void* func_ptr)
{
    debug_log_ptr = reinterpret_cast<debug_log_func_t>(func_ptr);
}

} // namespace log
} // namespace mc