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

#ifndef MC_CORE_MODULE_H
#define MC_CORE_MODULE_H

#include <memory>
#include <string>
#include <vector>
#include <boost/program_options.hpp>
#include "mc/variant.h"

namespace mc {

namespace po = boost::program_options;

/**
 * @brief 模块信息结构
 */
struct module_info {
    std::string name;                    // 模块名称
    std::string version;                 // 模块版本
    std::vector<std::string> dependencies;  // 模块依赖
    std::string min_app_version;         // 最小应用版本要求
};

/**
 * @brief 模块接口类
 */
class module {
public:
    virtual ~module() = default;

    // 生命周期方法
    virtual bool load() = 0;                    // 加载模块
    virtual bool unload() = 0;                  // 卸载模块
    virtual void pre_unload() {}               // 卸载前处理
    virtual void post_load() {}                // 加载后处理
    
    // 配置管理
    virtual void register_options(po::options_description& cli_opts,
                                po::options_description& cfg_opts) {}
                                
    // 资源管理
    virtual void cleanup_resources() {}         // 清理资源
    
    // 状态查询
    virtual bool is_unloadable() const { return true; }  // 是否可以卸载
    virtual std::vector<std::string> get_active_services() const = 0;  // 获取活动服务
    
    // 版本和依赖
    virtual const module_info& get_info() const = 0;   // 获取模块信息
};

/**
 * @brief 基础模块类，提供通用功能实现
 */
template <typename Impl>
class module_base : public module {
public:
    module_base() = default;
    ~module_base() override = default;

    // 获取模块名称
    std::string name() const {
        return get_info().name;
    }

protected:
    // 添加模块依赖
    Impl& depends_on(const std::string& dep_name) {
        auto& deps = m_info.dependencies;
        if (!dep_name.empty() && 
            std::find(deps.begin(), deps.end(), dep_name) == deps.end()) {
            deps.push_back(dep_name);
        }
        return static_cast<Impl&>(*this);
    }

    // 设置模块信息
    void set_info(const module_info& info) {
        m_info = info;
    }

    // 获取模块信息
    const module_info& get_info() const override {
        return m_info;
    }

private:
    module_info m_info;
};

using module_ptr = std::shared_ptr<module>;

} // namespace mc

#endif // MC_CORE_MODULE_H 