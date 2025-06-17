/*
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <iostream>
#include <map>
#include <mc/engine/base.h>
#include <mc/expr/engine.h>
#include <mc/expr/function/call.h>
#include <mc/expr/function/collection.h>
#include <mc/log.h>
#include <memory>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace mc::expr {

void func::validate_result() {
    if (m_result == "") {
        MC_THROW(mc::parse_error_exception, "Func result is not initialized");
    }
}

void func::validate_args() {
    if (m_args.empty()) {
        MC_THROW(mc::parse_error_exception, "Func args Compatible is empty");
    }
}

bool is_relate_property(const mc::variant& value) {
    // 检查是否为字典类型
    if (value.get_type() != mc::type_id::object_type) {
        return false;
    }

    const auto& dict = value.as_dict();
    if (dict.size() != 4) {
        return false;
    }
    // 检查是否包含 object_name 和 property_name
    auto type_it = dict.find("type");
    auto obj_it  = dict.find("object_name");
    auto prop_it = dict.find("property_name");
    auto full_it = dict.find("full_name");

    if (type_it == dict.end() || obj_it == dict.end() || prop_it == dict.end() || full_it == dict.end()) {
        return false;
    }

    // 检查两个字段的值是否都是字符串类型
    if (!type_it->value.is_string() || !obj_it->value.is_string() || !prop_it->value.is_string()) {
        return false;
    }

    if (!full_it->value.is_string()) {
        return false;
    }

    return true;
}

// 处理函数调用
mc::variant handle_function_call(const mc::variant& call_value, const std::string_view& position) {
    auto functions = func_collection::get_instance().get(position);

    // 创建一个可变的字典来存储参数
    mc::mutable_dict func_params = call_value["params"].as_mutable_dict();
    auto             func_obj    = functions[call_value["func"]];

    if (func_obj.is_null()) {
        elog("Function not found: ${func_name}", ("func_name", call_value["func"].as<std::string>()));
        return mc::variant();
    }

    return func_obj.as<func>().call(position, func_params);
}

// 处理属性引用
mc::variant handle_relate_property(const mc::variant& prop_value, const std::string_view& position) {
    auto service = func_collection::get_instance().get_service(position);
    if (service == nullptr) {
        elog("Service not found: ${object_name}", ("object_name", prop_value["object_name"].as<std::string>()));
        return mc::variant();
    }

    // 构造对象名称字符串
    std::string object_name = prop_value["object_name"].as<std::string>() + "_" + std::string(position);

    // 查找对象
    auto& object_table = service->get_object_table();
    auto& idx          = object_table.get<mc::engine::by_object_name>();
    auto  obj_it       = idx.find(object_name);

    if (obj_it == idx.end()) {
        elog("Object not found: ${object_name}", ("object_name", object_name));
        return mc::variant();
    }

    auto& device = const_cast<mc::engine::abstract_object&>(*obj_it);
    return device.get_property(prop_value["property_name"].as<std::string>());
}

// 处理单个参数的变量注册
void handle_parameter_registration(mc::expr::context& ctx, const std::string& key,
                                   const mc::variant&      param_value,
                                   const std::string_view& position) {
    if (is_function_call(param_value)) {
        // 处理函数调用
        mc::variant result = handle_function_call(param_value, position);
        ctx.register_variable(key, result);
    } else if (is_relate_property(param_value)) {
        // 处理属性引用
        mc::variant result = handle_relate_property(param_value, position);
        ctx.register_variable(key, result);
    } else {
        // 直接使用参数值
        ctx.register_variable(key, param_value);
    }
}

mc::variant func::call(const std::string_view& position, mc::mutable_dict& params) {
    mc::expr::engine engine;
    auto&            ctx = engine.get_global_context();

    // 处理所有参数
    for (auto& item : m_args) {
        std::string key = item.key.as<std::string>();
        auto        it  = params.find(key);

        if (it == params.end()) {
            // 使用默认值
            handle_parameter_registration(ctx, key, item.value, position);
        } else {
            // 处理传入的参数
            handle_parameter_registration(ctx, key, it->value, position);
        }
    }

    return engine.evaluate(m_result, ctx);
}

mc::mutable_dict func::get_relate_properties(const std::string_view& position, mc::mutable_dict& params) {
    mc::mutable_dict relate_properties;
    // 处理所有参数
    for (auto& item : m_args) {
        std::string key = item.key.as<std::string>();
        auto        it  = params.find(key);

        if (it == params.end()) {
            // 使用默认值
            if (is_relate_property(item.value)) {
                relate_properties[item.value["full_name"].as<std::string>()] = item.value;
            }
        } else {
            // 处理传入的参数
            if (is_function_call(it->value)) {
                // 处理函数调用
                auto child_params = it->value["params"].as_mutable_dict();
                auto func_name    = it->value["func"].as<std::string>();
                auto func_obj     = func_collection::get_instance().get(position, func_name);
                if (func_obj.is_null()) {
                    elog("Function not found: ${func_name}", ("func_name", func_name));
                    continue;
                }

                auto child_rps = func_obj.as<func>().get_relate_properties(position, child_params);
                for (const auto& entry : child_rps) {
                    relate_properties[entry.key] = entry.value;
                }
            } else if (is_relate_property(it->value)) {
                relate_properties[it->value["full_name"].as<std::string>()] = mc::variant(it->value);
            }
        }
    }
    return relate_properties;
}

} // namespace mc::expr