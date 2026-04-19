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

#include <mc/engine/base.h>
#include <mc/engine/property/helper.h>
#include <mc/engine/property/processors/sync_property_processor.h>
#include <mc/exception.h>
#include <mc/expr/function/collection.h>
#include <mc/expr/function/parser.h>

namespace mc::engine {

bool sync_property_processor::matches(mc::string_view value_str) const
{
    return value_str.substr(0, 3) == "<=/";
}

void sync_property_processor::process(property_helper* property, mc::string_view value_str)
{
    auto sync_prop = mc::expr::func_parser::get_instance().parse_sync_property(value_str);
    hook_sync_property(property, sync_prop);
    property->set_property_type(p_type::sync);
}

p_type sync_property_processor::get_property_type() const
{
    return p_type::sync;
}

void sync_property_processor::hook_sync_property(property_helper*                   property,
                                                 const mc::engine::relate_property& relate_property)
{
    auto* target_object = property->find_related_object(relate_property.object_name);
    if (target_object == nullptr) {
        setup_deferred_sync_connection(property, relate_property);
    } else {
        setup_sync_property_connection(property, relate_property, *target_object);
    }

    // 同步属性不支持设置值
    property->ensure_extension_data();
    property->set_setter_function([property](const mc::variant& value) {
        MC_THROW(mc::invalid_op_exception, "Setting sync property value is not allowed: ${name}",
                 ("name", property->get_name()));
    });
}

void sync_property_processor::setup_sync_property_connection(property_helper*                   property,
                                                             const mc::engine::relate_property& relate_property,
                                                             abstract_object&                   target_object)
{
    // 断开旧连接
    property->clear_connection_slots();

    // 创建新的连接
    auto slot = target_object.property_changed().connect(
        [property, relate_property](const mc::variant& value, const property_base& prop) {
        if (prop.get_name() == relate_property.property_name) {
            auto result = property->get_relate_property(relate_property);
            if (!result.is_null()) {
                property->set_variant_value(result);
            }
        }
    });

    // 存储连接以便后续管理
    property->add_connection_slot(slot);

    // 设置初始值
    auto initial_value = target_object.get_property(relate_property.property_name);
    if (!initial_value.is_null()) {
        property->set_variant_value(initial_value);
    }
}

void sync_property_processor::setup_deferred_sync_connection(property_helper*                   property,
                                                             const mc::engine::relate_property& relate_property)
{
    auto position = property->get_object()->get_position();
    auto service  = mc::expr::func_collection::get_instance().get_service(position);
    if (service == nullptr) {
        return;
    }

    std::string full_object_name = relate_property.object_name + "_" + std::string(position);
    auto&       object_table     = service->get_object_table();

    auto slot = object_table.on_object_added.connect(
        [this, property, full_object_name, relate_property](mc::object_base& base_object) {
        auto& object = static_cast<mc::engine::abstract_object&>(base_object);
        if (object.get_name() == full_object_name) {
            setup_sync_property_connection(property, relate_property, object);
        }
    });

    // 存储延迟连接
    property->add_connection_slot(slot);
}

void sync_property_processor::hook_sync_properties(property_helper* property, mc::dict& relate_properties)
{
    // 断开旧连接
    property->clear_connection_slots();

    auto grouped_properties = property->group_properties_by_object(relate_properties);

    for (const auto& entry : grouped_properties) {
        auto object_name       = entry.key.template as<std::string>();
        auto object_properties = entry.value.template as<mc::dict>();
        process_sync_properties_for_object(property, object_name, object_properties);
    }

    // 所有监听器设置完毕后，统一进行一次初始化
    update_sync_value_from_function(property);

    // 同步属性不支持设置值
    property->ensure_extension_data();
    property->set_setter_function([](const mc::variant& value) {
        MC_THROW(mc::invalid_op_exception, "Setting sync property value is not allowed");
    });
}

void sync_property_processor::process_sync_properties_for_object(property_helper*   property,
                                                                 mc::string_view    object_name,
                                                                 const mc::dict&    object_properties)
{
    auto* target_object = property->find_related_object(object_name);
    if (target_object == nullptr) {
        setup_deferred_multi_sync_connection(property, object_name, object_properties);
        return;
    }

    setup_multi_sync_connection(property, *target_object, object_properties);
}

void sync_property_processor::setup_multi_sync_connection(property_helper* property, abstract_object& target_object,
                                                          const mc::dict& object_properties)
{
    auto slot = target_object.property_changed().connect(
        [property, object_properties_copy = object_properties](const mc::variant& value, const property_base& prop) {
        if (object_properties_copy.contains(prop.get_name())) {
            auto result = property->call_function_with_result(property->get_function_data());
            if (!result.is_null()) {
                property->set_variant_value(result);
            }
        }
    });

    // 存储连接
    property->add_connection_slot(slot);
}

void sync_property_processor::setup_deferred_multi_sync_connection(property_helper*   property,
                                                                   mc::string_view    object_name,
                                                                   const mc::dict&    object_properties)
{
    auto position = property->get_object()->get_position();
    auto service  = mc::expr::func_collection::get_instance().get_service(position);
    if (service == nullptr) {
        return;
    }

    std::string full_object_name = object_name + "_" + std::string(position);
    auto&       object_table     = service->get_object_table();

    auto slot = object_table.on_object_added.connect(
        [this, property, full_object_name, object_properties_copy = object_properties](mc::object_base& base_object) {
        auto& object = static_cast<mc::engine::abstract_object&>(base_object);
        if (object.get_name() == full_object_name) {
            setup_multi_sync_connection(property, object, object_properties_copy);
            update_sync_value_from_function(property);
        }
    });

    // 存储延迟连接
    property->add_connection_slot(slot);
}

void sync_property_processor::update_sync_value_from_function(property_helper* property)
{
    auto result = property->call_function_with_result(property->get_function_data());
    if (result.is_null()) {
        return;
    }

    property->set_variant_value(result);
}

} // namespace mc::engine