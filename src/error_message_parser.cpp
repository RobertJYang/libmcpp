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

#include <mc/error_message_parser.h>
#include <mc/exception.h>
#include <mc/filesystem.h>
#include <mc/json.h>
#include <mc/log.h>

#include <sstream>

namespace mc {

error_message_registry error_message_parser::parse_from_string(std::string_view json_content)
{
    error_message_registry registry;

    try {
        // 使用 mc::json 解析
        mc::variant var  = mc::json::json_decode(json_content);
        mc::dict    root = var.as<mc::dict>();

        // 解析基本字段
        if (root.contains("Description")) {
            registry.description = root["Description"].as<std::string>();
        }

        if (root.contains("RegistryPrefix")) {
            registry.registry_prefix = root["RegistryPrefix"].as<std::string>();
        }

        if (root.contains("RegistryVersion")) {
            registry.registry_version = root["RegistryVersion"].as<std::string>();
        }

        // 解析 Messages
        if (root.contains("Messages")) {
            mc::dict messages = root["Messages"].as<mc::dict>();

            // 使用迭代器遍历 dict
            for (auto it = messages.begin(); it != messages.end(); ++it) {
                std::string key   = (*it).key.as<std::string>();
                mc::variant value = (*it).value;

                error_message_definition def;
                mc::dict                 msg_dict = value.as<mc::dict>();

                // 解析各个字段
                if (msg_dict.contains("Description")) {
                    def.description = msg_dict["Description"].as<std::string>();
                }

                if (msg_dict.contains("Message")) {
                    def.message = msg_dict["Message"].as<std::string>();
                }

                if (msg_dict.contains("Severity")) {
                    def.severity = msg_dict["Severity"].as<std::string>();
                }

                if (msg_dict.contains("NumberOfArgs")) {
                    def.number_of_args = static_cast<int>(msg_dict["NumberOfArgs"].as<int64_t>());
                }

                if (msg_dict.contains("ParamTypes")) {
                    // ParamTypes 是数组类型
                    mc::variant param_types_var = msg_dict["ParamTypes"];
                    if (param_types_var.is_array()) {
                        for (size_t i = 0; i < param_types_var.size(); ++i) {
                            mc::variant param_type = param_types_var[i];
                            def.param_types.push_back(param_type.as<std::string>());
                        }
                    }
                }

                if (msg_dict.contains("Resolution")) {
                    def.resolution = msg_dict["Resolution"].as<std::string>();
                }

                if (msg_dict.contains("HttpStatusCode")) {
                    def.http_status_code = static_cast<int>(msg_dict["HttpStatusCode"].as<int64_t>());
                }

                if (msg_dict.contains("IpmiCompletionCode")) {
                    def.ipmi_completion_code = msg_dict["IpmiCompletionCode"].as<std::string>();
                }

                if (msg_dict.contains("SnmpStatusCode")) {
                    def.snmp_status_code = static_cast<int>(msg_dict["SnmpStatusCode"].as<int64_t>());
                }

                if (msg_dict.contains("TraceDepth")) {
                    def.trace_depth = static_cast<int>(msg_dict["TraceDepth"].as<int64_t>());
                }

                registry.messages[key] = std::move(def);
            }
        }

        ilog("成功解析错误注册表: ${prefix}, 版本 ${version}, 消息数量 ${count}",
             ("prefix", registry.registry_prefix)("version", registry.registry_version)("count",
                                                                                        registry.messages.size()));

    } catch (const mc::exception& e) {
        MC_THROW(mc::parse_error_exception, "解析错误定义文件失败: ${error}", ("error", e.what()));
    }

    return registry;
}

error_message_registry error_message_parser::parse_from_file(std::string_view file_path)
{
    ilog("Start to load error message definition file: ${path}", ("path", file_path));

    auto content_opt = mc::filesystem::read_file(file_path);
    if (!content_opt.has_value() || content_opt->empty()) {
        MC_THROW(mc::invalid_arg_exception, "Failed to read the error message definition file: ${path}",
                 ("path", file_path));
    }

    return parse_from_string(*content_opt);
}

std::optional<error_message_definition> error_message_parser::find_message(const error_message_registry& registry,
                                                                           std::string_view              message_name)
{
    auto it = registry.messages.find(std::string(message_name));
    if (it == registry.messages.end()) {
        return std::nullopt;
    }

    return it->second;
}

std::string error_message_parser::format_message(std::string_view template_msg, const mc::dict& args)
{
    std::string result = std::string(template_msg);

    // 使用正则表达式匹配 %数字 格式的占位符
    // 例如：%1, %2, %11, %123 等
    static const std::regex placeholder_pattern("%(\\d+)");

    std::string output;
    auto        it  = std::sregex_iterator(result.begin(), result.end(), placeholder_pattern);
    auto        end = std::sregex_iterator();

    size_t last_pos = 0;
    for (auto iter = it; iter != end; ++iter) {
        const std::smatch& match = *iter;
        // 添加匹配前的文本
        output.append(result, last_pos, match.position() - last_pos);

        // 提取数字并转换为 key_index（%1 -> key 0, %2 -> key 1, etc.）
        std::string num_str   = match.str(1);
        int         num       = std::stoi(num_str);
        int         key_index = num - 1;

        if (args.contains(key_index)) {
            // 类型转换：尝试获取字符串值
            std::string        value;
            const mc::variant& arg_value = args[key_index];

            // 根据类型进行转换
            switch (arg_value.get_type()) {
                case mc::type_id::string_type:
                    value = arg_value.as<std::string>();
                    break;
                case mc::type_id::int8_type:
                case mc::type_id::int16_type:
                case mc::type_id::int32_type:
                case mc::type_id::int64_type:
                    value = std::to_string(arg_value.as<int64_t>());
                    break;
                case mc::type_id::uint8_type:
                case mc::type_id::uint16_type:
                case mc::type_id::uint32_type:
                case mc::type_id::uint64_type:
                    value = std::to_string(arg_value.as<uint64_t>());
                    break;
                case mc::type_id::double_type:
                    value = std::to_string(arg_value.as<double>());
                    break;
                case mc::type_id::bool_type:
                    value = arg_value.as<bool>() ? "true" : "false";
                    break;
                default:
                    // 其他类型转换为字符串
                    value = mc::to_string(arg_value);
                    break;
            }

            output += value;
        } else {
            // 占位符存在但没有对应参数，保留原样并记录警告
            wlog("消息格式化缺少参数: 占位符 ${placeholder} 没有对应的参数值", ("placeholder", match.str()));
            output.append(match.str());
        }

        last_pos = match.position() + match.length();
    }

    // 添加剩余文本
    if (last_pos < result.length()) {
        output.append(result, last_pos, result.length() - last_pos);
    }

    return output;
}

std::string error_message_parser::format_message(const error_message_definition& def, const mc::dict& args)
{
    return format_message(def.message, args);
}

} // namespace mc
