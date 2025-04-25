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
 * @file example_plugin.cpp
 * @brief 示例插件实现
 */

#include "mc/core/plugin.h"
#include "mc/core/service.h"
#include "mc/core/service_factory.h"
#include "mc/core/supervisor.h"
#include "mc/log.h"
#include <boost/program_options.hpp>

using namespace mc;

// 示例服务实现
class example_service : public service {
public:
    // 构造函数
    example_service(const std::string& name) : service(name) {
        m_name = name;
    }

    bool init(dict args) override {
        set_state(service_state::starting);

        // 设置服务名称
        if (args.contains("name")) {
            m_name = args.at("name").as<std::string>();
        }

        // 设置服务消息
        m_message = "Hello from Example Service!";
        if (args.contains("message")) {
            m_message = args.at("message").as<std::string>();
        }

        // 设置重复次数
        if (args.contains("repeat_count")) {
            m_repeat_count = args.at("repeat_count").as<int32_t>();
        }

        // 从配置中获取依赖关系
        if (args.contains("dependencies")) {
            m_dependencies = args.at("dependencies").as<std::vector<std::string>>();
        }

        ilog("初始化示例服务: ${name}", ("name", name()));
        set_state(service_state::stopped);
        return true;
    }

    bool start() override {
        if (get_state() == service_state::running) {
            return true;
        }

        set_state(service_state::starting);

        // 先设置状态，再输出日志
        for (int32_t i = 0; i < m_repeat_count; ++i) {
            ilog("${message} [${count}/${total}]",
                 ("message", m_message)("count", i + 1)("total", m_repeat_count));
        }

        // 在设置完状态后，再输出带有监督器信息的日志
        ilog("启动示例服务: ${name}, 监督器: ${supervisor}",
             ("name", name())("supervisor", get_supervisor_name()));

        set_state(service_state::running);
        return true;
    }

    bool stop() override {
        if (get_state() != service_state::running) {
            return true;
        }

        set_state(service_state::stopping);

        // 先设置状态，再输出日志
        ilog("停止示例服务: ${name}, 监督器: ${supervisor}",
             ("name", name())("supervisor", get_supervisor_name()));

        set_state(service_state::stopped);
        return true;
    }

    void cleanup() override {
        // 先设置状态，再输出日志
        ilog("清理示例服务: ${name}, 监督器: ${supervisor}",
             ("name", name())("supervisor", get_supervisor_name()));
    }

    bool is_healthy() const override {
        return get_state() == service_state::running;
    }

    // 获取监督器名称
    std::string get_supervisor_name() const {
        auto supervisor = get_supervisor();
        if (supervisor) {
            return supervisor->name();
        }
        return "unknown";
    }

    // 获取服务名称
    const std::string& name() const override {
        return m_name;
    }

    // 获取服务状态
    service_state get_state() const override {
        return m_state;
    }

    // 设置监督器
    void set_supervisor(std::shared_ptr<supervisor> supervisor) override {
        m_supervisor = supervisor;
    }

    // 获取监督器
    std::shared_ptr<supervisor> get_supervisor() const override {
        return m_supervisor;
    }

    // 获取服务依赖关系
    const std::vector<std::string>& get_dependencies() const override {
        return m_dependencies;
    }

    struct register_options {
        void operator()(po::options_description& cli_opts, po::options_description& cfg_opts) {
            cfg_opts.add_options()(
                "example.message",
                po::value<std::string>()->default_value("Hello from Example Service!"),
                "示例服务的消息文本")("example.repeat_count",
                                      po::value<int32_t>()->default_value(3),
                                      "示例服务的消息重复次数");
        }
    };

protected:
    // 设置服务状态
    void set_state(service_state state) {
        m_state = state;
    }

private:
    std::string                 m_name;
    service_state               m_state;
    std::shared_ptr<supervisor> m_supervisor;
    std::string                 m_message      = "Hello from Example Service!";
    int32_t                     m_repeat_count = 3;
    std::vector<std::string>    m_dependencies;
};

// 示例插件实现
class example_plugin : public plugin {
public:
    // 插件信息
    const plugin_info& get_info() const override {
        static plugin_info info{
            "example_plugin", // 名称
            "1.0.0",          // 版本
            {}                // 依赖
        };
        return info;
    }

    // 插件初始化方法
    bool init(service_factory& factory) override {
        ilog("初始化示例插件");

        // 注册服务类型
        factory.register_service<example_service>("example");

        return true;
    }
};

// 导出插件
MC_EXPORT_PLUGIN(example_plugin)