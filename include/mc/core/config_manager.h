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
 * @file config_manager.h
 * @brief 配置管理器，负责配置的加载、解析和管理
 */
#ifndef MC_CONFIG_MANAGER_H
#define MC_CONFIG_MANAGER_H

#include <boost/program_options.hpp>
#include <memory>
#include <string>
#include <vector>
#include <iostream>

namespace mc {

namespace po = boost::program_options;

/**
 * @brief 配置选项结构体
 */
struct options {
    po::options_description cli;  // 命令行选项
    po::options_description cfg;  // 配置文件选项
    
    options() : cli("命令行选项"), cfg("配置文件选项") {}
};

/**
 * @brief 配置管理器类
 */
class config_manager {
public:
    // 构造和析构
    config_manager(bool silent = false);
    ~config_manager();

    // 配置文件加载
    void load_config_file(const std::string& cfg_file = "");

    // 命令行参数解析
    bool parse_command_line(int argc, char** argv);
    
    // 获取配置选项
    options &get_options() { return *m_opts; }
    
    // 配置值获取
    template<typename T>
    T get_option(const std::string& name, const T& default_value = T()) const {
        if (m_options.count(name)) {
            return m_options[name].as<T>();
        }
        return default_value;
    }
    
    bool has_option(const std::string& name) const;
    
    // 获取插件列表
    std::vector<std::string> get_plugin_names() const;

    // 静默模式控制
    void set_silent(bool silent);
    bool is_silent() const;

private:
    // 处理未识别的选项
    void process_unrecognized(const std::vector<std::string>& unrecognized) const;
    
    // 成员变量
    std::unique_ptr<options> m_opts;         // 配置选项
    po::variables_map m_options;             // 配置值映射表
    bool m_silent{false};                    // 静默模式（不打印警告和错误）
};

} // namespace mc

#endif // MC_CONFIG_MANAGER_H 