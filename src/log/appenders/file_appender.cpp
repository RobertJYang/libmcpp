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

#include <mc/log/appenders/file_appender.h>

namespace mc {
namespace log {

file_appender::file_appender(const variant& args) {
    init(args);
}

bool file_appender::init(const variant& args) {
    if (!args.is_dict()) {
        return false;
    }

    auto dict = args.as_dict();

    // 获取必要的配置参数
    if (!dict.contains("name") || !dict.contains("filename")) {
        return false;
    }

    m_file_config.m_name     = dict["name"].as_string();
    m_file_config.m_filename = dict["filename"].as_string();

    // 获取可选的配置参数
    if (dict.contains("truncate")) {
        m_file_config.m_truncate = dict["truncate"].as_bool();
    }

    if (dict.contains("flush_on_write")) {
        m_file_config.m_flush_on_write = dict["flush_on_write"].as_bool();
    }

    open_file();
    return true;
}

file_appender::~file_appender() {
    close_file();
}

void file_appender::append(const message& msg) {
    if (static_cast<int>(msg.get_level()) < static_cast<int>(m_config.m_level)) {
        return;
    }

    std::string formatted_msg = format_message(msg);

    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_file.is_open()) {
        m_file << formatted_msg << std::endl;

        if (m_file_config.m_flush_on_write) {
            m_file.flush();
        }
    }
}

void file_appender::set_filename(const std::string& filename) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_file_config.m_filename != filename) {
        close_file();
        m_file_config.m_filename = filename;
        open_file();
    }
}

const std::string& file_appender::get_filename() const {
    return m_file_config.m_filename;
}

void file_appender::set_flush_on_write(bool flush_on_write) {
    m_file_config.m_flush_on_write = flush_on_write;
}

bool file_appender::get_flush_on_write() const {
    return m_file_config.m_flush_on_write;
}

void file_appender::flush() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_file.is_open()) {
        m_file.flush();
    }
}

void file_appender::open_file() {
    if (!m_file_config.m_filename.empty()) {
        std::ios_base::openmode mode = std::ios::out;
        if (m_file_config.m_truncate) {
            mode |= std::ios::trunc;
        } else {
            mode |= std::ios::app;
        }

        m_file.open(m_file_config.m_filename, mode);
    }
}

void file_appender::close_file() {
    if (m_file.is_open()) {
        m_file.flush();
        m_file.close();
    }
}

} // namespace log
} // namespace mc