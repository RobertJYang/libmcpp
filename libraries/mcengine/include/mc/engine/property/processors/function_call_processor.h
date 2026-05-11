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

#include <mc/dict.h>
#include <mc/engine/property/processor.h>

namespace mc::engine {

class sync_property_processor;
class ref_property_processor;

/**
 * @brief 函数调用处理器
 *
 * 处理格式为 "$Func_..." 的函数调用属性
 */
class MC_API function_call_processor : public property_processor_interface {
public:
    function_call_processor();

    bool   matches(mc::string_view value_str) const override;
    void   process(property_helper* property, mc::string_view value_str) override;
    p_type get_property_type() const override;

private:
    /**
     * @brief 处理属性值的函数调用逻辑
     */
    void process_property_value(property_helper* property, mc::string_view value_str);

    std::unique_ptr<sync_property_processor> m_sync_processor;
    std::unique_ptr<ref_property_processor>  m_ref_processor;
};

} // namespace mc::engine