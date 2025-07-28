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
 * @brief 插件基类定义
 */
#ifndef MC_CORE_PLUGIN_H
#define MC_CORE_PLUGIN_H

#include <mc/core/service_factory.h>

#include <string>
#include <vector>

namespace mc::core {

/**
 * @brief 插件信息结构
 */
struct plugin_info {
    std::string              name;         // 插件名称
    std::string              version;      // 插件版本
    std::vector<std::string> dependencies; // 依赖的其他插件
};

/**
 * @brief 插件基类
 */
class MC_API plugin {
public:
    virtual ~plugin() = default;

    /**
     * @brief 获取插件信息
     * @return 插件信息
     */
    virtual const plugin_info& get_info() const = 0;

    /**
     * @brief 初始化插件
     * @param factory 服务工厂实例
     * @return 初始化是否成功
     */
    virtual bool init(service_factory& factory) = 0;
};

// 插件创建函数类型
using create_plugin_func = plugin* (*)();

// 插件销毁函数类型
using destroy_plugin_func = void (*)(plugin*);

} // namespace mc::core

// 导出插件的宏定义
#define MC_EXPORT_PLUGIN(PluginClass)                 \
    extern "C" {                                      \
    MC_API mc::core::plugin* create_plugin() {        \
        return new PluginClass();                     \
    }                                                 \
    MC_API void destroy_plugin(mc::core::plugin* p) { \
        delete p;                                     \
    }                                                 \
    }

#endif // MC_CORE_PLUGIN_H