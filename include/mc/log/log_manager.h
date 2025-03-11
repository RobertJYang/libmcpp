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

#ifndef MC_LOG_MANAGER_H
#define MC_LOG_MANAGER_H

#include <mc/log/logger.h>
#include <mc/log/log_backend.h>
#include <mc/log/log_backend_loader.h>
#include <mc/log/log_backend_adapter.h>
#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>

namespace mc {
namespace log {

/**
 * @brief 日志管理器
 * 
 * 负责管理日志记录器和后端
 */
class log_manager {
public:
    /**
     * @brief 获取单例实例
     * 
     * @return log_manager& 单例实例引用
     */
    static log_manager& instance() {
        static log_manager manager;
        return manager;
    }
    
    /**
     * @brief 获取日志记录器
     * 
     * @param name 日志记录器名称
     * @return logger 日志记录器
     */
    logger get_logger(const std::string& name = DEFAULT_LOGGER) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // 检查是否已存在
        auto it = m_loggers.find(name);
        if (it != m_loggers.end()) {
            return it->second;
        }
        
        // 创建新的日志记录器
        logger new_logger(name);
        m_loggers[name] = new_logger;
        return new_logger;
    }
    
    /**
     * @brief 加载日志后端
     * 
     * @param lib_path 动态库路径
     * @param config 配置字符串
     * @return std::shared_ptr<log_backend> 日志后端指针
     */
    std::shared_ptr<log_backend> load_backend(const std::string& lib_path, const std::string& config = "") {
        return log_backend_loader::instance().load(lib_path, config);
    }
    
    /**
     * @brief 卸载日志后端
     * 
     * @param lib_path 动态库路径
     */
    void unload_backend(const std::string& lib_path) {
        log_backend_loader::instance().unload(lib_path);
    }
    
    /**
     * @brief 创建后端适配器
     * 
     * 将日志后端包装为追加器，以便添加到日志记录器
     * 
     * @param backend 日志后端
     * @return std::shared_ptr<appender> 追加器指针
     */
    std::shared_ptr<appender> create_backend_adapter(std::shared_ptr<log_backend> backend) {
        if (!backend) {
            return nullptr;
        }
        return std::make_shared<backend_adapter>(std::move(backend));
    }
    
private:
    log_manager() = default;
    log_manager(const log_manager&) = delete;
    log_manager& operator=(const log_manager&) = delete;
    
    std::unordered_map<std::string, logger> m_loggers; // 日志记录器映射
    std::mutex m_mutex; // 互斥锁
};

} // namespace log
} // namespace mc

#endif // MC_LOG_MANAGER_H 