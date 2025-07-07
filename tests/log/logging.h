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

#ifndef LOGGING_H
#define LOGGING_H

#include <cstdarg>
#include <cstdio>
#include <string>

// 日志级别枚举
typedef enum {
    DLOG_DEBUG = 0,
    DLOG_INFO,
    DLOG_NOTICE,
    DLOG_WARN,
    DLOG_ERROR
} DLOG_LEVEL_E;

// debug_log函数声明
// 参数说明：
// level: 日志级别
// file: 文件名
// line: 行号
// format: 格式化字符串
// ...: 可变参数
void debug_log(DLOG_LEVEL_E level, const char* file, unsigned int line, const char* format, ...);

// 日志文件管理函数
void init_log_file(const std::string& filename);
void close_log_file();
bool check_log_file_exists_and_not_empty(const std::string& filename);

#endif // LOGGING_H 