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
#include <mc/engine/property/ref_object.h>

namespace mc::engine {

/**
 * @brief 引用对象处理器
 *
 * 处理格式为 "#/object_name" 的引用对象属性
 */
class MC_API ref_object_processor : public property_processor_interface {
public:
    bool   matches(const std::string& value_str) const override;
    void   process(property_helper* property, const std::string& value_str) override;
    p_type get_property_type() const override;

private:
    /**
     * @brief 处理引用对象的from_variant逻辑
     */
    void process_ref_object_from_variant(property_helper* property, const std::string& ref_object_str);

    /**
     * @brief 初始化引用对象缓存
     */
    void initialize_ref_object_cache(property_helper* property, const std::string& object_name);
};

} // namespace mc::engine