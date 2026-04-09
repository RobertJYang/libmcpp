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

#include "log/builtin_appenders.h"

#include <mc/log.h>
#include <mc/log/appender_factory.h>
#include <mc/log/appenders/file_appender.h>
#include <mc/log/appenders/socket_appender.h>

#include <mutex>

namespace mc::log {

void register_builtin_appenders()
{
    static std::once_flag once;
    std::call_once(once, []() {
        auto& factory = appender_factory::instance();
        factory.register_creator("file", []() {
            return std::make_shared<file_appender>();
        });
        factory.register_creator("socket", []() {
            return std::make_shared<socket_appender>();
        });
    });
}

void bootstrap_default_logging()
{
    register_builtin_appenders();

    static std::once_flag once;
    std::call_once(once, []() {
        auto default_log = mc::log::default_logger();
        if (default_log.find_appender("default_file")) {
            return;
        }

        auto file_appender = appender_factory::instance().get_appender("default_file");
        if (!file_appender) {
            file_appender = appender_factory::instance().create("default_file", "file", {});
        }

        if (file_appender) {
            default_log.add_appender(file_appender);
        }
    });
}

} // namespace mc::log
