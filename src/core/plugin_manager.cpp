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

#include "mc/core/plugin_manager.h"
#include <dlfcn.h>
#include <mc/filesystem.h>
#include <mc/log.h>

namespace mc {

plugin_manager::plugin_manager() = default;

plugin_manager::~plugin_manager() {
    unload_all_plugins();
}

void plugin_manager::set_plugin_dir(const std::string& plugin_dir) {
    m_plugin_dir = plugin_dir;
    ilog("设置插件目录: ${dir}", ("dir", m_plugin_dir));
}

const std::string& plugin_manager::plugin_dir() const {
    return m_plugin_dir;
}

bool plugin_manager::register_plugin(std::shared_ptr<plugin> plugin) {
    if (!plugin) {
        elog("无法注册空插件");
        return false;
    }

    const auto& info = plugin->get_info();
    const auto& name = info.name;

    if (m_plugins.find(name) != m_plugins.end()) {
        elog("插件'${name}'已经注册", ("name", name));
        return false;
    }

    ilog("注册插件'${name}'，版本'${version}'", 
         ("name", name)("version", info.version));
    m_plugins[name] = std::move(plugin);
    return true;
}

plugin* plugin_manager::find_plugin(const std::string& name) const {
    auto it = m_plugins.find(name);
    if (it == m_plugins.end()) {
        return nullptr;
    }
    return it->second.get();
}

bool plugin_manager::load_plugin(const std::string& name) {
    if (find_plugin(name)) {
        ilog("插件'${name}'已加载", ("name", name));
        return true;
    }

    void* handle = nullptr;
    std::shared_ptr<plugin> plugin;

    if (!load_dynamic_library(name, handle, plugin)) {
        return false;
    }

    m_plugin_handles.push_back(handle);
    return register_plugin(std::move(plugin));
}

void plugin_manager::load_plugins(const std::vector<std::string>& plugin_names) {
    for (const auto& name : plugin_names) {
        if (!load_plugin(name)) {
            wlog("加载插件'${name}'失败", ("name", name));
        }
    }
}

bool plugin_manager::unload_plugin(const std::string& name) {
    auto it = m_plugins.find(name);
    if (it == m_plugins.end()) {
        wlog("插件'${name}'未找到，无法卸载", ("name", name));
        return false;
    }

    // 停止插件
    it->second->stop();
    
    // 移除
    m_plugins.erase(it);
    
    // 注意：不卸载动态库，因为不容易找到对应的句柄
    // 在析构函数中统一卸载所有动态库
    
    ilog("卸载插件'${name}'成功", ("name", name));
    return true;
}

void plugin_manager::unload_all_plugins() {
    // 停止所有插件
    stop_plugins();
    
    // 清空插件映射
    m_plugins.clear();
    
    // 卸载所有动态库
    for (void* handle : m_plugin_handles) {
        if (handle) {
            dlclose(handle);
        }
    }
    
    m_plugin_handles.clear();
    
    ilog("已卸载所有插件");
}

/**
 * @brief 初始化加载的所有插件
 * @param factory 服务工厂实例
 * @return 如果所有插件都成功初始化则返回true，否则返回false
 */
bool plugin_manager::init_plugins(service_factory& factory) {
    bool all_success = true;
    
    for (auto& [name, plugin] : m_plugins) {
        ilog("初始化插件: ${name}", ("name", plugin->get_info().name));
        if (!plugin->init(factory)) {
            elog("初始化插件'${name}'失败", ("name", plugin->get_info().name));
            all_success = false;
            // 继续初始化其他插件
        }
    }
    
    return all_success;
}

bool plugin_manager::start_plugins() {
    for (auto& [name, plugin] : m_plugins) {
        ilog("启动插件'${name}'", ("name", name));
        if (!plugin->start()) {
            elog("启动插件'${name}'失败", ("name", name));
            return false;
        }
    }
    
    return true;
}

bool plugin_manager::stop_plugins() {
    bool success = true;
    
    for (auto& [name, plugin] : m_plugins) {
        ilog("停止插件'${name}'", ("name", name));
        if (!plugin->stop()) {
            elog("停止插件'${name}'失败", ("name", name));
            success = false;
        }
    }
    
    return success;
}

std::vector<std::string> plugin_manager::get_loaded_plugins() const {
    std::vector<std::string> result;
    result.reserve(m_plugins.size());
    
    for (const auto& [name, _] : m_plugins) {
        result.push_back(name);
    }
    
    return result;
}

bool plugin_manager::load_dynamic_library(
    const std::string& plugin_name, 
    void*& handle, 
    std::shared_ptr<plugin>& plugin) {
    
    // 尝试加载动态库
    std::string lib_path = mc::filesystem::join(m_plugin_dir, "lib" + plugin_name + ".so");
    
    if (!mc::filesystem::exists(lib_path)) {
        elog("插件库文件不存在: ${path}", ("path", lib_path));
        return false;
    }
    
    // 加载动态库
    handle = dlopen(lib_path.c_str(), RTLD_LAZY | RTLD_LOCAL);
    if (!handle) {
        elog("加载动态库失败: ${path}, 错误: ${error}", 
             ("path", lib_path)("error", dlerror()));
        return false;
    }
    
    // 查找创建函数
    auto create_func = (create_plugin_func)dlsym(handle, "create_plugin");
    if (!create_func) {
        elog("找不到插件创建函数: ${error}", ("error", dlerror()));
        dlclose(handle);
        handle = nullptr;
        return false;
    }
    
    // 查找销毁函数
    auto destroy_func = (destroy_plugin_func)dlsym(handle, "destroy_plugin");
    if (!destroy_func) {
        elog("找不到插件销毁函数: ${error}", ("error", dlerror()));
        dlclose(handle);
        handle = nullptr;
        return false;
    }
    
    // 创建插件实例
    mc::plugin* raw_plugin = create_func();
    if (!raw_plugin) {
        elog("创建插件实例失败");
        dlclose(handle);
        handle = nullptr;
        return false;
    }
    
    // 创建智能指针，使用自定义删除器
    plugin = std::shared_ptr<mc::plugin>(raw_plugin, 
        [destroy_func](mc::plugin* p) { destroy_func(p); });
    
    ilog("成功加载插件库: ${path}", ("path", lib_path));
    return true;
}

} // namespace mc 