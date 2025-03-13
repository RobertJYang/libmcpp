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

/**
 * @file json.cpp
 * @brief 实现JSON编解码功能
 */
#include <mc/json.h>
#include <mc/dict.h>
#include <mc/variant.h>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <limits>
#include <cstdint>
#include <climits>
#include <algorithm>
#include <vector>

namespace mc {
namespace json {

// 用于JSON编码的辅助类
class encoder {
public:
    explicit encoder(const json_encode_options& options = {}) 
        : m_stream(), m_options(options), m_current_depth(0) 
    {
        // 规范化选项值
        m_options.normalize();
        
        // 如果指定了浮点数精度，设置流的精度
        if (m_options.float_precision >= 0) {
            m_stream.precision(m_options.float_precision);
        } else {
            m_stream.precision(std::numeric_limits<double>::max_digits10);
        }
    }

    // 编码入口函数
    std::string encode(const variant& value) {
        encode_value(value);
        return m_stream.str();
    }

private:
    std::ostringstream m_stream;
    json_encode_options m_options;
    int m_current_depth;

    // 检查嵌套深度
    void check_depth() {
        if (m_current_depth >= m_options.max_depth) {
            throw json_error("Maximum nesting depth exceeded");
        }
    }

    // 进入新的嵌套层级
    void enter_scope() {
        ++m_current_depth;
        check_depth();
    }

    // 离开嵌套层级
    void leave_scope() {
        --m_current_depth;
    }

    // 添加缩进
    void add_indent() {
        if (m_options.pretty_print) {
            m_stream << '\n' << std::string(m_current_depth * m_options.indent_size, ' ');
        }
    }

    // 添加分隔符
    void add_separator() {
        if (m_options.pretty_print) {
            m_stream << ' ';
        }
    }

    // 编码字符串
    void encode_string(const std::string& str) {
        m_stream << '"';
        for (unsigned char c : str) {
            switch (c) {
                case '"': m_stream << "\\\""; break;
                case '\\': m_stream << "\\\\"; break;
                case '\b': m_stream << "\\b"; break;
                case '\f': m_stream << "\\f"; break;
                case '\n': m_stream << "\\n"; break;
                case '\r': m_stream << "\\r"; break;
                case '\t': m_stream << "\\t"; break;
                default:
                    if (c < 0x20) {
                        m_stream << "\\u" << std::hex << std::setw(4) 
                                << std::setfill('0') << static_cast<int>(c);
                    } else if (m_options.escape_non_ascii && c >= 0x80) {
                        // 转义非ASCII字符
                        m_stream << "\\u" << std::hex << std::setw(4) 
                                << std::setfill('0') << static_cast<int>(c);
                    } else {
                        m_stream << c;
                    }
            }
        }
        m_stream << '"';
    }

    // 编码数字
    void encode_number(double num) {
        if (std::isfinite(num)) {
            if (m_options.float_precision >= 0) {
                // 使用 fixed 和 setprecision 控制小数位数
                m_stream << std::fixed << std::setprecision(m_options.float_precision) << num;
            } else {
                // 使用默认精度
                m_stream << std::defaultfloat << num;
            }
        } else {
            throw json_error("Invalid number value");
        }
    }

    // 编码数组
    void encode_array(const variants& arr) {
        enter_scope();
        m_stream << '[';
        bool first = true;
        for (const auto& item : arr) {
            if (!first) {
                m_stream << ',';
                add_separator();
            }
            if (m_options.pretty_print && !first) {
                add_indent();
            }
            encode_value(item);
            first = false;
        }
        if (m_options.pretty_print && !first) {
            add_indent();
        }
        m_stream << ']';
        leave_scope();
    }

