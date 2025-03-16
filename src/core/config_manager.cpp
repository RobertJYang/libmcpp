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
 * 配置管理器实现
 * 
 * 配置优先级规则（从高到低）：
 * 1. 命令行参数（优先级最高）
 * 2. 配置文件（优先级次之）
 * 3. 默认值（优先级最低）
 * 
 * 特殊情况：
 * - 对于标记为 composing() 的选项（如 module），多处指定的值会被合并而不是覆盖
 * - 服务选项会在模块加载后被收集，所以依赖于模块注册的服务选项只能在模块加载后使用
 */

#include "mc/core/config_manager.h"
#include "mc/filesystem.h"
#include <iostream>
#include <thread>
#include <unordered_map>
#include <typeinfo>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>
#include <fstream>
#include <mc/log.h>

namespace mc {

// 构造函数
config_manager::config_manager(bool silent) 
    : m_opts(std::make_unique<options>())
    , m_silent(silent) {
    m_opts->cli.add_options()
        ("help,h", "显示帮助信息")
        ("version,v", "显示版本信息")
        ("config,c", po::value<std::string>()->default_value("./config.ini"), "配置文件路径")
        ("module-dir", po::value<std::string>(), "模块目录路径")
        ("threads,t", po::value<unsigned int>()->default_value(std::thread::hardware_concurrency()), "线程数量")
        ("module,m", po::value<std::vector<std::string>>()->composing(), "要加载的模块列表");

    m_opts->cfg.add(m_opts->cli);
}

// 析构函数
config_manager::~config_manager() {
}

// 设置静默模式
void config_manager::set_silent(bool silent) {
    m_silent = silent;
}

// 获取静默模式状态
bool config_manager::is_silent() const {
    return m_silent;
}

// 处理未识别的选项
void config_manager::process_unrecognized(const std::vector<std::string>& unrecognized) const {
    if (!m_silent && !unrecognized.empty()) {
        std::cerr << "警告: 未识别的选项:";
        for (const auto& opt : unrecognized) {
            std::cerr << " " << opt;
        }
        std::cerr << std::endl;
    }
}

// 解析配置文件
void config_manager::load_config_file(const std::string& cfg_file) {
    std::string config_file = cfg_file;
    if (config_file.empty()) {
        if (!has_option("config")) {
            return;
        }
        config_file = get_option<std::string>("config");
    }
    
    try {
        std::ifstream file(config_file);
        if (!file.is_open()) {
            wlog("警告: 配置文件 '${file}' 不存在", ("file", config_file));
            return;
        }
        
        // 解析配置文件
        po::store(po::parse_config_file(file, m_opts->cfg, true), m_options);
        po::notify(m_options);
    } catch (const po::error& e) {
        elog("错误: 解析配置文件 '${file}' 失败: ${error}", ("file", config_file)("error", e.what()));
    }
}

// 从命令行解析配置
bool config_manager::parse_command_line(int argc, char** argv) {
    try {
        // 处理命令行参数
        po::parsed_options parsed = po::command_line_parser(argc, argv)
            .options(m_opts->cli)
            .allow_unregistered()
            .run();
        
        // 存储已识别的选项
        po::store(parsed, m_options);
        po::notify(m_options);
        
        // 处理未识别的选项
        std::vector<std::string> unrecognized = po::collect_unrecognized(parsed.options, po::include_positional);
        if (!unrecognized.empty() && !m_silent) {
            wlog("警告: 未识别的选项:");
            for (const auto& opt : unrecognized) {
                wlog("- ${option}", ("option", opt));
            }
        }
        
        return true;
    } catch (const po::error& e) {
        elog("错误: 解析命令行参数失败: ${error}", ("error", e.what()));
        return false;
    }
}

// 检查选项是否存在
bool config_manager::has_option(const std::string& name) const {
    return m_options.count(name) > 0;
}

// 获取模块列表
std::vector<std::string> config_manager::get_module_names() const {
    if (has_option("module")) {
        return get_option<std::vector<std::string>>("module");
    }
    return {};
}

} // namespace mc 