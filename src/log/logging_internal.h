/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef MC_LOGGING_INTERNAL_H
#define MC_LOGGING_INTERNAL_H

#include <fstream>
#include <string>

#include <glib.h>

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
{
    (void)str;
}

inline std::string& get_stub_log_path()
{
    static std::string log_path;
    return log_path;
}

inline void set_stub_log_path(const std::string& path)
{
    get_stub_log_path() = path;
}

inline void internal_log_handler(LogRecord* rec, bool limit)
{
    (void)limit;

    const auto& path = get_stub_log_path();
    if (path.empty()) {
        return;
    }

    std::ofstream ofs(path, std::ios::app);
    if (!ofs.is_open()) {
        return;
    }
    if (rec && !rec->log.empty()) {
        ofs << rec->log << std::endl;
    }
}

} // namespace logging

#endif
