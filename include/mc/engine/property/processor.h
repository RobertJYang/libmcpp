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

#pragma once

#include <mc/engine/property/types.h>
#include <mc/variant.h>
#include <memory>
#include <string>
#include <vector>

namespace mc::engine {

// 前向声明
class property_helper;
namespace detail {
template <typename T>
struct property_extension_data;
}

/**
 * @brief 属性处理器接口
 *
 * 定义了处理不同类型属性（引用对象、引用属性、同步属性、函数调用）的统一接口
 */
class property_processor_interface {
public:
    virtual ~property_processor_interface() = default;

    /**
     * @brief 检查字符串是否匹配当前处理器的格式
     */
    virtual bool matches(const std::string& value_str) const = 0;

    /**
     * @brief 处理属性值设置
     */
    virtual void process(property_helper* property, const std::string& value_str) = 0;

    /**
     * @brief 获取处理器处理的属性类型
     */
    virtual p_type get_property_type() const = 0;
};

/**
 * @brief 属性处理器工厂类
 *
 * 管理所有属性处理器，提供统一的处理入口
 */
class property_processor_factory {
public:
    static MC_API property_processor_factory& get_instance();

    /**
     * @brief 注册处理器
     */
    void register_processor(std::unique_ptr<property_processor_interface> processor);

    /**
     * @brief 处理属性值
     * @param property 属性对象
     * @param value_str 属性值字符串
     * @return true 如果找到合适的处理器并处理成功
     */
    MC_API bool process_property_value(property_helper* property, const std::string& value_str);

private:
    std::vector<std::unique_ptr<property_processor_interface>> m_processors;
};

} // namespace mc::engine