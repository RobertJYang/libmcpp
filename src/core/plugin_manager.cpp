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

#include <mc/core/plugin_manager.h>
#include <mc/filesystem.h>
#include <mc/log.h>

#include <dlfcn.h>

namespace mc::core {

plugin_manager::plugin_manager() = default;

plugin_manager::~plugin_manager() {
    unload_all_plugins();
}

void plugin_manager::set_plugin_dir(const std::string& dir) {
    m_plugin_dir = dir;
    dlog("set plugin dir: ${dir}", ("dir", dir));
}

const std::string& plugin_manager::plugin_dir() const {
    return m_plugin_dir;
}

bool plugin_manager::register_plugin(std::shared_ptr<plugin> plugin) {
    if (!plugin) {
        elog("cannot register null plugin");
        return false;
    }

    const auto& info = plugin->get_info();
    const auto& name = info.name;

    if (m_plugins.find(name) != m_plugins.end()) {
        elog("plugin '${name}' already registered", ("name", name));
        return false;
    }

    dlog("register plugin '${name}', version '${version}'",
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
        dlog("plugin '${name}' already loaded", ("name", name));
        return true;
    }

    void*                   handle = nullptr;
    std::shared_ptr<plugin> plugin;

    if (!load_dynamic_library(name, handle, plugin)) {
        return false;
    }

    m_plugin_handles.push_back(handle);
    return register_plugin(std::move(plugin));
}

bool plugin_manager::load_plugins(const std::vector<std::string>& plugin_names) {
    if (m_plugin_dir.empty()) {
        elog("plugin dir not set");
        return false;
    }

    // 如果指定了插件名称，只加载指定的插件
    if (!plugin_names.empty()) {
        for (const auto& name : plugin_names) {
            void*                   handle = nullptr;
            std::shared_ptr<plugin> plugin;
            if (!load_dynamic_library(name, handle, plugin)) {
                wlog("load plugin failed: ${name}", ("name", name));
                continue;
            }
            m_plugin_handles.push_back(handle);
            if (!register_plugin(std::move(plugin))) {
                wlog("register plugin failed: ${name}", ("name", name));
            }
        }
        return true;
    }

    // 否则加载目录下的所有插件
    try {
        for (const auto& entry : mc::filesystem::directory_iterator(m_plugin_dir)) {
            if (entry.path().extension() == ".so") {
                std::string name = entry.path().stem().string();
                if (name.substr(0, 3) == "lib") {
                    name = name.substr(3); // 移除 "lib" 前缀
                }
                void*                   handle = nullptr;
                std::shared_ptr<plugin> plugin;
                if (!load_dynamic_library(name, handle, plugin)) {
                    wlog("load plugin failed: ${path}", ("path", entry.path().string()));
                    continue;
                }
                m_plugin_handles.push_back(handle);
                if (!register_plugin(std::move(plugin))) {
                    wlog("register plugin failed: ${name}", ("name", name));
                }
            }
        }
        return true;
    } catch (const mc::filesystem::filesystem_error& e) {
        elog("traverse plugin dir failed: ${error}", ("error", e.what()));
        return false;
    }
}

bool plugin_manager::unload_plugin(const std::string& name) {
    auto it = m_plugins.find(name);
    if (it == m_plugins.end()) {
        wlog("plugin '${name}' not found, cannot unload", ("name", name));
        return false;
    }

    // 移除
    m_plugins.erase(it);

    // 注意：不卸载动态库，因为不容易找到对应的句柄
    // 在析构函数中统一卸载所有动态库

    dlog("unload plugin: ${name}", ("name", name));
    return true;
}

void plugin_manager::unload_all_plugins() {
    dlog("unload all plugins");
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

    for (auto& pair : m_plugins) {
        const auto& name   = pair.first;
        auto&       plugin = pair.second;
        if (!plugin->init(factory)) {
            elog("init plugin failed: ${name}", ("name", name));
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

bool plugin_manager::load_dynamic_library(const std::string& plugin_name, void*& handle,
                                          std::shared_ptr<plugin>& plugin) {
    // 尝试加载动态库
    std::string lib_path = mc::filesystem::join(m_plugin_dir, "lib" + plugin_name + ".so");

    if (!mc::filesystem::exists(lib_path)) {
        elog("plugin library file not found: ${path}", ("path", lib_path));
        return false;
    }

    // 加载动态库
    handle = dlopen(lib_path.c_str(), RTLD_LAZY | RTLD_LOCAL);
    if (!handle) {
        elog("load dynamic library failed: ${path}, error: ${error}",
             ("path", lib_path)("error", dlerror()));
        return false;
    }

    // 查找创建函数
    auto create_func = (create_plugin_func)dlsym(handle, "create_plugin");
    if (!create_func) {
        elog("cannot find plugin create function: ${error}", ("error", dlerror()));
        dlclose(handle);
        handle = nullptr;
        return false;
    }

    // 查找销毁函数
    auto destroy_func = (destroy_plugin_func)dlsym(handle, "destroy_plugin");
    if (!destroy_func) {
        elog("cannot find plugin destroy function: ${error}", ("error", dlerror()));
        dlclose(handle);
        handle = nullptr;
        return false;
    }

    // 创建插件实例
    mc::core::plugin* raw_plugin = create_func();
    if (!raw_plugin) {
        elog("create plugin instance failed");
        dlclose(handle);
        handle = nullptr;
        return false;
    }

    // 创建智能指针，使用自定义删除器
    plugin = std::shared_ptr<mc::core::plugin>(raw_plugin, [destroy_func](mc::core::plugin* p) {
        destroy_func(p);
    });

    return true;
}

} // namespace mc::core