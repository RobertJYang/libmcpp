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

#ifndef MC_LOG_FILE_APPENDER_H
#define MC_LOG_FILE_APPENDER_H

#include <mc/log/appenders/appender.h>
#include <fstream>
#include <mutex>
#include <string>

namespace mc {
namespace log {

/**
 * @brief 文件日志追加器配置
 */
struct file_appender_config : public appender_config {
    std::string m_filename;           // 日志文件名
    bool        m_truncate = false;   // 是否截断文件
    bool        m_flush_on_write = false; // 是否在每次写入后刷新
    
    file_appender_config() : appender_config("file") {}
    
    file_appender_config(std::string name, std::string filename, 
                         level lvl = level::info, 
                         bool truncate = false,
                         bool flush_on_write = false)
        : appender_config(std::move(name), lvl), 
          m_filename(std::move(filename)),
          m_truncate(truncate),
          m_flush_on_write(flush_on_write) {}
};

/**
 * @brief 文件日志追加器
 * 
 * 将日志消息输出到文件
 */
class file_appender : public appender {
public:
    /**
     * @brief 构造函数
     * 
     * @param config 文件追加器配置
     */
    explicit file_appender(file_appender_config config = file_appender_config())
        : appender(config), m_file_config(std::move(config)) {
        open_file();
    }
    
    /**
     * @brief 析构函数
     */
    ~file_appender() override {
        close_file();
    }
    
    /**
     * @brief 追加日志消息
     * 
     * @param msg 日志消息
     */
    void append(const message& msg) override {
        if (static_cast<int>(msg.get_level()) < static_cast<int>(m_config.m_level)) {
            return;
        }
        
        std::string formatted_msg;
        if (m_config.m_formatter) {
            formatted_msg = m_config.m_formatter->format(msg);
        } else {
            formatted_msg = msg.get_formatted_message();
        }
        
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_file.is_open()) {
            m_file << formatted_msg << std::endl;
            
            if (m_file_config.m_flush_on_write) {
                m_file.flush();
            }
        }
    }
    
    /**
     * @brief 设置日志文件名
     * 
     * @param filename 日志文件名
     */
    void set_filename(const std::string& filename) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_file_config.m_filename != filename) {
            close_file();
            m_file_config.m_filename = filename;
            open_file();
        }
    }
    
    /**
     * @brief 获取日志文件名
     * 
     * @return const std::string& 日志文件名
     */
    const std::string& get_filename() const {
        return m_file_config.m_filename;
    }
    
    /**
     * @brief 设置是否在每次写入后刷新
     * 
     * @param flush_on_write 是否在每次写入后刷新
     */
    void set_flush_on_write(bool flush_on_write) {
        m_file_config.m_flush_on_write = flush_on_write;
    }
    
    /**
     * @brief 获取是否在每次写入后刷新
     * 
     * @return bool 是否在每次写入后刷新
     */
    bool get_flush_on_write() const {
        return m_file_config.m_flush_on_write;
    }
    
    /**
     * @brief 刷新日志文件
     */
    void flush() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_file.is_open()) {
            m_file.flush();
        }
    }
    
private:
    /**
     * @brief 打开日志文件
     */
    void open_file() {
        if (!m_file_config.m_filename.empty()) {
            std::ios_base::openmode mode = std::ios::out;
            if (m_file_config.m_truncate) {
                mode |= std::ios::trunc;
            } else {
                mode |= std::ios::app;
            }
            
            m_file.open(m_file_config.m_filename, mode);
        }
    }
    
    /**
     * @brief 关闭日志文件
     */
    void close_file() {
        if (m_file.is_open()) {
            m_file.flush();
            m_file.close();
        }
    }
    
    file_appender_config m_file_config; // 文件追加器配置
    std::ofstream m_file;              // 文件流
    std::mutex m_mutex;                // 互斥锁，保证线程安全
};

} // namespace log
} // namespace mc

#endif // MC_LOG_FILE_APPENDER_H 