    // 编码对象
    void encode_object(const dict& obj) {
        enter_scope();
        m_stream << '{';

        // 获取所有键并可能排序
        std::vector<std::reference_wrapper<const std::string>> keys;
        keys.reserve(obj.size());
        for (const auto& entry : obj) {
            keys.push_back(std::cref(entry.key));
        }
        if (m_options.sort_keys) {
            std::sort(keys.begin(), keys.end(), 
                [](const auto& a, const auto& b) { return a.get() < b.get(); });
        }

        bool first = true;
        for (const auto& key_ref : keys) {
            const auto& key = key_ref.get();
            if (!first) {
                m_stream << ',';
                add_separator();
            }
            if (m_options.pretty_print && !first) {
                add_indent();
            }
            encode_string(key);
            m_stream << ':';
            add_separator();
            encode_value(obj[key]);
            first = false;
        }
        if (m_options.pretty_print && !first) {
            add_indent();
        }
        m_stream << '}';
        leave_scope();
    }

    // 编码任意值
    void encode_value(const variant& value) {
        switch (value.get_type()) {
            case variant::type_id::null_type:
                m_stream << "null";
                break;
            case variant::type_id::bool_type:
                m_stream << (value.as_bool() ? "true" : "false");
                break;
            case variant::type_id::int8_type:
                m_stream << static_cast<int16_t>(value.as_int8()); // 避免被当作字符处理
                break;
            case variant::type_id::uint8_type:
                m_stream << static_cast<uint16_t>(value.as_uint8()); // 避免被当作字符处理
                break;
            case variant::type_id::int16_type:
                m_stream << value.as_int16();
                break;
            case variant::type_id::uint16_type:
                m_stream << value.as_uint16();
                break;
            case variant::type_id::int32_type:
                m_stream << value.as_int32();
                break;
            case variant::type_id::uint32_type:
                m_stream << value.as_uint32();
                break;
            case variant::type_id::int64_type:
                m_stream << value.as_int64();
                break;
            case variant::type_id::uint64_type:
                m_stream << value.as_uint64();
                break;
            case variant::type_id::double_type:
                encode_number(value.as_double());
                break;
            case variant::type_id::string_type:
                encode_string(value.as_string());
                break;
            case variant::type_id::array_type:
                encode_array(value.get_array());
                break;
            case variant::type_id::object_type:
                encode_object(value.get_object());
                break;
            case variant::type_id::blob_type:
                encode_string(value.as_string()); // blob 类型转换为字符串处理
                break;
            default:
                throw json_error("Unsupported value type");
                break;
        }
    }
};

// 用于JSON解码的辅助类
class decoder {
public:
    explicit decoder(std::string_view json, const json_decode_options& options = {})
        : m_input(json), m_pos(0), m_options(options), m_current_depth(0)
    {
        m_options.normalize();
        
        // 检查输入JSON字符串的总长度
        if (m_input.length() > m_options.max_input_length) {
            throw json_error("Input JSON string length exceeds limit");
        }
    }

    // 解码入口函数
    variant decode() {
        skip_whitespace();
        variant result = parse_value();
        skip_whitespace();
        if (m_pos < m_input.length()) {
            throw json_error("Extra characters after JSON string");
        }
        return result;
    }

private:
    std::string_view m_input;
    size_t m_pos;
    json_decode_options m_options;
    int m_current_depth;

    // 跳过空白字符
    void skip_whitespace() {
        while (m_pos < m_input.length() && std::isspace(m_input[m_pos])) {
            ++m_pos;
        }
    }

    // 检查是否到达输入末尾
    bool is_eof() const {
        return m_pos >= m_input.length();
    }

    // 获取当前字符
    char current() const {
        return is_eof() ? '\0' : m_input[m_pos];
    }

    // 移动到下一个字符
    void advance() {
        if (!is_eof()) {
            ++m_pos;
        }
    }

    // 检查嵌套深度
    void check_depth() {
        if (m_current_depth >= m_options.max_depth) {
            throw json_error("Maximum nesting depth exceeded");
        }
    }

    // 进入新的嵌套层级
    void enter_scope() {
        ++m_current_depth;
        check_depth();
    }

    // 离开嵌套层级
    void leave_scope() {
        --m_current_depth;
    }

