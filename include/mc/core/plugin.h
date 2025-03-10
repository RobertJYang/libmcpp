/*
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef MC_CORE_PLUGIN_H
#define MC_CORE_PLUGIN_H

#include <algorithm>
#include <boost/program_options.hpp>
#include <filesystem>
#include <string>
#include <vector>

namespace mc {
namespace fs = std::filesystem;
namespace po = boost::program_options;

// 前向声明
class application;

// 声明获取应用程序实例的函数
application& app();

/**
 * @brief 插件接口类
 */
class plugin {
public:
    virtual ~plugin() = default;

    /**
     * @brief 获取插件名称
     * @return 插件名称
     */
    virtual std::string name() const = 0;

    /**
     * @brief 初始化插件
     * @return 初始化是否成功
     */
    virtual bool initialize() = 0;

    /**
     * @brief 启动插件
     */
    virtual void startup() = 0;

    /**
     * @brief 关闭插件
     */
    virtual void shutdown() = 0;

    /**
     * @brief 收集插件的命令行和配置文件配置项
     * @param[out] cli_opts 命令行配置项
     * @param[out] cfg_opts 配置文件配置项
     */
    virtual void register_options(po::options_description& cli_opts,
                                  po::options_description& cfg_opts) {
    }

    /**
     * @brief 获取插件依赖列表
     * @return 依赖的插件名称列表
     */
    virtual const std::vector<std::string>& dependencies() const {
        static std::vector<std::string> empty_deps;
        return empty_deps;
    }
};

/**
 * @brief 基础插件类，提供通用功能实现
 * @tparam Impl 插件实现类，必须提供static const char* plugin_name()方法
 */
template <typename Impl>
class plugin_base : public plugin {
public:
    plugin_base() = default;
    virtual ~plugin_base() = default;

    /**
     * @brief 获取插件名称
     * @return 插件名称
     */
    std::string name() const override {
        return Impl::plugin_name();
    }

    /**
     * @brief 获取插件依赖列表
     * @return 依赖的插件名称列表
     */
    const std::vector<std::string>& dependencies() const override {
        return m_dependencies;
    }

protected:
    /**
     * @brief 添加插件依赖
     * @param dep_name 依赖的插件名称
     * @return 插件实现类引用，用于链式调用
     */
    Impl& depends_on(const std::string& dep_name) {
        if (!dep_name.empty() && std::find(m_dependencies.begin(), m_dependencies.end(),
                                           dep_name) == m_dependencies.end()) {
            m_dependencies.push_back(dep_name);
        }
        return static_cast<Impl&>(*this);
    }

    /**
     * @brief 获取应用程序实例
     * @return 应用程序实例引用
     */
    application& app() {
        return mc::app();
    }

private:
    std::vector<std::string> m_dependencies; ///< 依赖的插件名称列表
};

} // namespace mc

#endif // MC_CORE_PLUGIN_H