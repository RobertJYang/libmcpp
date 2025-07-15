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

#include "logging.h"
#include <cstring>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <mutex>

// 全局文件流和互斥锁
static std::ofstream g_log_file;
static std::mutex g_log_mutex;
static std::string g_log_filename;

// 获取日志级别字符串
static const char* get_level_string(DLOG_LEVEL_E level) {
    switch (level) {
        case DLOG_DEBUG:
            return "DEBUG";
        case DLOG_INFO:
            return "INFO";
        case DLOG_NOTICE:
            return "NOTICE";
        case DLOG_WARN:
            return "WARNING";
        case DLOG_ERROR:
            return "ERROR";
        default:
            return "UNKNOWN";
    }
}

// 初始化日志文件
void init_log_file(const std::string& filename) {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    if (g_log_file.is_open()) {
        g_log_file.close();
    }
    g_log_filename = filename;
    g_log_file.open(filename, std::ios::out | std::ios::app);
}

// 关闭日志文件
void close_log_file() {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    if (g_log_file.is_open()) {
        g_log_file.flush();
        g_log_file.close();
    }
}

// 检查日志文件是否存在且不为空
bool check_log_file_exists_and_not_empty(const std::string& filename) {
    return std::filesystem::exists(filename) && std::filesystem::file_size(filename) > 0;
}

void debug_log(DLOG_LEVEL_E level, const char* file, unsigned int line, const char* format, ...) {
    // 验证入参合法性
    if (file == nullptr) {
        std::cerr << "ERROR: debug_log - file parameter is null" << std::endl;
        return;
    }
    
    if (format == nullptr) {
        std::cerr << "ERROR: debug_log - format parameter is null" << std::endl;
        return;
    }
    
    // 验证日志级别是否在有效范围内
    if (level < DLOG_DEBUG || level > DLOG_ERROR) {
        std::cerr << "ERROR: debug_log - invalid log level: " << level << std::endl;
        return;
    }
    
    // 处理可变参数并打印消息内容
    va_list args;
    va_start(args, format);
    
    // 计算格式化后的字符串长度
    va_list args_copy;
    va_copy(args_copy, args);
    int length = vsnprintf(nullptr, 0, format, args_copy);
    va_end(args_copy);
    
    std::string message_content;
    if (length > 0) {
        // 分配缓冲区并格式化字符串
        char* buffer = new char[length + 1];
        vsnprintf(buffer, length + 1, format, args);
        message_content = buffer;
        delete[] buffer;
    } else {
        message_content = "(格式化失败)";
        std::cout << " 消息内容: " << message_content << std::endl;
    }
    
    va_end(args);
    
    // 写入文件
    {
        std::lock_guard<std::mutex> lock(g_log_mutex);
        if (g_log_file.is_open()) {
            g_log_file << "[" << get_level_string(level) << "] " 
                      << file << ":" << line << " - "
                      << message_content << std::endl;
            g_log_file.flush();
        }
    }
} 