    // 解析值
    variant parse_value() {
        skip_whitespace();
        char c = current();
        switch (c) {
            case 'n': return parse_null();
            case 't': return parse_true();
            case 'f': return parse_false();
            case '"': return parse_string();
            case '[': return parse_array();
            case '{': return parse_object();
            default:
                if (c == '-' || std::isdigit(c)) {
                    return parse_number();
                }
                throw json_error("Invalid JSON value");
        }
    }

    // 解析null
    variant parse_null() {
        if (m_input.substr(m_pos, 4) == "null") {
            m_pos += 4;
            return variant();
        }
        throw json_error("Invalid null value");
    }

    // 解析true
    variant parse_true() {
        if (m_input.substr(m_pos, 4) == "true") {
            m_pos += 4;
            return variant(true);
        }
        throw json_error("Invalid true value");
    }

    // 解析false
    variant parse_false() {
        if (m_input.substr(m_pos, 5) == "false") {
            m_pos += 5;
            return variant(false);
        }
        throw json_error("Invalid false value");
    }

    // 解析字符串
    variant parse_string() {
        advance(); // 跳过开始的双引号
        std::string result;
        while (!is_eof()) {
            char c = current();
            if (c == '"') {
                advance(); // 跳过结束的双引号
                if (result.length() > m_options.max_string_length) {
                    throw json_error("String length exceeds limit");
                }
                return variant(result);
            }
            if (c == '\\') {
                advance();
                if (is_eof()) {
                    throw json_error("String not properly terminated");
                }
                c = current();
                switch (c) {
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case '/': result += '/'; break;
                    case 'b': result += '\b'; break;
                    case 'f': result += '\f'; break;
                    case 'n': result += '\n'; break;
                    case 'r': result += '\r'; break;
                    case 't': result += '\t'; break;
                    case 'u': {
                        // 解析Unicode转义序列
                        if (m_pos + 4 >= m_input.length()) {
                            throw json_error("Invalid Unicode escape sequence");
                        }
                        std::string hex = std::string(m_input.substr(m_pos + 1, 4));
                        
                        // 检查是否包含有效的十六进制字符
                        for (char c : hex) {
                            if (!std::isxdigit(c)) {
                                throw json_error("Invalid Unicode escape sequence");
                            }
                        }
                        
                        try {
                            int code_point = std::stoi(hex, nullptr, 16);
                            // 这里简化处理，只支持基本的ASCII字符
                            if (code_point <= 0x7F) {
                                result += static_cast<char>(code_point);
                            } else {
                                // 对于非ASCII字符，直接输出对应的UTF-8编码
                                if (code_point <= 0x7FF) {
                                    result += static_cast<char>(0xC0 | (code_point >> 6));
                                    result += static_cast<char>(0x80 | (code_point & 0x3F));
                                } else if (code_point <= 0xFFFF) {
                                    result += static_cast<char>(0xE0 | (code_point >> 12));
                                    result += static_cast<char>(0x80 | ((code_point >> 6) & 0x3F));
                                    result += static_cast<char>(0x80 | (code_point & 0x3F));
                                } else {
                                    throw json_error("Unsupported Unicode character");
                                }
                            }
                            m_pos += 4;
                        } catch (...) {
                            throw json_error("Invalid Unicode escape sequence");
                        }
                        break;
                    }
                    default:
                        throw json_error("Invalid escape sequence");
                }
            } else if (static_cast<unsigned char>(c) < 0x20) {
                throw json_error("String contains invalid character");
            } else {
                result += c;
            }
            advance();
        }
        throw json_error("String not properly terminated");
    }

