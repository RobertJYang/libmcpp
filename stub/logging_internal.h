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

inline void filter_invalid_chars(std::string& str) {
}

inline void internal_log_handler(LogRecord* rec, bool limit) {
}

} // namespace logging

#endif /* LOGGING_INTERNAL_H */