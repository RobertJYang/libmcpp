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

#include "mc/core/module_manager.h"
#include "mc/filesystem.h"
#include <dlfcn.h>
#include <iostream>
#include <unordered_set>
#include <mc/log.h>
#include <mutex>

namespace mc {

// 构造函数
module_manager::module_manager() {
}

// 析构函数
module_manager::~module_manager() {
    unload_all_modules();
}

// 设置模块目录
void module_manager::set_module_dir(const std::string& module_dir) {
    m_module_dir = module_dir;
}

// 获取模块目录
const std::string& module_manager::module_dir() const {
    return m_module_dir;
}

// 注册模块
bool module_manager::register_module(std::shared_ptr<class module> module) {
    // 检查模块信息是否有效
    if (!module) {
        elog("错误: 尝试注册空模块");
        return false;
    }
    
    // 检查模块是否已注册
    const auto& info = module->get_info();
    if (m_modules.find(info.name) != m_modules.end()) {
        elog("错误: 模块 '${name}' 已注册", ("name", info.name));
        return false;
    }
    
    // 注册模块
    m_modules[info.name] = std::move(module);
    
    return true;
}

// 查找模块
class module* module_manager::find_module(const std::string& name) const {
    auto it = m_modules.find(name);
    if (it != m_modules.end()) {
        return it->second.get();
    }
    return nullptr;
}

// 加载模块
bool module_manager::load_module(const std::string& name) {
    try {
        // 加载动态库并创建模块实例
        void* handle = nullptr;
        std::shared_ptr<class module> module;
        
        if (!load_dynamic_library(name, handle, module)) {
            return false;
        }
        
        // 初始化模块
        if (!module->init()) {
            elog("初始化模块 '${name}' 失败", ("name", name));
            dlclose(handle);
            return false;
        }
        
        // 保存模块和句柄
        if (!register_module(std::move(module))) {
            dlclose(handle);
            return false;
        }
        
        // 保存句柄
        m_module_handles.push_back(handle);
        
        ilog("加载模块 '${name}' 成功", ("name", name));
        return true;
    } catch (const std::exception& e) {
        elog("加载模块 '${name}' 失败: ${error}", ("name", name)("error", e.what()));
        return false;
    }
}

// 加载多个模块
void module_manager::load_modules(const std::vector<std::string>& module_names) {
    std::unordered_set<std::string> loaded_modules;
    for (const auto& name : module_names) {
        if (loaded_modules.count(name)) {
            continue;
        }
        
        load_module(name);
        loaded_modules.insert(name);
    }
}

// 卸载模块
bool module_manager::unload_module(const std::string& name) {
    auto it = m_modules.find(name);
    if (it == m_modules.end()) {
        elog("错误: 模块 '${name}' 未加载", ("name", name));
        return false;
    }
    
    if (!it->second->unload()) {
        elog("卸载模块 '${name}' 失败", ("name", name));
    }
    
    // 移除模块
    m_modules.erase(it);
    
    // 关闭动态库（简化处理，实际应该找到对应的句柄）
    for (auto handle_it = m_module_handles.begin(); handle_it != m_module_handles.end(); ) {
        dlclose(*handle_it);
        handle_it = m_module_handles.erase(handle_it);
        break;  // 简化处理，只关闭一个句柄
    }
    
    ilog("卸载模块 '${name}' 成功", ("name", name));
    return true;
}

// 卸载所有模块
void module_manager::unload_all_modules() {
    // 先停止所有模块
    stop_modules();
    
    // 卸载所有模块
    std::vector<std::string> modules_to_unload;
    for (const auto& [name, _] : m_modules) {
        modules_to_unload.push_back(name);
    }
    
    for (const auto& name : modules_to_unload) {
        unload_module(name);
    }
    
    // 确保模块列表和句柄列表都已清空
    m_modules.clear();
    m_module_handles.clear();
}

// 初始化所有模块
bool module_manager::init_modules() {
    bool success = true;
    
    for (auto& [name, module] : m_modules) {
        if (!module->init()) {
            elog("初始化模块 '${name}' 失败", ("name", name));
            success = false;
        }
    }
    
    return success;
}

// 启动所有模块
bool module_manager::start_modules() {
    bool success = true;
    for (auto& [name, module] : m_modules) {
        try {
            if (!module->start()) {
                elog("启动模块 '${name}' 失败", ("name", name));
                success = false;
            }
        } catch (const std::exception& e) {
            elog("启动模块 '${name}' 失败: ${error}", ("name", name)("error", e.what()));
            success = false;
        }
    }
    return success;
}

// 停止所有模块
bool module_manager::stop_modules() {
    bool success = true;
    for (auto& [name, module] : m_modules) {
        try {
            if (!module->stop()) {
                elog("停止模块 '${name}' 失败", ("name", name));
                success = false;
            }
        } catch (const std::exception& e) {
            elog("停止模块 '${name}' 失败: ${error}", ("name", name)("error", e.what()));
            success = false;
        }
    }
    return success;
}

// 加载动态库并创建模块实例
bool module_manager::load_dynamic_library(const std::string& module_name, void*& handle, std::shared_ptr<class module>& module) {
    // 检查模块是否已加载
    if (m_modules.find(module_name) != m_modules.end()) {
        ilog("模块 '${name}' 已加载", ("name", module_name));
        return false;
    }
    
    // 检查模块路径
    if (m_module_dir.empty()) {
        elog("错误: 未设置模块目录，无法加载模块 '${name}'", ("name", module_name));
        return false;
    }
    
    // 构建模块库路径
    std::string module_path = mc::filesystem::join(m_module_dir, "lib" + module_name + ".so");
    if (!mc::filesystem::exists(module_path)) {
        elog("错误: 模块文件 '${path}' 不存在", ("path", module_path));
        return false;
    }
    
    // 加载动态库
    handle = dlopen(module_path.c_str(), RTLD_NOW | RTLD_GLOBAL);
    if (!handle) {
        elog("错误: 无法加载模块 '${name}': ${error}", 
             ("name", module_name)("error", dlerror()));
        return false;
    }
    
    // 查找创建模块的函数
    using create_module_func = class module* (*)();
    create_module_func create_module = 
        reinterpret_cast<create_module_func>(dlsym(handle, "create_module"));
    
    if (!create_module) {
        elog("错误: 模块 '${name}' 没有导出 'create_module' 函数: ${error}", 
             ("name", module_name)("error", dlerror()));
        dlclose(handle);
        return false;
    }
    
    // 创建模块实例
    module = std::shared_ptr<class module>(create_module());
    if (!module) {
        elog("错误: 无法创建模块 '${name}' 的实例", ("name", module_name));
        dlclose(handle);
        return false;
    }
    
    return true;
}

} // namespace mc 