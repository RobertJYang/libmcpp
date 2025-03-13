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

#ifndef MC_FILE_LOG_BACKEND_H
#define MC_FILE_LOG_BACKEND_H

#include <mc/log/backends/log_backend.h>
#include <fstream>
#include <mutex>
#include <string>

namespace mc {
namespace log {

/**
 * @brief 文件日志后端
 * 
 * 将日志消息写入文件
 */
class file_log_backend : public log_backend {
public:
    /**
     * @brief 构造函数
     */
    file_log_backend() = default;
    
    /**
     * @brief 析构函数
     */
    ~file_log_backend() override {
        close();
    }
    
    /**
     * @brief 初始化日志后端
     * 
     * @param config 配置字符串，格式为文件路径
     * @return bool 初始化是否成功
     */
    bool init(const std::string& config) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // 关闭已打开的文件
        if (m_file.is_open()) {
            m_file.close();
        }
        
        // 打开新文件
        m_file_path = config;
        m_file.open(m_file_path, std::ios::out | std::ios::app);
        
        return m_file.is_open();
    }
    
    /**
     * @brief 写入日志消息
     * 
     * @param msg 日志消息
     */
    void write(const message& msg) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (m_file.is_open()) {
            m_file << msg.get_message() << std::endl;
        }
    }
    
    /**
     * @brief 刷新日志缓冲
     */
    void flush() override {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (m_file.is_open()) {
            m_file.flush();
        }
    }
    
    /**
     * @brief 关闭日志后端
     */
    void close() override {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (m_file.is_open()) {
            m_file.close();
        }
    }
    
    /**
     * @brief 获取后端名称
     * 
     * @return std::string 后端名称
     */
    std::string name() const override {
        return "file_log_backend";
    }
    
private:
    std::string m_file_path;  // 文件路径
    std::ofstream m_file;     // 文件流
    std::mutex m_mutex;       // 互斥锁
};

} // namespace log
} // namespace mc

#endif // MC_FILE_LOG_BACKEND_H 