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

#include <mc/engine/property/helper.h>
#include <mc/engine/property/processors/ref_property_processor.h>
#include <mc/exception.h>
#include <mc/expr/function/parser.h>
#include <mc/log.h>

namespace mc::engine {

bool ref_property_processor::matches(const std::string& value_str) const
{
    return value_str.substr(0, 2) == "#/" && value_str.find('.') != std::string::npos;
}

void ref_property_processor::process(property_helper* property, const std::string& value_str)
{
    auto ref_prop = mc::expr::func_parser::get_instance().parse_ref_property(value_str);
    hook_ref_property(property, ref_prop);
    property->set_property_type(p_type::reference);
}

p_type ref_property_processor::get_property_type() const
{
    return p_type::reference;
}

void ref_property_processor::hook_ref_property(property_helper*                 property,
                                               const mc::expr::relate_property& relate_property)
{
    property->ensure_extension_data();

    property->set_getter_function([property, relate_property]() -> mc::variant {
        auto result = property->get_relate_property(relate_property);
        if (result.is_null()) {
            MC_THROW(mc::invalid_op_exception,
                     "get reference property of ${name} failed: ${object_name}.${property_name}",
                     ("name", property->get_name())("object_name", relate_property.object_name)(
                         "property_name", relate_property.property_name));
        }
        return result;
    });

    property->set_setter_function([property, relate_property](const mc::variant& value) {
        property->set_relate_property(relate_property, value);
    });
}

void ref_property_processor::hook_ref_properties(property_helper* property, mc::dict& relate_properties)
{
    property->ensure_extension_data();

    // 设置获取函数，调用函数并获取结果
    property->set_getter_function([property]() -> mc::variant {
        auto result = property->call_function_with_result(property->get_function_data());
        if (result.is_null()) {
            return mc::variant();
        }
        return result;
    });

    // 引用属性不支持设置值
    property->set_setter_function([property](const mc::variant& value) {
        MC_THROW(mc::invalid_op_exception, "Setting sync property value is not allowed: ${name}",
                 ("name", property->get_name()));
    });
}

} // namespace mc::engine