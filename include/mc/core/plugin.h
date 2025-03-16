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
 * @file plugin.h
 * @brief 定义插件接口，用于动态加载服务实现
 */
#ifndef MC_CORE_PLUGIN_H
#define MC_CORE_PLUGIN_H

#include <mc/log.h>
#include <string>
#include <vector>

namespace mc {

// 前置声明
class service_factory;

/**
 * @brief 插件信息
 */
struct plugin_info {
    std::string              name;         // 插件名称
    std::string              version;      // 插件版本
    std::vector<std::string> dependencies; // 依赖的插件
};

/**
 * @brief 插件接口类
 */
class plugin {
public:
    virtual ~plugin() = default;

    // 获取插件信息
    virtual const plugin_info& get_info() const = 0;

    // 初始化插件（注册服务）
    virtual bool init(service_factory& factory) = 0;

    // 启动插件
    virtual bool start() = 0;

    // 停止插件
    virtual bool stop() = 0;
};

/**
 * @brief 插件基类模板，提供基础状态管理
 *
 * @tparam Impl 派生类类型（CRTP模式）
 */
template <typename Impl>
class plugin_base : public plugin {
public:
    plugin_base() = default;
    ~plugin_base() override = default;

    // 获取插件信息
    const plugin_info& get_info() const override = 0;

    /**
     * @brief 初始化插件
     * @param factory 服务工厂实例，用于注册服务类型
     * @return 初始化是否成功
     */
    bool init(service_factory& factory) override {
        bool result = static_cast<Impl*>(this)->plugin_init(factory);
        if (result) {
            ilog("插件 '${name}' 初始化成功", ("name", get_info().name));
        } else {
            elog("插件 '${name}' 初始化失败", ("name", get_info().name));
        }
        return result;
    }

    /**
     * @brief 启动插件
     * @return 启动是否成功
     */
    bool start() override {
        bool result = static_cast<Impl*>(this)->plugin_start();
        if (result) {
            ilog("插件 '${name}' 启动成功", ("name", get_info().name));
        } else {
            elog("插件 '${name}' 启动失败", ("name", get_info().name));
        }
        return result;
    }

    /**
     * @brief 停止插件
     * @return 停止是否成功
     */
    bool stop() override {
        bool result = static_cast<Impl*>(this)->plugin_stop();
        if (result) {
            ilog("插件 '${name}' 停止成功", ("name", get_info().name));
        } else {
            elog("插件 '${name}' 停止失败", ("name", get_info().name));
        }
        return result;
    }

protected:
    // 派生类需要实现的实际操作方法
    bool plugin_init(service_factory& factory) {
        return true;
    }
    
    bool plugin_start() {
        return true;
    }
    
    bool plugin_stop() {
        return true;
    }

private:
    bool m_initialized{false}; // 是否已初始化
    bool m_running{false};     // 是否正在运行
};

// 插件创建与销毁函数类型定义
typedef plugin* (*create_plugin_func)();
typedef void (*destroy_plugin_func)(plugin*);

// 导出插件的宏
#define MC_EXPORT_PLUGIN(PluginClass) \
    extern "C" { \
        mc::plugin* create_plugin() { \
            return new PluginClass(); \
        } \
        \
        void destroy_plugin(mc::plugin* p) { \
            delete p; \
        } \
    }

} // namespace mc

#endif // MC_CORE_PLUGIN_H 