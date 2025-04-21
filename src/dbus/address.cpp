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

#include <mc/dbus/address.h>
#include <mc/dbus/validator.h>
#include <mc/exception.h>
#include <mc/log.h>

#include <iomanip>
#include <sstream>

namespace mc {
namespace dbus {

// address_entry 实现
address_entry::address_entry(std::string method) : m_method(std::move(method)) {
}

const std::string& address_entry::get_method() const {
    return m_method;
}

void address_entry::set_value(std::string key, std::string value) {
    if (!validator::is_valid_member_name(key)) {
        MC_THROW(mc::invalid_arg_exception, "无效的DBus地址键名: '${key}'", ("key", key));
    }

    m_values.emplace(std::move(key), std::move(value));
}

std::string_view address_entry::get_value(const std::string& key) const {
    auto it = m_values.find(key);
    if (it != m_values.end()) {
        return it->second;
    }

    return {};
}

bool address_entry::has_key(const std::string& key) const {
    return m_values.find(key) != m_values.end();
}

const address_entry::key_value_map& address_entry::get_values() const {
    return m_values;
}

std::string address_entry::to_string() const {
    std::ostringstream oss;
    oss << m_method << ":";

    bool first = true;
    for (const auto& [key, value] : m_values) {
        if (!first) {
            oss << ",";
        }
        first = false;
        oss << key << "=" << address::escape_value(value);
    }

    return oss.str();
}

static std::string parse_method(std::string_view address_str, size_t& pos) {
    size_t method_end = address_str.find(':', pos);
    if (method_end == std::string::npos) {
        MC_THROW(mc::parse_error_exception, "DBus地址字符串格式错误: 缺少冒号");
    }
    std::string_view method_view = address_str.substr(pos, method_end - pos);
    if (method_view.empty()) {
        MC_THROW(mc::parse_error_exception, "DBus地址字符串格式错误: 传输方法为空");
    }
    pos = method_end + 1;
    return std::string{method_view};
}

static std::string parse_key(std::string_view address_str, size_t& pos) {
    size_t key_end = address_str.find('=', pos);
    if (key_end == std::string::npos) {
        MC_THROW(mc::parse_error_exception, "DBus地址字符串格式错误: 缺少等号");
    }
    std::string_view key_view = address_str.substr(pos, key_end - pos);
    if (key_view.empty()) {
        MC_THROW(mc::parse_error_exception, "DBus地址字符串格式错误: 键为空");
    }

    if (!validator::is_valid_member_name(key_view)) {
        MC_THROW(mc::parse_error_exception, "DBus地址字符串格式错误: 无效的键名 '${key}'",
                 ("key", key_view));
    }

    pos = key_end + 1;
    return std::string{key_view};
}

static std::string parse_value(std::string_view address_str, size_t& pos) {
    size_t value_end = pos;
    while (value_end < address_str.size()) {
        if (address_str[value_end] == '%') {
            // 处理百分号转义序列
            if (value_end + 2 >= address_str.size()) {
                MC_THROW(mc::parse_error_exception, "DBus地址字符串格式错误: 转义序列不完整");
            }
            // 跳过转义序列
            value_end += 3;
        } else if (address_str[value_end] == ',' || address_str[value_end] == ';') {
            // 遇到分隔符，结束值
            break;
        } else {
            // 普通字符
            value_end++;
        }
    }

    auto value     = address_str.substr(pos, value_end - pos);
    auto unescaped = address::unescape_value(value);
    if (!unescaped) {
        MC_THROW(mc::parse_error_exception, "DBus地址字符串格式错误: 转义序列无效");
    }

    pos = value_end;
    return *unescaped;
}

address address::parse(std::string_view address_str) {
    address result;

    if (address_str.empty()) {
        MC_THROW(mc::parse_error_exception, "DBus地址字符串不能为空");
    }

    size_t pos = 0;
    while (pos < address_str.size()) {
        std::string method = parse_method(address_str, pos);

        auto entry = std::make_shared<address_entry>(std::move(method));
        while (pos < address_str.size()) {
            std::string key   = parse_key(address_str, pos);
            std::string value = parse_value(address_str, pos);
            entry->set_value(key, value);
            if (pos >= address_str.size()) {
                break;
            } else if (address_str[pos] == ';') {
                pos++;
                break; // 遇到分隔符，结束当前地址条目
            } else if (address_str[pos] == ',') {
                pos++; // 遇到逗号，继续解析下一个键值对
            } else {
                MC_THROW(mc::parse_error_exception,
                         "DBus地址格式错误: ${address} 在位置 ${pos} 遇到意外的终止字符 ${char}",
                         ("address", address_str)("pos", pos)("char", address_str[pos]));
            }
        }

        if (entry->get_values().empty()) {
            wlog("DBus地址字符串格式错误: 方法 ${method} 条目不能为空，跳过该方法",
                 ("method", entry->get_method()));
        } else {
            result.add_entry(std::move(entry));
        }
    }

    if (result.get_entries().empty()) {
        MC_THROW(mc::parse_error_exception, "DBus地址字符串格式错误: 没有解析到任何地址");
    }

    return result;
}

void address::add_entry(address_entry_ptr entry) {
    m_entries.push_back(std::move(entry));
}

const address::entry_list& address::get_entries() const {
    return m_entries;
}

std::string address::to_string() const {
    std::ostringstream oss;

    bool first = true;
    for (const auto& entry : m_entries) {
        if (!first) {
            oss << ";";
        }
        first = false;
        oss << entry->to_string();
    }

    return oss.str();
}

// 判断字符是否需要被转义
static bool is_optionally_escaped(char c) {
    // 允许的字符：字母、数字、下划线、/、-、@、.
    return !((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
             c == '_' || c == '/' || c == '-' || c == '@' || c == '.');
}

std::string address::escape_value(std::string_view value) {
    std::string result;
    result.reserve(value.size() * 2);

    for (char c : value) {
        if (is_optionally_escaped(c)) {
            char hex[4];
            snprintf(hex, sizeof(hex), "%%%02X", static_cast<unsigned char>(c));
            result += hex;
        } else {
            result += c;
        }
    }

    return result;
}

std::optional<std::string> address::unescape_value(std::string_view value) {
    std::string result;
    result.reserve(value.size());

    size_t i = 0;
    while (i < value.size()) {
        if (value[i] != '%') {
            result += value[i];
            i++;
            continue;
        }

        if (i + 2 >= value.size()) {
            return std::nullopt;
        }

        char  hex[3]    = {value[i + 1], value[i + 2], 0};
        char* end_ptr   = nullptr;
        long  hex_value = strtol(hex, &end_ptr, 16);
        if (*end_ptr != '\0' || hex_value < 0 || hex_value > 255) {
            return std::nullopt;
        }

        result += static_cast<char>(hex_value);
        i += 3;
    }

    return result;
}

} // namespace dbus
} // namespace mc