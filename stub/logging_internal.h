/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2022. All rights reserved.
 *
 * this file licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 * http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 *
 * Description: logging internal header file
 */

#ifndef LOGGING_INTERNAL_H
#define LOGGING_INTERNAL_H

#include <fstream>
#include <glib.h>
#include <string>

#define BUILD_TYPE_DT (0x0a)
namespace logging {

struct LogRecord {
    guint          is_skynet : 1;
    guint          level     : 4;
    guint          lineno    : 16;
    guint          hash;
    std::string    filename;
    std::string    module_name;
    std::string    log;
    std::string    log_template;
    struct timeval tv;
    guint          tick_sec;
    guint          period;
};

inline void filter_invalid_chars(std::string& str)
{}

// 测试用例可通过设置此变量来指定日志输出文件
// 默认值：TEST_LOG_DIR/test_file_appender_mock.log
inline std::string& get_stub_log_path() {
#ifdef TEST_LOG_DIR
    static std::string log_path = std::string(TEST_LOG_DIR) + "/test_file_appender_mock.log";
#else
    static std::string log_path = "/tmp/test_file_appender_mock.log";
#endif
    return log_path;
}

// 设置 stub 日志输出文件路径
inline void set_stub_log_path(const std::string& path) {
    get_stub_log_path() = path;
}

// Stub 实现：将日志写入到 get_stub_log_path() 指定的文件
inline void internal_log_handler(LogRecord* rec, bool limit) {
    std::ofstream ofs(get_stub_log_path(), std::ios::app);
    if (!ofs.is_open()) {
        return;
    }
    if (rec && !rec->log.empty()) {
        ofs << rec->log << std::endl;
    }
}

} // namespace logging

#endif /* LOGGING_INTERNAL_H */