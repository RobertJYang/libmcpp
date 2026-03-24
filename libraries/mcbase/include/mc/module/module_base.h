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

#ifndef MC_MODULE_MODULE_BASE_H
#define MC_MODULE_MODULE_BASE_H

#include <mc/memory.h>
#include <mc/reflect/reflect.h>
#include <mc/reflect/reflection_factory.h>

#include <string_view>

namespace mc::module {

/**
 * @brief 模块接口
 */
class MC_API module_base : public mc::shared_base {
public:
    virtual ~module_base() = default;

    /**
     * @brief 获取模块名称
     */
    virtual mc::string_view name() const = 0;

    /**
     * @brief 获取模块版本
     */
    virtual mc::string_view version() const = 0;

    /**
     * @brief 获取模块的反射工厂 - 供脚本使用
     */
    virtual mc::reflect::reflection_factory* get_factory() const = 0;
};

using module_ptr = mc::shared_ptr<module_base>;

} // namespace mc::module

#endif // MC_MODULE_MODULE_BASE_H