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

#include <mc/log/appender.h>
#include <mc/log/appender_factory.h>
#include <mc/log/logger.h>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace mc {
namespace log {

/**
 * @brief 日志管理器
 *
 * 负责管理日志记录器和追加器
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

        auto it = m_loggers.find(name);
        if (it != m_loggers.end()) {
            return it->second;
        }

        logger new_logger(name);
        m_loggers[name] = new_logger;
        return new_logger;
    }

    /**
     * @brief 加载追加器
     *
     * @param lib_path 动态库路径
     * @param appender_name 追加器名称
     * @return bool 是否成功加载
     */
    bool load_appender(const std::string& lib_path, const std::string& appender_name) {
        return appender_factory::instance().load(lib_path, appender_name);
    }

    /**
     * @brief 从目录加载所有追加器
     *
     * @param dir_path 目录路径
     */
    void load_appenders(const std::string& dir_path) {
        appender_factory::instance().load_all(dir_path);
    }

    /**
     * @brief 创建追加器
     *
     * @param name 追加器名称
     * @return appender_ptr 追加器指针
     */
    template<typename T>
    std::shared_ptr<T> create_appender(const std::string& name) {
        return appender_factory::instance().create<T>(name);
    }

private:
    log_manager()                              = default;
    log_manager(const log_manager&)            = delete;
    log_manager& operator=(const log_manager&) = delete;

    std::unordered_map<std::string, logger> m_loggers; // 日志记录器映射
    std::mutex                              m_mutex;   // 互斥锁
};

} // namespace log
} // namespace mc

#endif // MC_LOG_MANAGER_H