    // 解析数字
    variant parse_number() {
        size_t start = m_pos;
        bool is_float = false;
        bool is_negative = false;

        // 处理负号
        if (current() == '-') {
            is_negative = true;
            advance();
        }

        // 处理整数部分
        if (current() == '0') {
            advance();
        } else if (std::isdigit(current())) {
            while (!is_eof() && std::isdigit(current())) {
                advance();
            }
        } else {
            throw json_error("Invalid number format");
        }

        // 处理小数部分
        if (current() == '.') {
            is_float = true;
            advance();
            if (!std::isdigit(current())) {
                throw json_error("Decimal point must be followed by digits");
            }
            while (!is_eof() && std::isdigit(current())) {
                advance();
            }
        }

        // 处理指数部分
        if (current() == 'e' || current() == 'E') {
            is_float = true;
            advance();
            if (current() == '+' || current() == '-') {
                advance();
            }
            if (!std::isdigit(current())) {
                throw json_error("Exponent must contain digits");
            }
            while (!is_eof() && std::isdigit(current())) {
                advance();
            }
        }

        std::string_view num_str = m_input.substr(start, m_pos - start);
        
        try {
            if (is_float) {
                return variant(std::stod(std::string(num_str)));
            }

            // 对于整数，我们需要根据值的范围选择合适的类型
            if (is_negative) {
                int64_t val = std::stoll(std::string(num_str));
                if (val >= INT8_MIN && val <= INT8_MAX) {
                    return variant(static_cast<int8_t>(val));
                } else if (val >= INT16_MIN && val <= INT16_MAX) {
                    return variant(static_cast<int16_t>(val));
                } else if (val >= INT32_MIN && val <= INT32_MAX) {
                    return variant(static_cast<int32_t>(val));
                } else {
                    return variant(val);
                }
            } else {
                uint64_t val = std::stoull(std::string(num_str));
                if (val <= UINT8_MAX) {
                    return variant(static_cast<uint8_t>(val));
                } else if (val <= UINT16_MAX) {
                    return variant(static_cast<uint16_t>(val));
                } else if (val <= UINT32_MAX) {
                    return variant(static_cast<uint32_t>(val));
                } else {
                    return variant(val);
                }
            }
        } catch (...) {
            throw json_error("Number conversion failed");
        }
    }

    // 解析数组
    variant parse_array() {
        enter_scope();  // 进入数组作用域
        advance(); // 跳过开始的方括号
        variants result;
        skip_whitespace();
        
        if (current() == ']') {
            advance();
            leave_scope();  // 离开数组作用域
            return variant(result);
        }

        while (true) {
            if (result.size() >= m_options.max_array_size) {
                throw json_error("Array element count exceeds limit");
            }
            
            result.push_back(parse_value());
            skip_whitespace();
            
            if (current() == ']') {
                advance();
                leave_scope();  // 离开数组作用域
                return variant(result);
            }
            
            if (current() != ',') {
                throw json_error("Array elements must be separated by commas");
            }
            advance();
            skip_whitespace();
        }
    }

    // 解析对象
    variant parse_object() {
        enter_scope();  // 进入对象作用域
        advance(); // 跳过开始的大括号
        mutable_dict result;
        skip_whitespace();
        
        if (current() == '}') {
            advance();
            leave_scope();  // 离开对象作用域
            return variant(result);
        }

        while (true) {
            if (result.size() >= m_options.max_object_size) {
                throw json_error("Object key-value pair count exceeds limit");
            }

            if (current() != '"') {
                throw json_error("Object key must be a string");
            }
            
            std::string key = parse_string().as<std::string>();
            skip_whitespace();
            
            if (current() != ':') {
                throw json_error("Object key-value pairs must be separated by colons");
            }
            advance();
            
            variant value = parse_value();
            result(key, value);
            
            skip_whitespace();
            if (current() == '}') {
                advance();
                leave_scope();  // 离开对象作用域
                return variant(result);
            }
            
            if (current() != ',') {
                throw json_error("Object key-value pairs must be separated by commas");
            }
            advance();
            skip_whitespace();
        }
    }
};

// 实现json_encode函数
std::string json_encode(const variant& value, const json_encode_options& options) {
    return encoder(options).encode(value);
}

// 实现json_decode函数
variant json_decode(std::string_view json, const json_decode_options& options) {
    return decoder(json, options).decode();
}

} // namespace json
} // namespace mc 