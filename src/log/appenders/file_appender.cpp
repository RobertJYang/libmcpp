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
#include <stdarg.h>
#include <stdio.h>

typedef enum {
    DLOG_ERROR,
    DLOG_WARN,
    DLOG_NOTICE,
    DLOG_INFO,
    DLOG_DEBUG
} DLOG_LEVEL_E;

typedef void (*debug_log_func_t)(DLOG_LEVEL_E, const char*, int, const char*, ...);
typedef void (*set_log_module_name_func_t)(const char*);
static debug_log_func_t           debug_log_ptr           = nullptr;
static set_log_module_name_func_t set_log_module_name_ptr = nullptr;

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
        }
    }

    // 从配置中获取模块名称并设置
    auto dict = args.as<mc::dict>();
    if (dict.contains("module_name")) {
        std::string module_name = dict["module_name"].as<std::string>();
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

void file_appender::append(const message& msg) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // 获取上下文信息
    const context& ctx = msg.get_context();

    DLOG_LEVEL_E level;
    switch (msg.get_level()) {
    case level::debug:
        level = DLOG_DEBUG;
        break;
    case level::info:
        level = DLOG_INFO;
        break;
    case level::warn:
        level = DLOG_WARN;
        break;
    case level::error:
        level = DLOG_ERROR;
        break;
    case level::notice:
        level = DLOG_NOTICE;
        break;
    default:
        level = DLOG_DEBUG;
        break;
    }

    std::string file_str;
    file_str.reserve(64); // 预分配足够空间
    if (ctx.m_file.empty()) {
        file_str.append("unknown");
    } else {
        file_str.append(mc::filesystem::basename(ctx.m_file));
    }

    std::string message_str = msg.get_message();

    if (debug_log_ptr) {
        debug_log_ptr(level, file_str.c_str(), ctx.m_line, "%s", message_str.c_str());
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