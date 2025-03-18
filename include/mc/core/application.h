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

/**
 * @file application.h
 * @brief 应用程序类，作为核心类，协调各个管理器
 */
#ifndef MC_APPLICATION_H
#define MC_APPLICATION_H

#include "mc/core/config_manager.h"
#include "mc/core/plugin_manager.h"
#include "mc/core/service_factory.h"
#include "mc/core/service_manager.h"
#include "mc/core/supervisor_manager.h"
#include "mc/singleton.h"
#include <boost/asio.hpp>
#include <memory>
#include <string>
#include <unordered_map>

namespace mc {

/**
 * @brief 应用程序类
 */
class application {
public:
    // 类型定义
    using io_context_type = boost::asio::io_context;
    using executor_type = io_context_type::executor_type;
    using strand_type = boost::asio::strand<executor_type>;
    using work_guard_type = boost::asio::executor_work_guard<executor_type>;

    // 单例访问
    static application& instance() {
        // 使用自定义创建函数访问单例
        static auto creator = []() { return new application(); };
        return singleton<application>::instance_with_creator(creator);
    }
    
    // 测试专用：重置单例状态
    static void reset_for_test() {
        // 直接调用全局函数重置所有单例
        mc::reset_singletons_for_test();
    }

    // 禁止拷贝和移动
    application(const application&) = delete;
    application& operator=(const application&) = delete;
    application(application&&) = delete;
    application& operator=(application&&) = delete;

    // 析构函数
    ~application();

    // 版本管理
    void set_version(const std::string& version);
    const std::string& version() const;

    // 各个管理器的访问接口
    plugin_manager& get_plugin_manager();
    service_factory& get_service_factory();
    service_manager& get_service_manager();
    config_manager& get_config_manager();
    supervisor_manager& get_supervisor_manager();

    // 应用程序生命周期
    bool initialize();
    bool initialize(int argc, char** argv);
    application& start();
    void exec();
    void stop();
    void cleanup();
    bool is_stopped() const;

    // IO上下文和执行器
    io_context_type& get_io_context();
    strand_type& get_strand();

    // 重启所有服务
    void restart_all_services();

private:
    // 让singleton能够访问私有构造函数
    friend class detail::singleton_impl<application>;

    // 私有构造函数
    application();

    // 成员变量
    std::string m_version;                                // 应用程序版本号
    std::unique_ptr<plugin_manager> m_plugin_manager;     // 插件管理器
    std::unique_ptr<service_factory> m_service_factory;   // 服务工厂
    std::unique_ptr<service_manager> m_service_manager;   // 服务管理器
    std::unique_ptr<config_manager> m_config_manager;     // 配置管理器
    std::unique_ptr<supervisor_manager> m_supervisor_manager; // 监督器管理器
    std::unordered_map<std::string, std::shared_ptr<supervisor>> m_supervisors; // 监督器映射表
    
    // IO相关
    io_context_type m_io_context;                         // IO上下文
    strand_type m_strand;                                 // 执行器
    work_guard_type m_work_guard;                         // 工作守卫（单个即可）
    unsigned int m_thread_count;                          // 线程数量
    bool m_stopped;                                       // 停止标志

    // 加载插件
    bool load_plugins(bool config_loaded);
    
    // 初始化监督器
    bool initialize_supervisors(bool config_loaded);
    
    // 初始化服务
    bool initialize_services(bool config_loaded);
    
    // 停止所有服务
    void stop_all_services();
};

// 全局访问函数
inline application& app() {
    return application::instance();
}

} // namespace mc

#endif // MC_APPLICATION_H