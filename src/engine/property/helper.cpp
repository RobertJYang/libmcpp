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
#include <mc/exception.h>
#include <mc/expr/function/collection.h>
#include <mc/expr/function/parser.h>

namespace mc::engine {

abstract_object* property_helper::find_related_object(const std::string& object_name) {
    auto position = get_object()->get_position();
    auto service  = mc::expr::func_collection::get_instance().get_service(position);
    if (service == nullptr) {
        elog("Service not found for position: ${position}",
             ("position", std::string(position)));
        return nullptr;
    }

    std::string full_object_name = object_name + "_" + std::string(position);

    auto& object_table = service->get_object_table();
    auto& idx          = object_table.template get<mc::engine::by_object_name>();
    auto  obj_it       = idx.find(full_object_name);
    if (obj_it == idx.end()) {
        obj_it = idx.find(full_object_name + "_dev");
    }

    if (obj_it == idx.end()) {
        elog("Object not found: ${object_name}, searched for: ${full_name}",
             ("object_name", object_name)("full_name", full_object_name));
        return nullptr;
    }

    return const_cast<mc::engine::abstract_object*>(&(*obj_it));
}

mc::variant property_helper::call_function_with_result(const detail::func_data* function_data) {
    if (!function_data) {
        elog("Function data is null for property: ${name}", ("name", get_name()));
        return mc::variant();
    }

    try {
        auto position = get_object()->get_position();
        // 需要复制参数，因为call方法需要非const引用
        auto params_copy = function_data->params;
        // 使用const_cast来调用非const方法，这在这里是安全的
        auto result = const_cast<mc::expr::func&>(function_data->func_obj).call(position, params_copy);
        if (result.is_null()) {
            elog("Function call failed for property: ${name}", ("name", get_name()));
        }
        return result;
    } catch (const std::exception& e) {
        elog("Function call exception for property: ${name}, error: ${error}",
             ("name", get_name())("error", e.what()));
        return mc::variant();
    }
}

mc::variant property_helper::get_relate_property(const mc::expr::relate_property& relate_property) {
    auto* target_object = find_related_object(relate_property.object_name);
    if (target_object == nullptr) {
        return mc::variant();
    }

    // 如果指定了接口，先获取接口再获取属性
    if (!relate_property.interface.empty()) {
        auto interface_obj = target_object->get_interface(relate_property.interface);
        if (interface_obj == nullptr) {
            elog("Interface not found: ${interface} in object: ${object_name}",
                 ("interface", relate_property.interface)("object_name", relate_property.object_name));
            return mc::variant();
        }
        return interface_obj->get_property(relate_property.property_name);
    } else {
        // 传统方式：直接从对象获取属性
        return target_object->get_property(relate_property.property_name);
    }
}

void property_helper::set_relate_property(const mc::expr::relate_property& relate_property, const mc::variant& value) {
    auto* target_object = find_related_object(relate_property.object_name);
    if (target_object == nullptr) {
        MC_THROW(mc::invalid_op_exception, "set_relate_property ${name} failed: Object not found: ${object_name}",
                 ("name", get_name())("object_name", relate_property.object_name));
    }

    // 如果指定了接口，先获取接口再设置属性
    if (!relate_property.interface.empty()) {
        auto interface_obj = target_object->get_interface(relate_property.interface);
        if (interface_obj == nullptr) {
            MC_THROW(mc::invalid_op_exception, "set_relate_property ${name} failed: Interface not found: ${interface} in object: ${object_name}",
                     ("name", get_name())("interface", relate_property.interface)("object_name", relate_property.object_name));
        }
        interface_obj->set_property(relate_property.property_name, value);
    } else {
        // 传统方式：直接在对象上设置属性
        target_object->set_property(relate_property.property_name, value);
    }
}

void property_helper::connect_property_listener(abstract_object&      target_object,
                                                const std::string&    property_name,
                                                std::function<void()> callback) {
    target_object.property_changed().connect(
        [property_name, callback](const mc::variant& value, const property_base& prop) {
        if (prop.get_name() == property_name) {
            callback();
        }
    });
}

void property_helper::disconnect_all_connections(std::vector<mc::connection_type>& connection_slots) {
    // 主动断开所有连接
    for (auto& slot : connection_slots) {
        slot.disconnect();
    }
    // 清理现有的连接状态
    connection_slots.clear();
}

mc::dict property_helper::group_properties_by_object(const mc::dict& relate_properties) {
    mc::dict object_properties;

    for (const auto& entry : relate_properties) {
        auto relate_property = entry.value.template as<mc::expr::relate_property>();

        if (!object_properties.contains(relate_property.object_name)) {
            object_properties[relate_property.object_name] = mc::dict{};
        }

        auto object_property = object_properties[relate_property.object_name].as<mc::dict>();
        object_property.insert(relate_property.property_name, true);
        object_properties[relate_property.object_name] = object_property;
    }

    return object_properties;
}

} // namespace mc::engine