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

/**
 * @file plugin_manager.h
 * @brief 插件管理器，负责插件的加载、初始化、启动和停止
 */
#ifndef MC_CORE_PLUGIN_MANAGER_H
#define MC_CORE_PLUGIN_MANAGER_H

#include <mc/core/plugin.h>
#include <mc/core/service_factory.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace mc::core {

/**
 * @brief 插件管理器类
 */
class MC_API plugin_manager {
public:
    // 构造和析构
    MC_API plugin_manager();
    MC_API ~plugin_manager();

    // 插件目录管理
    MC_API void  set_plugin_dir(const std::string& plugin_dir);
    MC_API const std::string& plugin_dir() const;

    // 插件管理
    MC_API bool    register_plugin(std::shared_ptr<plugin> plugin);
    MC_API plugin* find_plugin(const std::string& name) const;

    // 插件加载和卸载
    MC_API bool load_plugin(const std::string& name);
    /**
     * @brief 加载插件
     * @param plugin_names 要加载的插件名称列表，如果为空则加载目录下的所有插件
     * @return 是否加载成功
     */
    MC_API bool load_plugins(const std::vector<std::string>& plugin_names);
    MC_API bool unload_plugin(const std::string& name);
    MC_API void unload_all_plugins();

    // 插件生命周期管理
    MC_API bool init_plugins(service_factory& factory);

    // 获取加载的插件列表
    MC_API std::vector<std::string> get_loaded_plugins() const;

private:
    // 加载动态库并创建插件实例
    bool load_dynamic_library(const std::string& plugin_name, void*& handle,
                              std::shared_ptr<plugin>& plugin);

    // 成员变量
    std::string                                              m_plugin_dir;     // 插件目录
    std::unordered_map<std::string, std::shared_ptr<plugin>> m_plugins;        // 插件映射表
    std::vector<void*>                                       m_plugin_handles; // 动态库句柄列表
};

} // namespace mc::core

#endif // MC_CORE_PLUGIN_MANAGER_H