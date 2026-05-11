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
#include <mc/engine/property/processors/ref_object_processor.h>
#include <mc/exception.h>
#include <mc/expr/function/parser.h>

namespace mc::engine {

bool ref_object_processor::matches(mc::string_view value_str) const
{
    return value_str.substr(0, 2) == "#/" && value_str.find('.') == mc::string_view::npos;
}

void ref_object_processor::process(property_helper* property, mc::string_view value_str)
{
    process_ref_object_from_variant(property, value_str);
}

p_type ref_object_processor::get_property_type() const
{
    return p_type::ref_object;
}

void ref_object_processor::process_ref_object_from_variant(property_helper* property, mc::string_view ref_object_str)
{
    auto ref_obj = mc::expr::func_parser::get_instance().parse_ref_object(ref_object_str);
    property->set_property_type(p_type::ref_object);

    // 清理旧的缓存（如果有的话）
    if (property->has_extension_data()) {
        property->set_ref_object_cache(nullptr);
    }

    // 暂时存储对象名称到属性值中（懒加载，等到需要时再转换）
    property->set_internal_value(mc::variant(ref_obj.object_name));

    property->ensure_extension_data();
    property->set_setter_function([property](const mc::variant& value) {
        MC_THROW(mc::invalid_op_exception, "Setting reference object property value is not allowed: ${name}",
                 ("name", property->get_name()));
    });

    // 初始化引用对象缓存
    initialize_ref_object_cache(property, ref_obj.object_name);
}

void ref_object_processor::initialize_ref_object_cache(property_helper* property, mc::string_view object_name)
{
    // 分配缓存内存
    auto cache = std::make_unique<mc::variant>();

    // 创建弱引用对象包装器
    auto object_finder = [property](mc::string_view name) -> abstract_object* {
        return property->find_related_object(name);
    };
    auto ref_obj = mc::make_shared<ref_object>(object_name, object_finder);

    // 在缓存中存储 ref_object
    *cache = mc::variant(ref_obj);
    property->set_ref_object_cache(std::move(cache));

    // 现在安全地设置属性的实际值为ref_object，使用set_internal_value避免递归
    property->set_internal_value(mc::variant(ref_obj));
}

} // namespace mc::engine