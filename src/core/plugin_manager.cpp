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

void plugin_manager::set_plugin_dir(const std::string& dir) {
    m_plugin_dir = dir;
    dlog("设置插件目录: ${dir}", ("dir", dir));
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

    dlog("注册插件'${name}'，版本'${version}'", 
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
        dlog("插件'${name}'已加载", ("name", name));
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

bool plugin_manager::load_plugins(const std::vector<std::string>& plugin_names) {
    if (m_plugin_dir.empty()) {
        elog("插件目录未设置");
        return false;
    }

    // 如果指定了插件名称，只加载指定的插件
    if (!plugin_names.empty()) {
        for (const auto& name : plugin_names) {
            void* handle = nullptr;
            std::shared_ptr<plugin> plugin;
            if (!load_dynamic_library(name, handle, plugin)) {
                wlog("加载插件失败: ${name}", ("name", name));
                continue;
            }
            m_plugin_handles.push_back(handle);
            if (!register_plugin(std::move(plugin))) {
                wlog("注册插件失败: ${name}", ("name", name));
            }
        }
        return true;
    }

    // 否则加载目录下的所有插件
    try {
        for (const auto& entry : std::filesystem::directory_iterator(m_plugin_dir)) {
            if (entry.path().extension() == ".so") {
                std::string name = entry.path().stem().string();
                if (name.substr(0, 3) == "lib") {
                    name = name.substr(3);  // 移除 "lib" 前缀
                }
                void* handle = nullptr;
                std::shared_ptr<plugin> plugin;
                if (!load_dynamic_library(name, handle, plugin)) {
                    wlog("加载插件失败: ${path}", ("path", entry.path().string()));
                    continue;
                }
                m_plugin_handles.push_back(handle);
                if (!register_plugin(std::move(plugin))) {
                    wlog("注册插件失败: ${name}", ("name", name));
                }
            }
        }
        return true;
    } catch (const std::filesystem::filesystem_error& e) {
        elog("遍历插件目录失败: ${error}", ("error", e.what()));
        return false;
    }
}

bool plugin_manager::unload_plugin(const std::string& name) {
    auto it = m_plugins.find(name);
    if (it == m_plugins.end()) {
        wlog("插件'${name}'未找到，无法卸载", ("name", name));
        return false;
    }
    
    // 移除
    m_plugins.erase(it);
    
    // 注意：不卸载动态库，因为不容易找到对应的句柄
    // 在析构函数中统一卸载所有动态库
    
    dlog("卸载插件: ${name}", ("name", name));
    return true;
}

void plugin_manager::unload_all_plugins() {
    dlog("卸载所有插件");
    m_plugins.clear();
    
    // 卸载所有动态库
    for (void* handle : m_plugin_handles) {
        if (handle) {
            dlclose(handle);
        }
    }
    
    m_plugin_handles.clear();
}

bool plugin_manager::init_plugins(service_factory& factory) {
    bool all_success = true;
    
    for (auto& [name, plugin] : m_plugins) {
        if (!plugin->init(factory)) {
            elog("初始化插件失败: ${name}", ("name", name));
            all_success = false;
        }
    }
    
    return all_success;
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
    
    return true;
}

} // namespace mc 