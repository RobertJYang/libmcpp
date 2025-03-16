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

#ifndef MC_MODULE_H
#define MC_MODULE_H

#include <string>
#include <vector>

namespace mc {

/**
 * @brief 模块信息
 */
struct module_info {
    std::string name;                    // 模块名称
    std::string version;                 // 模块版本
    std::vector<std::string> dependencies;  // 依赖的模块
};

/**
 * @brief 模块基类
 */
class module {
public:
    virtual ~module() = default;

    // 获取模块信息
    virtual const module_info& get_info() const = 0;

    // 初始化模块（注册服务）
    virtual bool init() = 0;

    // 启动模块
    virtual bool start() = 0;

    // 停止模块
    virtual bool stop() = 0;

    // 卸载模块
    virtual bool unload() = 0;
};

} // namespace mc

#endif // MC_MODULE_H 