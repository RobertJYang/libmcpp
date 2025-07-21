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
#include <mc/exception.h>
#include <mc/expr/engine.h>
#include <mc/expr/function/parser.h>
#include <memory>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace mc;

namespace mc::expr {

func_parser& func_parser::get_instance() {
    static func_parser instance;
    return instance;
}

// 尝试将字符串转换为不同类型的值
mc::variant parse_value(const std::string& value) {
    // 尝试转换为布尔值
    if (value == "true" || value == "True" || value == "TRUE") {
        return variant(true);
    }
    if (value == "false" || value == "False" || value == "FALSE") {
        return variant(false);
    }

    // 尝试转换为整数
    try {
        return variant(static_cast<int32_t>(std::stoi(value)));
    } catch (...) {
        // 转换失败，继续尝试其他类型
    }

    // 尝试转换为浮点数
    try {
        return variant(std::stod(value));
    } catch (...) {
        // 转换失败，作为字符串处理
    }

    // 如果都不是，则作为字符串处理
    return variant(value);
}

std::pair<std::string, std::string> func_parser::parse_dotted_property(const std::string& input) {
    // 支持两种格式：
    // 1. AAA[bmc.dev.xxx].DeviceName - 带接口的新语法
    // 2. AAA.DeviceName - 传统语法（向后兼容）

    std::regex  pattern_with_interface(R"(([^.\[]+)\[([^\]]+)\]\.([^.]+))");
    std::smatch matches;

    if (std::regex_match(input, matches, pattern_with_interface)) {
        // 新语法：AAA[bmc.dev.xxx].DeviceName
        return {matches[1].str(), matches[3].str()}; // 返回对象名和属性名
    } else {
        // 传统语法：AAA.DeviceName
        std::regex traditional_pattern(R"(([^.]+)\.([^.]+))");
        if (!std::regex_match(input, matches, traditional_pattern)) {
            MC_THROW(mc::invalid_arg_exception, "Invalid property format: ${input}", ("input", input));
        }
        return {matches[1].str(), matches[2].str()}; // 返回对象名和属性名
    }
}

// 通用属性解析函数
relate_property func_parser::parse_property_with_type(const std::string&          input,
                                                      const property_type_config& config) {
    // 验证前缀格式
    if (input.length() < config.prefix.length() ||
        input.substr(0, config.prefix.length()) != config.prefix) {
        MC_THROW(mc::invalid_arg_exception, "${error_prefix}属性格式无效: ${input}",
                 ("error_prefix", config.error_message_prefix)("input", input));
    }

    // 提取去除前缀后的内容
    std::string dotted_part = input.substr(config.prefix.length());

    // 解析接口信息
    std::string interface_name;
    std::regex  pattern_with_interface(R"(([^.\[]+)\[([^\]]+)\]\.([^.]+))");
    std::smatch matches;

    std::string obj_name, prop_name;

    if (std::regex_match(dotted_part, matches, pattern_with_interface)) {
        // 新语法：AAA[bmc.dev.xxx].DeviceName
        obj_name       = matches[1].str();
        interface_name = matches[2].str();
        prop_name      = matches[3].str();
    } else {
        // 传统语法：AAA.DeviceName
        auto [object_name, property_name] = parse_dotted_property(dotted_part);
        obj_name                          = object_name;
        prop_name                         = property_name;
        interface_name                    = ""; // 传统语法接口为空
    }

    // 构造结果
    relate_property prop;
    if (!config.type_name.empty()) {
        prop.type = config.type_name;
    }
    prop.object_name   = obj_name;
    prop.property_name = prop_name;
    prop.full_name     = input;
    prop.interface     = interface_name;

    return prop;
}

relate_property func_parser::parse_property(const std::string& input) {
    // 解析接口信息
    std::string interface_name;
    std::regex  pattern_with_interface(R"(([^.\[]+)\[([^\]]+)\]\.([^.]+))");
    std::smatch matches;

    std::string obj_name, prop_name;

    if (std::regex_match(input, matches, pattern_with_interface)) {
        // 新语法：AAA[bmc.dev.xxx].DeviceName
        obj_name       = matches[1].str();
        interface_name = matches[2].str();
        prop_name      = matches[3].str();
    } else {
        // 传统语法：AAA.DeviceName
        auto [object_name, property_name] = parse_dotted_property(input);
        obj_name                          = object_name;
        prop_name                         = property_name;
        interface_name                    = ""; // 传统语法接口为空
    }

    relate_property prop;
    prop.object_name   = obj_name;
    prop.property_name = prop_name;
    prop.full_name     = input; // 设置完整名称
    prop.interface     = interface_name;
    return prop;
}

relate_property func_parser::parse_sync_property(const std::string& input) {
    static const property_type_config config = {"<=/", "sync", "同步"};
    return parse_property_with_type(input, config);
}

relate_property func_parser::parse_ref_property(const std::string& input) {
    static const property_type_config config = {"#/", "ref", "引用"};
    return parse_property_with_type(input, config);
}

relate_object func_parser::parse_ref_object(const std::string& input) {
    // 验证格式 #/ObjectName
    if (input.length() < 3 || input.substr(0, 2) != "#/") {
        MC_THROW(mc::invalid_arg_exception, "引用对象格式无效: ${input}", ("input", input));
    }

    // 提取对象名，去除前缀 #/
    std::string object_name = input.substr(2);

    // 验证对象名不为空且只包含字母、数字和下划线
    if (object_name.empty() || !std::regex_match(object_name, std::regex(R"([A-Za-z_][A-Za-z0-9_]*)"))) {
        MC_THROW(mc::invalid_arg_exception, "引用对象名称格式无效: ${object_name}", ("object_name", object_name));
    }

    // 构造结果
    relate_object obj;
    obj.type        = "ref";
    obj.object_name = object_name;
    obj.full_name   = input;

    return obj;
}

std::string func_parser::parse_function_name(const std::string& input) {
    // 找到第一个 $Func_ 符号
    size_t start = input.find("$Func_");
    if (start == std::string::npos) {
        MC_THROW(mc::invalid_op_exception, "Invalid function name format: ${input}", ("input", input));
    }
    size_t end = start + 6;
    while (end < input.length() && (std::isalnum(input[end]) || input[end] == '_')) {
        ++end;
    }
    if (end <= start + 6) {
        MC_THROW(mc::invalid_op_exception, "Invalid function name format: ${input}", ("input", input));
    }
    std::string func_name = input.substr(start + 1, end - start - 1); // 包含Func_前缀
    if (!std::regex_match(func_name, std::regex(R"(Func_[A-Za-z_][A-Za-z0-9_]*)"))) {
        MC_THROW(mc::invalid_op_exception, "Invalid function name format: ${func_name}", ("func_name", func_name));
    }
    return func_name;
}

std::string func_parser::parse_params_string(const std::string& input) {
    // 找到第一个 { 和最后一个 }
    size_t start = input.find('{');
    size_t end   = input.rfind('}');

    // 如果没有找到花括号，说明是无参场景，返回空字符串
    if (start == std::string::npos || end == std::string::npos || start >= end) {
        return "";
    }

    return input.substr(start + 1, end - start - 1);
}

func_call func_parser::parse_function_call(const std::string& input) {
    func_call result;
    result.func            = parse_function_name(input);
    std::string params_str = parse_params_string(input);

    // 如果参数字符串为空或者是空的花括号，直接返回空参数列表
    if (params_str.empty() || params_str == "{}") {
        return result;
    }

    size_t pos = 0;
    while (pos < params_str.length()) {
        while (pos < params_str.length() && std::isspace(params_str[pos])) {
            ++pos;
        }
        if (pos >= params_str.length()) {
            break;
        }
        size_t name_start = pos;
        while (pos < params_str.length() &&
               (std::isalnum(params_str[pos]) || params_str[pos] == '_')) {
            ++pos;
        }
        std::string param_name = params_str.substr(name_start, pos - name_start);
        while (pos < params_str.length() &&
               (std::isspace(params_str[pos]) || params_str[pos] == ':')) {
            ++pos;
        }
        if (pos >= params_str.length()) {
            break;
        }
        size_t      value_start = pos;
        std::string param_value;
        if (params_str[pos] == '$') {
            size_t start = pos;
            ++pos;
            while (pos < params_str.length() && params_str[pos] != '{') {
                ++pos;
            }
            if (pos == params_str.length()) {
                MC_THROW(mc::invalid_arg_exception, "Invalid function call format: missing {");
            }
            ++pos;
            int brace_count = 1;
            while (pos < params_str.length() && brace_count > 0) {
                if (params_str[pos] == '{') {
                    ++brace_count;
                } else if (params_str[pos] == '}') {
                    --brace_count;
                }
                ++pos;
            }
            if (brace_count != 0) {
                MC_THROW(mc::invalid_arg_exception, "Invalid function call format: braces not matched");
            }
            param_value = params_str.substr(start, pos - start);
        } else if (params_str[pos] == '"') {
            ++pos;
            while (pos < params_str.length() && params_str[pos] != '"') {
                ++pos;
            }
            ++pos;
            param_value = params_str.substr(value_start, pos - value_start);
        } else {
            while (pos < params_str.length() && params_str[pos] != ',' && params_str[pos] != '}' &&
                   params_str[pos] != ')') {
                ++pos;
            }
            param_value = params_str.substr(value_start, pos - value_start);
        }
        if (param_value[0] == '"') {
            result.params[param_name] =
                mc::variant(param_value.substr(1, param_value.length() - 2));
        } else if (param_value == "true" || param_value == "false") {
            result.params[param_name] = mc::variant(param_value == "true");
        } else if (std::regex_match(param_value, std::regex(R"(-?\d+\.\d+)"))) {
            result.params[param_name] = mc::variant(std::stod(param_value));
        } else if (std::regex_match(param_value, std::regex(R"(-?\d+)"))) {
            result.params[param_name] = mc::variant(static_cast<int32_t>(std::stoi(param_value)));
        } else if (param_value[0] == '$') {
            auto             nested_call = parse_function_call(param_value);
            mc::mutable_dict nested_dict;
            nested_dict.insert("func", mc::variant(nested_call.func));
            mc::mutable_dict params_dict;
            for (const auto& param : nested_call.params) {
                params_dict.insert(param.key, param.value);
            }
            nested_dict.insert("params", mc::variant(params_dict));
            result.params[param_name] = mc::variant(nested_dict);
        } else if (param_value.substr(0, 3) == "<=/") {
            result.params[param_name] = mc::variant(parse_sync_property(param_value));
        } else if (param_value.substr(0, 2) == "#/") {
            // 根据是否包含点号来区分引用对象和引用属性
            if (param_value.find('.') != std::string::npos) {
                result.params[param_name] = mc::variant(parse_ref_property(param_value));
            } else {
                result.params[param_name] = mc::variant(parse_ref_object(param_value));
            }
        } else if (std::regex_match(
                       param_value,
                       std::regex(R"([A-Za-z_][A-Za-z0-9_]*\.[A-Za-z_][A-Za-z0-9_]*)"))) {
            result.params[param_name] = mc::variant(parse_property(param_value));
        } else {
            MC_THROW(mc::invalid_arg_exception, "Invalid parameter value: ${param_value}", ("param_value", param_value));
        }
        while (pos < params_str.length() && (std::isspace(params_str[pos]) ||
                                             params_str[pos] == ',' || params_str[pos] == ')')) {
            ++pos;
        }
    }
    return result;
}

bool param_value_comparator::operator()(const mc::variant& a, const mc::variant& b) const {
    if (a.get_type() != b.get_type()) {
        return false;
    }

    switch (a.get_type()) {
    case mc::type_id::string_type:
        return a.as_string() == b.as_string();
    case mc::type_id::int8_type:
        return a.as_int8() == b.as_int8();
    case mc::type_id::double_type:
        return a.as_double() == b.as_double();
    case mc::type_id::bool_type:
        return a.as_bool() == b.as_bool();
    case mc::type_id::object_type:
        return compare_dicts(a.as_dict(), b.as_dict());
    default:
        return false;
    }
}

bool param_value_comparator::compare_dicts(const mc::dict& a, const mc::dict& b) const {
    if (a.size() != b.size()) {
        return false;
    }

    for (const auto& entry : a) {
        auto it = b.find(entry.key);
        if (it == b.end()) {
            return false;
        }
        if (!(*this)(entry.value, it->value)) {
            return false;
        }
    }
    return true;
}

} // namespace mc::expr

MC_REFLECT(mc::expr::relate_property, ((type, "type"))((object_name, "object_name"))((property_name, "property_name"))((full_name, "full_name"))((interface, "interface")));
MC_REFLECT(mc::expr::relate_object, ((type, "type"))((object_name, "object_name"))((full_name, "full_name")));
