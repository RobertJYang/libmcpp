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
 * @file module_manager.h
 * @brief 模块管理器，负责模块的加载、初始化、启动、停止和卸载
 */
#ifndef MC_MODULE_MANAGER_H
#define MC_MODULE_MANAGER_H

#include "mc/core/module.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace mc {

/**
 * @brief 模块管理器类
 */
class module_manager {
public:
    // 构造和析构
    module_manager();
    ~module_manager();

    // 模块目录管理
    void set_module_dir(const std::string& module_dir);
    const std::string& module_dir() const;

    // 模块管理
    bool register_module(std::shared_ptr<class module> module);
    class module* find_module(const std::string& name) const;
    
    // 模块加载和卸载
    bool load_module(const std::string& name);
    void load_modules(const std::vector<std::string>& module_names);
    bool unload_module(const std::string& name);
    void unload_all_modules();
    
    // 模块生命周期管理
    bool init_modules();
    bool start_modules();
    bool stop_modules();

private:
    // 加载动态库并创建模块实例
    bool load_dynamic_library(const std::string& module_name, void*& handle, std::shared_ptr<class module>& module);

    // 成员变量
    std::string m_module_dir;                                           // 模块目录
    std::unordered_map<std::string, std::shared_ptr<class module>> m_modules;  // 模块映射表
    std::vector<void*> m_module_handles;                                // 动态库句柄列表
};

} // namespace mc

#endif // MC_MODULE_MANAGER_H 