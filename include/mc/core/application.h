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

#ifndef MC_CORE_APPLICATION_H
#define MC_CORE_APPLICATION_H

#include "mc/core/module.h"
#include "mc/core/service.h"
#include "mc/core/supervisor.h"
#include "mc/core/priority_queue.h"
#include <boost/asio.hpp>
#include <filesystem>
#include <memory>
#include <string>

namespace mc {
namespace fs = std::filesystem;

/**
 * @brief 应用程序类，单例模式
 */
class application {
public:
    using io_context_type        = boost::asio::io_context;
    using priority_executor_type = priority_queue_executor<io_context_type>;
    using work_guard_type        = boost::asio::executor_work_guard<io_context_type::executor_type>;

    /**
     * @brief 获取应用程序单例实例
     * @return 应用程序实例引用
     */
    static application& instance();

    /**
     * @brief 析构函数
     */
    ~application();

    /**
     * @brief 设置应用程序版本号
     * @param version 版本号
     */
    void set_version(const std::string& version);

    /**
     * @brief 获取应用程序版本号
     * @return 版本号
     */
    const std::string& version() const;

    /**
     * @brief 设置配置文件目录
     * @param config_dir 配置文件目录路径
     */
    void set_config_dir(const fs::path& config_dir);

    /**
     * @brief 获取配置文件目录
     * @return 配置文件目录路径
     */
    const fs::path& config_dir() const;

    /**
     * @brief 设置模块目录
     * @param module_dir 模块目录路径
     */
    void set_module_dir(const fs::path& module_dir);

    /**
     * @brief 获取模块目录
     * @return 模块目录路径
     */
    const fs::path& module_dir() const;

    /**
     * @brief 注册模块
     * @param module 模块指针
     * @return 应用程序实例引用，用于链式调用
     */
    application& register_module(module_ptr module);

    /**
     * @brief 查找模块
     * @param name 模块名称
     * @return 模块指针，如果未找到则返回nullptr
     */
    module* find_module(const std::string& name) const;

    /**
     * @brief 加载模块
     * @param name 模块名称
     * @return 是否成功加载模块
     */
    bool load_module(const std::string& name);

    /**
     * @brief 卸载模块
     * @param name 模块名称
     * @return 是否成功卸载模块
     */
    bool unload_module(const std::string& name);

    /**
     * @brief 注册服务
     * @param type 服务类型
     * @param factory 服务工厂函数
     * @return 是否成功注册服务
     */
    bool register_service(const std::string& type, std::function<service_ptr()> factory);

    /**
     * @brief 创建服务
     * @param type 服务类型
     * @param config 服务配置
     * @return 服务指针
     */
    service_ptr create_service(const std::string& type, const service_config& config);

    /**
     * @brief 获取服务
     * @param name 服务名称
     * @return 服务指针
     */
    service_ptr get_service(const std::string& name) const;

    /**
     * @brief 获取根监督器
     * @return 根监督器指针
     */
    supervisor_ptr get_root_supervisor() const;

    /**
     * @brief 创建监督器
     * @param config 监督器配置
     * @return 监督器指针
     */
    supervisor_ptr create_supervisor(const supervisor_config& config);

    /**
     * @brief 初始化所有已注册的模块
     * @return 应用程序实例引用，用于链式调用
     */
    application& initialize();

    /**
     * @brief 使用命令行参数初始化应用程序和模块
     * @param argc 命令行参数数量
     * @param argv 命令行参数数组
     * @return 应用程序实例引用，用于链式调用
     */
    application& initialize(int argc, char** argv);

    /**
     * @brief 启动应用程序
     *
     * 此函数执行以下操作：
     * 1. 启动所有已初始化的模块
     * 2. 准备IO上下文和工作线程
     *
     * 注意：此函数不会阻塞，如需阻塞等待应用程序结束，请调用exec()
     *
     * @return 应用程序实例引用，用于链式调用
     */
    application& start();

    /**
     * @brief 执行应用程序并阻塞直到应用程序结束
     *
     * 此函数会阻塞当前线程，运行事件循环直到应用程序被停止
     */
    void exec();

    /**
     * @brief 停止应用程序
     *
     * 此函数执行以下操作：
     * 1. 标记应用程序为停止状态
     * 2. 停止IO上下文和事件循环
     *
     * 注意：此函数不会等待资源清理完成，如需完全清理，请调用cleanup()
     */
    void stop();

    /**
     * @brief 清理应用程序资源
     *
     * 此函数执行以下操作：
     * 1. 停止所有模块和服务
     * 2. 清理所有资源
     *
     * 注意：此函数通常在析构函数中自动调用，除非有特殊需求，否则不需要手动调用
     */
    void cleanup();

    /**
     * @brief 检查应用程序是否已停止
     * @return 如果应用程序已停止，返回true；否则返回false
     */
    bool is_stopped() const;

    /**
     * @brief 获取IO上下文
     * @return IO上下文引用
     */
    io_context_type& get_io_context();

    /**
     * @brief 获取优先级队列执行器
     * @return 优先级队列执行器引用
     */
    priority_executor_type& get_priority_executor();

    /**
     * @brief 提交任务到优先级队列
     * @param f 任务函数
     * @param p 任务优先级，默认为中优先级
     */
    template <typename Function>
    void post(Function&& f, int p = priority::normal) {
        get_priority_executor().execute(std::forward<Function>(f), p);
    }

private:
    /**
     * @brief 构造函数（私有，通过instance()获取实例）
     */
    application();

    class impl;
    std::unique_ptr<impl> pimpl_;
};

inline application& app() {
    return application::instance();
}

} // namespace mc

#endif // MC_CORE_APPLICATION_H