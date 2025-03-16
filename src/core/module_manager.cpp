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
    if (!module) {
        std::cerr << "错误: 尝试注册空模块" << std::endl;
        return false;
    }
    
    const auto& info = module->get_info();
    if (m_modules.find(info.name) != m_modules.end()) {
        std::cerr << "错误: 模块 '" << info.name << "' 已注册" << std::endl;
        return false;
    }
    
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
            std::cerr << "初始化模块 '" << name << "' 失败" << std::endl;
            dlclose(handle);
            return false;
        }
        
        // 保存模块和句柄
        m_modules[name] = std::move(module);
        m_module_handles.push_back(handle);
        
        std::cout << "加载模块 '" << name << "' 成功" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "加载模块 '" << name << "' 失败: " << e.what() << std::endl;
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
        std::cerr << "错误: 模块 '" << name << "' 未加载" << std::endl;
        return false;
    }
    
    try {
        // 停止模块
        auto& module = it->second;
        if (!module->stop()) {
            std::cerr << "停止模块 '" << name << "' 失败" << std::endl;
        }
        
        // 卸载模块
        if (!module->unload()) {
            std::cerr << "卸载模块 '" << name << "' 失败" << std::endl;
            return false;
        }
        
        // 移除模块
        m_modules.erase(it);
        
        // 关闭动态库（这里简化处理，实际应该找到对应的句柄）
        for (auto handle_it = m_module_handles.begin(); handle_it != m_module_handles.end(); ) {
            dlclose(*handle_it);
            handle_it = m_module_handles.erase(handle_it);
            break;  // 简化处理，只关闭一个句柄
        }
        
        std::cout << "卸载模块 '" << name << "' 成功" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "卸载模块 '" << name << "' 失败: " << e.what() << std::endl;
        return false;
    }
}

// 卸载所有模块
void module_manager::unload_all_modules() {
    // 先停止所有模块
    stop_modules();

    // 按照依赖关系的反序卸载模块
    std::vector<std::string> unload_order;
    std::unordered_set<std::string> visited;

    // 辅助函数：深度优先遍历获取卸载顺序
    std::function<void(const std::string&)> visit = [&](const std::string& name) {
        if (visited.find(name) != visited.end()) {
            return;
        }
        visited.insert(name);

        auto it = m_modules.find(name);
        if (it != m_modules.end()) {
            const auto& module = it->second;
            for (const auto& dep : module->get_info().dependencies) {
                visit(dep);
            }
            unload_order.push_back(name);
        }
    };

    // 获取卸载顺序
    for (const auto& [name, _] : m_modules) {
        visit(name);
    }

    // 按顺序卸载模块
    for (auto it = unload_order.rbegin(); it != unload_order.rend(); ++it) {
        const auto& name = *it;
        auto module_it = m_modules.find(name);
        if (module_it == m_modules.end()) {
            continue;
        }

        try {
            auto& module = module_it->second;
            if (!module->unload()) {
                std::cerr << "卸载模块 '" << name << "' 失败" << std::endl;
                continue;
            }
            
            std::cout << "卸载模块 '" << name << "' 成功" << std::endl;
            m_modules.erase(module_it);
        } catch (const std::exception& e) {
            std::cerr << "卸载模块 '" << name << "' 失败: " << e.what() << std::endl;
        }
    }

    // 最后关闭所有动态库
    for (void* handle : m_module_handles) {
        dlclose(handle);
    }
    m_module_handles.clear();
}

// 初始化所有模块
bool module_manager::init_modules() {
    bool success = true;
    for (auto& [name, module] : m_modules) {
        try {
            if (!module->init()) {
                std::cerr << "初始化模块 '" << name << "' 失败" << std::endl;
                success = false;
            }
        } catch (const std::exception& e) {
            std::cerr << "初始化模块 '" << name << "' 失败: " << e.what() << std::endl;
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
                std::cerr << "启动模块 '" << name << "' 失败" << std::endl;
                success = false;
            }
        } catch (const std::exception& e) {
            std::cerr << "启动模块 '" << name << "' 失败: " << e.what() << std::endl;
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
                std::cerr << "停止模块 '" << name << "' 失败" << std::endl;
                success = false;
            }
        } catch (const std::exception& e) {
            std::cerr << "停止模块 '" << name << "' 失败: " << e.what() << std::endl;
            success = false;
        }
    }
    return success;
}

// 加载动态库并创建模块实例
bool module_manager::load_dynamic_library(const std::string& module_name, void*& handle, std::shared_ptr<class module>& module) {
    // 检查模块是否已加载
    if (m_modules.find(module_name) != m_modules.end()) {
        std::cout << "模块 '" << module_name << "' 已加载" << std::endl;
        return false;
    }
    
    // 检查模块路径
    if (m_module_dir.empty()) {
        std::cerr << "错误: 未设置模块目录，无法加载模块 '" << module_name << "'" << std::endl;
        return false;
    }
    
    // 构建模块库路径
    std::string module_path = mc::filesystem::join(m_module_dir, "lib" + module_name + ".so");
    if (!mc::filesystem::exists(module_path)) {
        std::cerr << "错误: 模块文件 '" << module_path << "' 不存在" << std::endl;
        return false;
    }
    
    // 加载动态库
    handle = dlopen(module_path.c_str(), RTLD_NOW | RTLD_GLOBAL);
    if (!handle) {
        std::cerr << "错误: 无法加载模块 '" << module_name << "': " << dlerror() << std::endl;
        return false;
    }
    
    // 查找创建模块的函数
    using create_module_func = class module* (*)();
    create_module_func create_module = 
        reinterpret_cast<create_module_func>(dlsym(handle, "create_module"));
    
    if (!create_module) {
        std::cerr << "错误: 模块 '" << module_name << "' 没有导出 'create_module' 函数: " 
                  << dlerror() << std::endl;
        dlclose(handle);
        return false;
    }
    
    // 创建模块实例
    module = std::shared_ptr<class module>(create_module());
    if (!module) {
        std::cerr << "错误: 无法创建模块 '" << module_name << "' 的实例" << std::endl;
        dlclose(handle);
        return false;
    }
    
    return true;
}

} // namespace mc 