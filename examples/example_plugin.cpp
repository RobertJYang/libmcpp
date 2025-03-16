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
#include "mc/log.h"
#include <boost/program_options.hpp>

using namespace mc;
namespace po = boost::program_options;

// 示例服务实现
class example_service : public service_base<example_service> {
public:
    explicit example_service(std::string name = "") : service_base<example_service>(std::move(name)) {}
    
    bool init(dict args) override {
        set_state(service_state::starting);
        
        m_message = "Hello from Example Service!";
        if (args.contains("message")) {
            m_message = args.at("message").as<std::string>();
        }
        
        if (args.contains("repeat_count")) {
            m_repeat_count = args.at("repeat_count").as<int>();
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
        ilog("启动示例服务: ${name}", ("name", name()));
        
        for (int i = 0; i < m_repeat_count; ++i) {
            ilog("${message} [${count}/${total}]", 
                 ("message", m_message)("count", i + 1)("total", m_repeat_count));
        }
        
        set_state(service_state::running);
        return true;
    }
    
    bool stop() override {
        if (get_state() != service_state::running) {
            return true;
        }
        
        set_state(service_state::stopping);
        ilog("停止示例服务: ${name}", ("name", name()));
        set_state(service_state::stopped);
        return true;
    }
    
    void cleanup() override {
        ilog("清理示例服务: ${name}", ("name", name()));
    }
    
    bool is_healthy() const override {
        return get_state() == service_state::running;
    }
    
    // 注册配置选项
    static void register_options(po::options_description& cli_opts, po::options_description& cfg_opts) {
        cfg_opts.add_options()
            ("example.message", po::value<std::string>()->default_value("Hello from Example Service!"), 
             "示例服务的消息文本")
            ("example.repeat_count", po::value<int>()->default_value(3), 
             "示例服务的消息重复次数");
    }
    
private:
    std::string m_message = "Hello from Example Service!";
    int m_repeat_count = 3;
};

// 示例插件实现
class example_plugin : public plugin_base<example_plugin> {
public:
    // 插件信息
    const plugin_info& get_info() const override {
        static plugin_info info {
            "example_plugin",  // 名称
            "1.0.0",          // 版本
            {}                // 依赖
        };
        return info;
    }
    
    // 插件初始化方法
    bool plugin_init(service_factory& factory) {
        ilog("初始化示例插件");
        
        // 注册服务类型
        factory.register_service<example_service>("example");
        
        return true;
    }
    
    // 插件启动方法
    bool plugin_start() {
        ilog("启动示例插件");
        return true;
    }
    
    // 插件停止方法
    bool plugin_stop() {
        ilog("停止示例插件");
        return true;
    }
};

// 导出插件
MC_EXPORT_PLUGIN(example_plugin) 