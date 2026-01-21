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

#include <dlfcn.h>
#include <mc/filesystem.h>	 
 #include <mc/log/appenders/file_appender.h>	 
 #include <mc/log/log_level.h>	 
 #include <mc/engine/context.h>	 
 #include <mc/engine/service.h>	 
 #include <stdarg.h>	 
 #include <stdio.h>	 
 #include <string>	 
 #include <syslog.h>

#include <logging_internal.h>

#if __has_include(<logging.h>)
#include <logging.h>
#else
typedef enum {
    DLOG_ERROR,
    DLOG_WARN,
    DLOG_NOTICE,
    DLOG_INFO,
    DLOG_DEBUG
} DLOG_LEVEL_E;
#endif

typedef void (*debug_log_func_t)(DLOG_LEVEL_E, const char*, int, const char*, ...);	 
 typedef void (*set_log_module_name_func_t)(const char*);	 
 typedef const char* (*get_log_time_str_func_t)(int);	 
 typedef DLOG_LEVEL_E (*set_log_level_func_t)(DLOG_LEVEL_E);	 
 static debug_log_func_t           debug_log_ptr           = nullptr;	 
 static set_log_module_name_func_t set_log_module_name_ptr = nullptr;	 
 static get_log_time_str_func_t get_log_time_str_ptr = nullptr;	 
 static set_log_level_func_t set_log_level_ptr = nullptr; 
 static std::string                g_module_name{"Unknown"}; // 全局模块名称，所有 file_appender 实例共享 
 constexpr uint32_t LOG_US_TIME = 0x01;

// 将 mc::log::level 映射到 DLOG_LEVEL_E
// 对于没有对应级别的（all, trace, fatal, off），返回 DLOG_DEBUG 作为默认值
static DLOG_LEVEL_E log_level_to_dlog_level(mc::log::level lvl) {
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

file_appender::file_appender() {
}

bool file_appender::init(const variant& args) {
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

    // auto dict = args.as_object();

    // // 获取必要的配置参数
    // if (!dict.contains("name") || !dict.contains("filename")) {
    //     return false;
    // }

    // m_file_config.m_name     = dict["name"].as_string();
    // m_file_config.m_filename = dict["filename"].as_string();

    // // 获取可选的配置参数
    // if (dict.contains("truncate")) {
    //     m_file_config.m_truncate = dict["truncate"].as_bool();
    // }

    // if (dict.contains("flush_on_write")) {
    //     m_file_config.m_flush_on_write = dict["flush_on_write"].as_bool();
    // }

    open_file();
    return true;
}

file_appender::~file_appender() {
    close_file();
}

void append_debug(const message& msg) {
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

    logging::LogRecord record = {0};
    record.lineno             = ctx.m_line;
    record.module_name        = module_name;
    record.level              = level;
    record.is_skynet          = 0;
    record.log                = message_str;
    record.filename           = file_str.c_str();

    logging::internal_log_handler(&record, true);
}

void get_initiator(std::string& interface, std::string& username, std::string& client_addr) {
    if (!mc::engine::context::get_current_context_ptr()) {
        return;
    }
    auto &ctx = mc::engine::context::get_current_context();

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

void append_operation(const message& msg) {
    // 获取上下文信息
    const context& ctx = msg.get_context();

    std::string message_str = msg.get_message();
    // 过滤无效字符，避免写入包含控制字符的内容
    logging::filter_invalid_chars(message_str);
    std::string interface_name = "N/A";
    std::string username       = "N/A";
    std::string client_addr    = "localhost";
    get_initiator(interface_name, username, client_addr);

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

    if (get_log_time_str_ptr) {
        const char* time_str = get_log_time_str_ptr(LOG_US_TIME);
        if (time_str) {
            syslog(LOG_LOCAL5 | LOG_INFO, "%s %s,%s@%s,%s,%s", time_str,
                   interface_name.c_str(), username.c_str(), client_addr.c_str(), module_name.c_str(), message_str.c_str());
        }
    }
}

void file_appender::append(const message& msg) {
    std::lock_guard<std::mutex> lock(m_mutex);

    log_category category = msg.get_category();
    switch (category) {
    case log_category::debug:
        append_debug(msg);
        break;
    case log_category::operation:
        // 使用全局变量中的模块名，所有 file_appender 实例共享
        append_operation(msg);
        break;
    default:
        append_debug(msg);
        break;
    }
}

void file_appender::set_filename(const std::string& filename) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_file_config.filename != filename) {
        close_file();
        m_file_config.filename = filename;
        open_file();
    }
}

void file_appender::set_debug_log_level(level lvl) {
    if (set_log_level_ptr) {
        set_log_level_ptr(log_level_to_dlog_level(lvl));
    }
}

const std::string& file_appender::get_filename() const {
    return m_file_config.filename;
}

void file_appender::set_flush_on_write(bool flush_on_write) {
    m_file_config.flush_on_write = flush_on_write;
}

bool file_appender::get_flush_on_write() const {
    return m_file_config.flush_on_write;
}

void file_appender::flush() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_file.is_open()) {
        m_file.flush();
    }
}

void file_appender::open_file() {
    if (!m_file_config.filename.empty()) {
        std::ios_base::openmode mode = std::ios::out;
        if (m_file_config.truncate) {
            mode |= std::ios::trunc;
        } else {
            mode |= std::ios::app;
        }

        m_file.open(m_file_config.filename, mode);
    }
}

void file_appender::close_file() {
    if (m_file.is_open()) {
        m_file.flush();
        m_file.close();
    }
}

void file_appender::set_debug_log_ptr(void* func_ptr) {
    debug_log_ptr = reinterpret_cast<debug_log_func_t>(func_ptr);
}

} // namespace log
} // namespace mc