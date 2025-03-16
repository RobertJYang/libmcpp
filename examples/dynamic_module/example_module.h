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

#ifndef EXAMPLE_MODULE_H
#define EXAMPLE_MODULE_H

#include "example_service.h"
#include "mc/core/application.h"
#include "mc/core/module.h"
#include <iostream>
#include <memory>

namespace mc {

/**
 * @brief 示例模块类
 */
class example_module : public module {
private:
    module_info m_info;

public:
    example_module() {
        m_info.name    = "example";
        m_info.version = "1.0.0";
    }

    // 获取模块信息
    const module_info& get_info() const override {
        return m_info;
    }

    // 初始化模块（注册服务）
    bool init() override {
        std::cout << "初始化示例模块..." << std::endl;

        // 注册服务
        app().get_service_manager().register_service(
            example_service::service_type(),
            []() -> service_ptr {
                return std::make_shared<example_service>();
            },
            example_service::register_options
        );

        return true;
    }

    // 启动模块
    bool start() override {
        std::cout << "启动示例模块..." << std::endl;
        return true;
    }

    // 停止模块
    bool stop() override {
        std::cout << "停止示例模块..." << std::endl;
        return true;
    }

    // 卸载模块
    bool unload() override {
        std::cout << "卸载示例模块..." << std::endl;
        return true;
    }
};

// 导出模块创建函数
extern "C" module* create_module();

} // namespace mc

#endif // EXAMPLE_MODULE_H