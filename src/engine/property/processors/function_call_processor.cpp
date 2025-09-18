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

#include <mc/engine/property/detail.h>
#include <mc/engine/property/helper.h>
#include <mc/engine/property/processors/function_call_processor.h>
#include <mc/engine/property/processors/ref_property_processor.h>
#include <mc/engine/property/processors/sync_property_processor.h>
#include <mc/exception.h>
#include <mc/expr/function/collection.h>
#include <mc/expr/function/parser.h>
#include <mc/log.h>
#include <mc/variant.h>

namespace mc::engine {

function_call_processor::function_call_processor()
    : m_sync_processor(std::make_unique<sync_property_processor>()),
      m_ref_processor(std::make_unique<ref_property_processor>()) {
}

bool function_call_processor::matches(const std::string& value_str) const {
    return value_str.substr(0, 6) == "$Func_";
}

void function_call_processor::process(property_helper* property, const std::string& value_str) {
    process_property_value(property, value_str);
}

p_type function_call_processor::get_property_type() const {
    return p_type::normal; // 函数调用可能产生不同类型的属性
}

void function_call_processor::process_property_value(property_helper* property, const std::string& value_str) {
    auto func_info = mc::expr::func_parser::get_instance().parse_function_call(value_str);
    auto position  = property->get_object()->get_position();
    auto call_func = mc::expr::func_collection::get_instance().get(position, func_info.func);

    if (call_func.is_null()) {
        elog("Function not found: ${name}", ("name", func_info.func));
        // 函数未找到时，不修改属性值，保持原有值
        return;
    }

    auto func_params       = func_info.params;
    auto relate_properties = call_func.template as<mc::expr::func>().get_relate_properties(
        property->get_object()->get_position(), func_params);

    if (relate_properties.empty()) {
        // 没有关联属性，只计算一次，不需要保存函数对象
        auto result = call_func.template as<mc::expr::func>().call(position, func_params);
        if (result.is_null()) {
            return;
        }
        property->set_variant_value(result);
        return;
    }

    // 分配func_data，保存函数对象
    property->ensure_extension_data();
    auto func_data = std::make_unique<detail::func_data>();
    mc::from_variant(call_func, func_data->func_obj); // 正确转换到func类型
    func_data->params = func_params;
    property->set_function_data(std::move(func_data));

    // 处理第一个关联属性的类型
    for (const auto& entry : relate_properties) {
        auto value = entry.value.template as<mc::expr::relate_property>();
        if (value.type == "ref") {
            auto mutable_relate_properties = relate_properties; // 创建可变副本
            m_ref_processor->hook_ref_properties(property, mutable_relate_properties);
            property->set_property_type(p_type::reference);
        } else if (value.type == "sync") {
            auto mutable_relate_properties = relate_properties; // 创建可变副本
            m_sync_processor->hook_sync_properties(property, mutable_relate_properties);
            property->set_property_type(p_type::sync);
        }
        break;
    }
}

} // namespace mc::engine