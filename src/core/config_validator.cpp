/*
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <mc/core/config_schema.h>
#include <mc/log.h>

namespace mc::config {

bool config_validator::validate_app_config(const app_config& config) {
    if (config.kind.empty()) {
        elog("application config validation failed: missing resource type(kind)");
        return false;
    }

    if (config.api_version.empty()) {
        elog("application config validation failed: missing api version(api_version)");
        return false;
    }

    if (config.meta.name.empty()) {
        elog("application config validation failed: missing name(meta.name)");
        return false;
    }

    if (config.plugin_dir.empty()) {
        wlog("application config warning: missing plugin directory(plugin_dir), using default "
             "value");
    }

    return true;
}

bool config_validator::validate_supervisor_config(const supervisor_config& config) {
    if (config.kind.empty()) {
        elog("supervisor config validation failed: missing resource type(kind)");
        return false;
    }

    if (config.api_version.empty()) {
        elog("supervisor config validation failed: missing api version(api_version)");
        return false;
    }

    if (config.meta.name.empty()) {
        elog("supervisor config validation failed: missing name(meta.name)");
        return false;
    }

    if (config.max_restarts < 0) {
        elog("supervisor config validation failed: max restarts(max_restarts) cannot be negative");
        return false;
    }

    return true;
}

bool config_validator::validate_service_config(const service_config& config) {
    if (config.kind.empty()) {
        elog("service config validation failed: missing resource type(kind)");
        return false;
    }

    if (config.api_version.empty()) {
        elog("service config validation failed: missing api version(api_version)");
        return false;
    }

    if (config.meta.name.empty()) {
        elog("service config validation failed: missing name(meta.name)");
        return false;
    }

    if (config.type.empty()) {
        elog("service config validation failed: missing service type(type)");
        return false;
    }

    return true;
}

bool config_validator::validate_plugin_config(const plugin_config& config) {
    if (config.kind.empty()) {
        elog("plugin config validation failed: missing resource type(kind)");
        return false;
    }

    if (config.api_version.empty()) {
        elog("plugin config validation failed: missing api version(api_version)");
        return false;
    }

    if (config.meta.name.empty()) {
        elog("plugin config validation failed: missing name(meta.name)");
        return false;
    }

    if (config.version.empty()) {
        wlog("plugin config warning: missing version(version)");
    }

    return true;
}

} // namespace mc::config