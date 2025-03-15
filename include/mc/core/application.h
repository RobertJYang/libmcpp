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

#ifndef MC_APPLICATION_H
#define MC_APPLICATION_H

#include "mc/core/module.h"
#include "mc/core/service.h"
#include "mc/core/supervisor.h"
#include "mc/filesystem.h"
#include <boost/asio.hpp>
#include <boost/program_options.hpp>
#include <memory>
#include <string>

namespace mc {

namespace po = boost::program_options;

/**
 * @brief 服务类型信息结构体
 */
struct service_type_info {
    std::function<service_ptr()> factory;                                                    // 服务工厂函数
    std::function<void(po::options_description&, po::options_description&)> register_options;  // 配置选项注册函数
};

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
    static application& instance();

    // 禁止拷贝和移动
    application(const application&) = delete;
    application& operator=(const application&) = delete;
    application(application&&) = delete;
    application& operator=(application&&) = delete;

    // 析构函数
    ~application();

    // 版本和配置管理
    void set_version(const std::string& version);
    const std::string& version() const;
    void set_config_dir(const std::string& config_dir);
    const std::string& config_dir() const;
    void set_module_dir(const std::string& module_dir);
    const std::string& module_dir() const;

    // 模块管理
    application& register_module(std::shared_ptr<class module> module);
    class module* find_module(const std::string& name) const;

    // 服务管理
    bool register_service(const std::string& type, 
                         std::function<service_ptr()> factory,
                         std::function<void(po::options_description&, po::options_description&)> register_options);
    service_ptr create_service(const std::string& type, const service_config& config);
    service_ptr get_service(const std::string& name) const;

    // 监督器管理
    supervisor_ptr get_root_supervisor() const;
    supervisor_ptr create_supervisor(const supervisor_config& config);

    // 应用程序生命周期
    application& initialize();
    application& initialize(int argc, char** argv);
    application& start();
    void exec();
    void stop();
    void cleanup();
    bool is_stopped() const;

    // IO上下文和执行器
    io_context_type& get_io_context();
    strand_type& get_strand();

private:
    // 私有构造函数
    application();

    // 实现类
    class impl;
    std::unique_ptr<impl> pimpl_;
};

// 全局访问函数
inline application& app() {
    return application::instance();
}

} // namespace mc

#endif // MC_APPLICATION_H