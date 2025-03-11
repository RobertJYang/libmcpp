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

#include <mc/log/log_backend.h>
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
        auto creator = reinterpret_cast<log_backend_creator>(dlsym(handle, "create_log_backend"));
        if (!creator) {
            dlclose(handle);
            return nullptr;
        }
        
        // 创建后端实例
        auto backend = creator();
        if (!backend) {
            dlclose(handle);
            return nullptr;
        }
        
        // 初始化后端
        if (!backend->init(config)) {
            dlclose(handle);
            return nullptr;
        }
        
        // 保存后端和句柄
        m_backends[lib_path] = backend;
        m_handles[lib_path] = handle;
        
        return backend;
    }
    
    /**
     * @brief 卸载日志后端
     * 
     * @param lib_path 动态库路径
     */
    void unload(const std::string& lib_path) {
        auto it = m_backends.find(lib_path);
        if (it != m_backends.end()) {
            it->second->close();
            m_backends.erase(it);
        }
        
        auto handle_it = m_handles.find(lib_path);
        if (handle_it != m_handles.end()) {
            dlclose(handle_it->second);
            m_handles.erase(handle_it);
        }
    }
    
    /**
     * @brief 析构函数
     */
    ~log_backend_loader() {
        for (auto& pair : m_backends) {
            pair.second->close();
        }
        
        for (auto& pair : m_handles) {
            dlclose(pair.second);
        }
    }
    
private:
    log_backend_loader() = default;
    log_backend_loader(const log_backend_loader&) = delete;
    log_backend_loader& operator=(const log_backend_loader&) = delete;
    
    std::unordered_map<std::string, std::shared_ptr<log_backend>> m_backends; // 后端映射
    std::unordered_map<std::string, void*> m_handles; // 动态库句柄映射
};

} // namespace log
} // namespace mc

#endif // MC_LOG_BACKEND_LOADER_H 