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

#include <mc/engine/property/processor.h>
#include <mc/expr/function/parser.h>

namespace mc::engine {

/**
 * @brief 引用属性处理器
 *
 * 处理格式为 "#/object_name.property_name" 的引用属性
 */
class MC_API ref_property_processor : public property_processor_interface {
public:
    bool   matches(const std::string& value_str) const override;
    void   process(property_helper* property, const std::string& value_str) override;
    p_type get_property_type() const override;

    // 公开方法供其他处理器使用
    void hook_ref_properties(property_helper* property, mc::dict& relate_properties);

private:
    /**
     * @brief 处理引用属性的逻辑
     */
    void hook_ref_property(property_helper* property, const mc::expr::relate_property& relate_property);
};

} // namespace mc::engine