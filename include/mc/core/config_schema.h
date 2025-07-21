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

/**
 * @file config_schema.h
 * @brief 配置模式定义
 */
#ifndef MC_CONFIG_SCHEMA_H
#define MC_CONFIG_SCHEMA_H

#include <mc/dict.h>
#include <mc/reflect.h>

namespace mc::config {

/**
 * @brief 元数据结构
 */
struct metadata {
    MC_REFLECTABLE("mc.config.metadata")

    std::string         name;        // 名称
    std::optional<dict> labels;      // 标签
    std::optional<dict> annotations; // 注解
};

/**
 * @brief 资源基类
 */
struct resource_base {
    MC_REFLECTABLE("mc.config.resource_base")

    std::string api_version;
    std::string kind; // 资源类型
    metadata    meta; // 元数据
};

/**
 * @brief 应用程序配置
 */
struct app_config : resource_base {
    MC_REFLECTABLE("mc.config.app_config")

    std::string              plugin_dir;   // 插件目录
    std::vector<std::string> plugins;      // 插件列表
    std::size_t              threads;      // 线程数量
    std::size_t              work_threads; // 工作线程数量
};

/**
 * @brief 监督器策略
 */
enum class supervisor_strategy {
    one_for_one, // 只重启失败的服务
    one_for_all, // 重启所有服务
    rest_for_one // 重启失败服务及其后定义的服务
};

/**
 * @brief 监督器配置
 */
struct supervisor_config : resource_base {
    MC_REFLECTABLE("mc.config.supervisor_config")

    supervisor_strategy      strategy;     // 监督策略
    int                      max_restarts; // 最大重启次数
    std::vector<std::string> services;     // 服务列表
};

/**
 * @brief 服务配置
 */
struct service_config : resource_base {
    MC_REFLECTABLE("mc.config.service_config")

    std::string              type;         // 服务类型
    std::vector<std::string> dependencies; // 依赖的服务
    dict                     properties;   // 服务属性
};

/**
 * @brief 插件配置
 */
struct plugin_config : resource_base {
    MC_REFLECTABLE("mc.config.plugin_config")

    std::string version;    // 插件版本
    dict        properties; // 插件属性
};

/**
 * @brief 配置验证器，验证配置对象是否有效
 */
class MC_API config_validator {
public:
    /**
     * @brief 验证应用程序配置
     * @param config 应用程序配置
     * @return 验证结果，true表示有效，false表示无效
     */
    MC_API static bool validate_app_config(const app_config& config);

    /**
     * @brief 验证监督器配置
     * @param config 监督器配置
     * @return 验证结果，true表示有效，false表示无效
     */
    MC_API static bool validate_supervisor_config(const supervisor_config& config);

    /**
     * @brief 验证服务配置
     * @param config 服务配置
     * @return 验证结果，true表示有效，false表示无效
     */
    MC_API static bool validate_service_config(const service_config& config);

    /**
     * @brief 验证插件配置
     * @param config 插件配置
     * @return 验证结果，true表示有效，false表示无效
     */
    MC_API static bool validate_plugin_config(const plugin_config& config);
};

// 元数据配置
struct meta_config {
    std::string               name;
    std::string               version;
    std::string               description;
    std::shared_ptr<mc::dict> labels;
};

} // namespace mc::config

MC_REFLECTABLE("mc.config.supervisor_strategy", mc::config::supervisor_strategy)

#endif // MC_CONFIG_SCHEMA_H