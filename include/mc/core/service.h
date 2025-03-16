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

#ifndef MC_CORE_SERVICE_H
#define MC_CORE_SERVICE_H

#include <memory>
#include <string>
#include <vector>
#include "mc/dict.h"
#include <boost/program_options.hpp>

namespace mc {

namespace po = boost::program_options;

/**
 * @brief 服务状态枚举
 */
enum class service_state {
    stopped,     // 已停止
    starting,    // 正在启动
    running,     // 运行中
    stopping,    // 正在停止
    failed       // 失败
};

/**
 * @brief 服务配置结构
 */
struct service_config {
    std::string name;                    // 服务名称
    std::string type;                    // 服务类型
    std::vector<std::string> dependencies;  // 服务依赖
    dict properties;                     // 服务属性配置
};

/**
 * @brief 服务接口类
 */
class service {
public:
    virtual ~service() = default;

    // 构造函数
    explicit service(std::string name = "") : m_name(std::move(name)) {}
    
    // 初始化方法
    virtual bool init(dict args) = 0;

    // 生命周期方法
    virtual bool start() = 0;                            // 启动服务
    virtual bool stop() = 0;                             // 停止服务
    virtual void cleanup() = 0;                          // 清理资源

    // 状态查询
    virtual service_state get_state() const = 0;         // 获取服务状态
    virtual bool is_healthy() const = 0;                 // 检查服务健康状态
    
    // 名称获取
    const std::string& name() const { return m_name; }
    
    // 配置获取（默认实现为空，供以前的服务实现兼容）
    virtual const service_config& get_config() const { 
        static service_config empty_config;
        return empty_config;
    }

protected:
    std::string m_name; // 服务实例名称
};

/**
 * @brief 基础服务类，提供通用功能实现
 */
template <typename Impl>
class service_base : public service {
public:
    explicit service_base(std::string name = "") : service(std::move(name)), m_state(service_state::stopped) {}
    ~service_base() override = default;

    // 获取服务状态
    service_state get_state() const override {
        return m_state;
    }

    static void register_options(po::options_description& cli_opts, po::options_description& cfg_opts) {
    }
    
protected:
    // 设置服务状态
    void set_state(service_state state) {
        m_state = state;
    }

private:
    service_state m_state;   // 服务状态
};

using service_ptr = std::shared_ptr<service>;

} // namespace mc

#endif // MC_CORE_SERVICE_H 