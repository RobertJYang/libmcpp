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

#include <mc/error_message_converter.h>
#include <mc/error_message_parser.h>
#include <mc/log.h>

#include <sstream>

namespace mc {

mc::dict standard_error_message::to_dict() const
{
    mc::dict result;
    result["MessageId"] = message_id;
    result["Message"]   = message;
    result["Severity"]  = severity;

    // 添加参数
    if (!message_args.empty()) {
        mc::variants args_list;
        for (auto it = message_args.begin(); it != message_args.end(); ++it) {
            args_list.push_back((*it).value);
        }
        result["MessageArgs"] = args_list;
    }

    // 可选字段
    if (!resolution.empty()) {
        result["Resolution"] = resolution;
    }

    // 添加相关属性列表（Redfish 规范）
    if (!related_properties.empty()) {
        mc::variants props_list;
        for (const auto& prop : related_properties) {
            props_list.push_back(prop);
        }
        result["RelatedProperties"] = props_list;
    }

    return result;
}

void error_message_converter::load_registries(std::string_view base_json_path, std::string_view custom_json_path)
{
    ilog("加载标准错误定义: base=${base}, custom=${custom}", ("base", base_json_path)("custom", custom_json_path));

    std::lock_guard<std::mutex> lock(m_mutex);

    m_base_registry = error_message_parser::parse_from_file(base_json_path);

    m_custom_registry = error_message_parser::parse_from_file(custom_json_path);

    ilog("错误定义加载完成: base消息数=${base_count}, custom消息数=${custom_count}",
         ("base_count", m_base_registry.messages.size())("custom_count", m_custom_registry.messages.size()));
}

void error_message_converter::load_registries_from_string(std::string_view base_json, std::string_view custom_json)
{
    ilog("从字符串加载标准错误定义");

    std::lock_guard<std::mutex> lock(m_mutex);

    m_base_registry   = error_message_parser::parse_from_string(base_json);
    m_custom_registry = error_message_parser::parse_from_string(custom_json);

    ilog("错误定义加载完成: base消息数=${base_count}, custom消息数=${custom_count}",
         ("base_count", m_base_registry.messages.size())("custom_count", m_custom_registry.messages.size()));
}

std::optional<error_message_definition> error_message_converter::find_definition(std::string_view message_name) const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // 先在 custom 中查找
    auto def = error_message_parser::find_message(m_custom_registry, message_name);
    if (def.has_value()) {
        return def;
    }

    // 再在 base 中查找
    return error_message_parser::find_message(m_base_registry, message_name);
}

standard_error_message error_message_converter::convert(const mc::error&                err,
                                                        const std::vector<std::string>& related_properties) const
{
    standard_error_message result;

    std::string error_name = std::string(err.get_name());

    std::lock_guard<std::mutex> lock(m_mutex);

    // 先在 custom 中查找
    auto def       = error_message_parser::find_message(m_custom_registry, error_name);
    bool is_custom = def.has_value();

    // 如果 custom 中没有，再在 base 中查找
    if (!is_custom) {
        def = error_message_parser::find_message(m_base_registry, error_name);
    }

    if (def.has_value()) {
        // 找到定义，使用标准格式
        const auto& definition = def.value();

        // 确定使用哪个注册表
        std::string prefix  = is_custom ? m_custom_registry.registry_prefix : m_base_registry.registry_prefix;
        std::string version = is_custom ? m_custom_registry.registry_version : m_base_registry.registry_version;

        result.message_name     = error_name;
        result.message_id       = prefix + "." + version + "." + error_name;
        result.registry_prefix  = prefix;
        result.registry_version = version;
        result.http_status_code = definition.http_status_code;
        result.severity         = definition.severity;
        result.resolution       = definition.resolution;

        // 转换参数格式：使用数字 key（0, 1, 2...）而不是字符串 key
        mc::dict converted_args;
        int      arg_index = 0;
        for (auto it = err.get_args().begin(); it != err.get_args().end(); ++it) {
            converted_args[arg_index++] = (*it).value;
        }
        result.message_args = converted_args;

        // 格式化消息
        result.message = error_message_parser::format_message(definition, result.message_args);

        // 设置相关属性列表（Redfish 规范）
        result.related_properties = related_properties;

    } else {
        // 未找到定义，转换为 InternalError
        wlog("未找到错误定义: ${name}, 使用 InternalError", ("name", error_name));

        // 使用 base 注册表的信息
        result.message_name = "InternalError";
        result.message_id = m_base_registry.registry_prefix + "." + m_base_registry.registry_version + ".InternalError";
        result.registry_prefix  = m_base_registry.registry_prefix;
        result.registry_version = m_base_registry.registry_version;
        result.severity         = "Critical";
        result.http_status_code = 500;
        result.resolution       = "Restart the device. If the issue persists, contact support.";

        // 将原始错误信息作为参数传递（使用数字 key）
        mc::dict converted_args;
        converted_args[0]   = error_name;
        converted_args[1]   = err.get_message();
        result.message_args = converted_args;

        // 查找 InternalError 的定义
        auto internal_def = error_message_parser::find_message(m_base_registry, "GeneralError");
        if (internal_def.has_value()) {
            result.message = error_message_parser::format_message(internal_def.value(), result.message_args);
        } else {
            result.message = "An internal error occurred: " + err.get_message();
        }

        // 设置相关属性列表（Redfish 规范）
        result.related_properties = related_properties;
    }

    return result;
}

mc::dict error_message_converter::convert_to_dict(const mc::error&                err,
                                                  const std::vector<std::string>& related_properties) const
{
    standard_error_message std_msg = convert(err, related_properties);
    return std_msg.to_dict();
}

error_message_converter& error_message_converter::get_instance()
{
    static error_message_converter instance;
    return instance;
}

standard_error_message to_standard_message(const mc::error& err, const std::vector<std::string>& related_properties)
{
    return error_message_converter::get_instance().convert(err, related_properties);
}

mc::dict to_standard_message_dict(const mc::error& err, const std::vector<std::string>& related_properties)
{
    return error_message_converter::get_instance().convert_to_dict(err, related_properties);
}

} // namespace mc
