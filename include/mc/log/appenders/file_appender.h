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

#include <fstream>
#include <mc/log/appender.h>
#include <mutex>
#include <string>

namespace mc {
namespace log {

/**
 * @brief 文件日志追加器配置
 */
struct file_appender_config {
    std::string name;                   // 追加器名称
    std::string filename;               // 日志文件名
    bool        truncate       = false; // 是否截断文件
    bool        flush_on_write = false; // 是否在每次写入后刷新

    file_appender_config() = default;

    file_appender_config(std::string name, std::string filename, bool truncate = false,
                         bool flush_on_write = false)
        : name(std::move(name)), filename(std::move(filename)), truncate(truncate),
          flush_on_write(flush_on_write) {
    }
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
    explicit file_appender();

    /**
     * @brief 析构函数
     */
    ~file_appender() override;

    /**
     * @brief 初始化追加器
     *
     * @param args 配置参数
     * @return bool 初始化是否成功
     */
    bool init(const variant& args) override;

    /**
     * @brief 追加日志消息
     *
     * @param msg 日志消息
     */
    void append(const message& msg) override;

    /**
     * @brief 设置日志文件名
     *
     * @param filename 日志文件名
     */
    void set_filename(const std::string& filename);

    /**
     * @brief 获取日志文件名
     *
     * @return const std::string& 日志文件名
     */
    const std::string& get_filename() const;

    /**
     * @brief 设置是否在每次写入后刷新
     *
     * @param flush_on_write 是否在每次写入后刷新
     */
    void set_flush_on_write(bool flush_on_write);

    /**
     * @brief 获取是否在每次写入后刷新
     *
     * @return bool 是否在每次写入后刷新
     */
    bool get_flush_on_write() const;

    /**
     * @brief 刷新日志文件
     */
    void flush();

    // 添加debug_log_ptr的setter
    static void set_debug_log_ptr(void* func_ptr);

private:
    /**
     * @brief 打开日志文件
     */
    void open_file();

    /**
     * @brief 关闭日志文件
     */
    void close_file();

    file_appender_config m_file_config; // 文件追加器配置
    std::ofstream        m_file;        // 文件流
    std::mutex           m_mutex;       // 互斥锁，保证线程安全
};

} // namespace log
} // namespace mc

#endif // MC_LOG_FILE_APPENDER_H