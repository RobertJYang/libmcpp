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
namespace po = boost::program_options;

// 示例服务实现
class example_service : public mc::core::service_base {
public:
    // 构造函数
    example_service(const std::string& name) : mc::core::service_base(name) {
    }

    bool init(dict args) override {
        set_state(mc::core::service_state::starting);

        // 设置服务名称
        if (args.contains("name")) {
            set_name(args.at("name").as<std::string>());
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
            set_dependencies(args.at("dependencies").as<std::vector<std::string>>());
        }

        ilog("init example service: ${name}", ("name", name()));
        set_state(mc::core::service_state::stopped);
        return true;
    }

    bool start() override {
        if (get_state() == mc::core::service_state::running) {
            return true;
        }

        set_state(mc::core::service_state::starting);

        // 先设置状态，再输出日志
        for (int32_t i = 0; i < m_repeat_count; ++i) {
            ilog("${message} [${count}/${total}]",
                 ("message", m_message)("count", i + 1)("total", m_repeat_count));
        }

        // 在设置完状态后，再输出带有监督器信息的日志
        ilog("start example service: ${name}", ("name", name()));

        set_state(mc::core::service_state::running);
        return true;
    }

    bool stop() override {
        if (get_state() != mc::core::service_state::running) {
            return true;
        }

        set_state(mc::core::service_state::stopping);

        // 先设置状态，再输出日志
        ilog("stop example service: ${name}", ("name", name()));

        set_state(mc::core::service_state::stopped);
        return true;
    }

    void cleanup() override {
        ilog("cleanup example service: ${name}", ("name", name()));
    }

    bool is_healthy() const override {
        return get_state() == mc::core::service_state::running;
    }

    struct register_options {
        void operator()(po::options_description& cli_opts, po::options_description& cfg_opts) {
            cfg_opts.add_options()(
                "example.message",
                po::value<std::string>()->default_value("Hello from Example Service!"),
                "example service message")("example.repeat_count",
                                           po::value<int32_t>()->default_value(3),
                                           "example service repeat count");
        }
    };

private:
    std::string m_message      = "Hello from Example Service!";
    int32_t     m_repeat_count = 3;
};

// 示例插件实现
class example_plugin : public mc::core::plugin {
public:
    // 插件信息
    const mc::core::plugin_info& get_info() const override {
        static mc::core::plugin_info info{
            "example_plugin", // 名称
            "1.0.0",          // 版本
            {}                // 依赖
        };
        return info;
    }

    // 插件初始化方法
    bool init(mc::core::service_factory& factory) override {
        ilog("init example plugin");

        // 注册服务类型
        factory.register_service<example_service>("example");

        return true;
    }
};

// 导出插件
MC_EXPORT_PLUGIN(example_plugin)