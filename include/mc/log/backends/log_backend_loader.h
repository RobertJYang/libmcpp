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

#ifndef MC_LOG_BACKEND_LOADER_H
#define MC_LOG_BACKEND_LOADER_H

#include <mc/log/backends/log_backend.h>
#include <string>
#include <memory>
#include <unordered_map>
#include <dlfcn.h>

namespace mc {
namespace log {

/**
 * @brief 日志后端加载器
 * 
 * 负责从动态库加载日志后端
 */
class log_backend_loader {
public:
    /**
     * @brief 获取单例实例
     * 
     * @return log_backend_loader& 单例实例引用
     */
    static log_backend_loader& instance() {
        static log_backend_loader loader;
        return loader;
    }
    
    /**
     * @brief 从动态库加载日志后端
     * 
     * @param lib_path 动态库路径
     * @param config 配置字符串
     * @return std::shared_ptr<log_backend> 日志后端指针
     */
    std::shared_ptr<log_backend> load(const std::string& lib_path, const std::string& config = "") {
        // 检查是否已加载
        auto it = m_backends.find(lib_path);
        if (it != m_backends.end()) {
            return it->second;
        }
        
        // 加载动态库
        void* handle = dlopen(lib_path.c_str(), RTLD_LAZY);
        if (!handle) {
            return nullptr;
        }
        
        // 获取创建函数
        auto creator = reinterpret_cast<log_backend_creator_c>(dlsym(handle, "create_log_backend_c"));
        if (!creator) {
            dlclose(handle);
            return nullptr;
        }
        
        // 获取销毁函数
        auto destroyer = reinterpret_cast<log_backend_destroyer_c>(dlsym(handle, "destroy_log_backend_c"));
        if (!destroyer) {
            dlclose(handle);
            return nullptr;
        }
        
        // 创建后端实例
        void* raw_ptr = creator();
        if (!raw_ptr) {
            dlclose(handle);
            return nullptr;
        }
        
        // 转换为 C++ 智能指针
        auto shared_ptr_ptr = static_cast<std::shared_ptr<log_backend>*>(raw_ptr);
        auto backend = *shared_ptr_ptr;
        
        // 存储库句柄和销毁函数
        m_handles[lib_path] = std::make_pair(handle, destroyer);
        m_raw_ptrs[lib_path] = raw_ptr;
        
        // 初始化后端
        if (!backend->init(config)) {
            unload(lib_path);
            return nullptr;
        }
        
        // 保存后端实例
        m_backends[lib_path] = backend;
        
        return backend;
    }
    
    /**
     * @brief 卸载日志后端
     * 
     * @param lib_path 动态库路径
     */
    void unload(const std::string& lib_path) {
        // 检查是否已加载
        auto backend_it = m_backends.find(lib_path);
        if (backend_it == m_backends.end()) {
            return;
        }
        
        // 关闭后端
        backend_it->second->close();
        
        // 销毁 C++ 封装
        auto handle_it = m_handles.find(lib_path);
        auto raw_ptr_it = m_raw_ptrs.find(lib_path);
        if (handle_it != m_handles.end() && raw_ptr_it != m_raw_ptrs.end()) {
            // 释放原始指针
            auto& [handle, destroyer] = handle_it->second;
            destroyer(raw_ptr_it->second);
            
            // 卸载库
            dlclose(handle);
        }
        
        // 移除记录
        m_backends.erase(backend_it);
        m_handles.erase(lib_path);
        m_raw_ptrs.erase(lib_path);
    }
    
    /**
     * @brief 析构函数
     */
    ~log_backend_loader() {
        for (auto& pair : m_backends) {
            pair.second->close();
        }
        
        for (auto& [lib_path, handle_pair] : m_handles) {
            auto& [handle, destroyer] = handle_pair;
            destroyer(m_raw_ptrs[lib_path]);
            dlclose(handle);
        }
        
        m_backends.clear();
        m_handles.clear();
        m_raw_ptrs.clear();
    }
    
private:
    log_backend_loader() = default;
    log_backend_loader(const log_backend_loader&) = delete;
    log_backend_loader& operator=(const log_backend_loader&) = delete;
    
    std::map<std::string, std::shared_ptr<log_backend>> m_backends;
    std::map<std::string, std::pair<void*, log_backend_destroyer_c>> m_handles;
    std::map<std::string, void*> m_raw_ptrs;
};

} // namespace log
} // namespace mc

#endif // MC_LOG_BACKEND_LOADER_H 