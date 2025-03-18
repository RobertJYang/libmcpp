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
#include <mc/exception.h>

namespace mc {
namespace config {

// 验证应用程序配置
bool config_validator::validate_app_config(const app_config& config) {
    // 基本验证：必须指定类型、名称、API版本
    if (config.kind.empty()) {
        elog("应用程序配置验证失败: 未指定资源类型(kind)");
        return false;
    }
    
    if (config.api_version.empty()) {
        elog("应用程序配置验证失败: 未指定API版本(api_version)");
        return false;
    }
    
    if (config.meta.name.empty()) {
        elog("应用程序配置验证失败: 未指定名称(meta.name)");
        return false;
    }

    // 应用程序特有验证
    if (config.plugin_dir.empty()) {
        wlog("应用程序配置警告: 未指定插件目录(plugin_dir)，将使用默认值");
    }

    return true;
}

// 验证监督器配置
bool config_validator::validate_supervisor_config(const supervisor_config& config) {
    // 基本验证：必须指定类型、名称、API版本
    if (config.kind.empty()) {
        elog("监督器配置验证失败: 未指定资源类型(kind)");
        return false;
    }
    
    if (config.api_version.empty()) {
        elog("监督器配置验证失败: 未指定API版本(api_version)");
        return false;
    }
    
    if (config.meta.name.empty()) {
        elog("监督器配置验证失败: 未指定名称(meta.name)");
        return false;
    }

    // 监督器特有验证
    if (config.max_restarts < 0) {
        elog("监督器配置验证失败: 最大重启次数(max_restarts)不能为负数");
        return false;
    }

    return true;
}

// 验证服务配置
bool config_validator::validate_service_config(const service_config& config) {
    // 基本验证：必须指定类型、名称、API版本
    if (config.kind.empty()) {
        elog("服务配置验证失败: 未指定资源类型(kind)");
        return false;
    }
    
    if (config.api_version.empty()) {
        elog("服务配置验证失败: 未指定API版本(api_version)");
        return false;
    }
    
    if (config.meta.name.empty()) {
        elog("服务配置验证失败: 未指定名称(meta.name)");
        return false;
    }

    // 服务特有验证
    if (config.type.empty()) {
        elog("服务配置验证失败: 未指定服务类型(type)");
        return false;
    }

    return true;
}

// 验证插件配置
bool config_validator::validate_plugin_config(const plugin_config& config) {
    // 基本验证：必须指定类型、名称、API版本
    if (config.kind.empty()) {
        elog("插件配置验证失败: 未指定资源类型(kind)");
        return false;
    }
    
    if (config.api_version.empty()) {
        elog("插件配置验证失败: 未指定API版本(api_version)");
        return false;
    }
    
    if (config.meta.name.empty()) {
        elog("插件配置验证失败: 未指定名称(meta.name)");
        return false;
    }

    // 插件特有验证
    if (config.version.empty()) {
        wlog("插件配置警告: 未指定版本(version)");
    }

    return true;
}

} // namespace config
} // namespace mc 