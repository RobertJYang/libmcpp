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

#ifndef EXAMPLE_SERVICE_H
#define EXAMPLE_SERVICE_H

#include "mc/core/service.h"
#include <iostream>
#include <boost/program_options.hpp>
#include <mc/log.h>

namespace mc {

namespace po = boost::program_options;

/**
 * @brief 示例服务类
 */
class example_service : public service_base<example_service> {
public:
    example_service() = default;
    ~example_service() override = default;

    // 服务类型名称
    static const char* service_type() { return "example.service"; }

    // 注册配置选项
    static void register_options(po::options_description& cli_opts,
                         po::options_description& cfg_opts) {
        // 在example.service节下添加选项
        cfg_opts.add_options()
            ("example.service.message", po::value<std::string>()->default_value("Hello, World!"), "示例服务的消息")
            ("example.service.repeat_count", po::value<int>()->default_value(1), "消息重复次数");
    }

    // 声明依赖
    static const std::vector<std::string>& dependencies() {
        static std::vector<std::string> deps;
        return deps;
    }

    // 初始化服务
    bool init(const service_config& config) override {
        ilog("初始化示例服务...");
        
        // 从配置中获取参数
        if (config.properties.contains("example.service.message")) {
            m_message = config.properties["example.service.message"].as<std::string>();
        } else {
            m_message = "Hello, World!";
        }
        
        if (config.properties.contains("example.service.repeat_count")) {
            m_repeat_count = config.properties["example.service.repeat_count"].as<int>();
        } else {
            m_repeat_count = 1;
        }
        
        ilog("配置参数: message=${message}, repeat_count=${count}", 
             ("message", m_message)("count", m_repeat_count));
        
        return true;
    }

    // 启动服务
    bool start() override {
        ilog("启动示例服务...");
        
        // 输出配置参数
        ilog("配置参数: message=${message}, repeat_count=${count}", 
             ("message", m_message)("count", m_repeat_count));
        
        // 输出消息
        for (int i = 0; i < m_repeat_count; ++i) {
            ilog("消息 #${index}: ${message}", ("index", i + 1)("message", m_message));
        }
        
        return true;
    }

    // 停止服务
    bool stop() override {
        ilog("停止示例服务...");
        return true;
    }

    // 清理资源
    void cleanup() override {
        ilog("清理示例服务资源...");
    }

    // 检查服务健康状态
    bool is_healthy() const override {
        return true;
    }

private:
    std::string m_message;
    int m_repeat_count;
};

} // namespace mc

#endif // EXAMPLE_SERVICE_H 