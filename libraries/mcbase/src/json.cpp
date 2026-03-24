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
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <string>
#include <mc/dict.h>
#include <mc/exception.h>
#include <mc/json.h>
#include <mc/variant.h>
#include <sstream>

namespace mc {
namespace json {

namespace {

// 兼容无 std::from_chars 的工具链（如 GCC 7）：用栈缓冲 + C 库解析，避免 to_std_string() 额外分配
constexpr std::size_t k_json_number_buf = 512U;

bool parse_uint32_hex4(mc::string_view hex, std::uint32_t& out)
{
    if (hex.size() != 4U) {
        return false;
    }
    char buf[5];
    std::memcpy(buf, hex.data(), 4U);
    buf[4] = '\0';
    for (int i = 0; i < 4; ++i) {
        if (std::isxdigit(static_cast<unsigned char>(buf[i])) == 0) {
            return false;
        }
    }
    errno                 = 0;
    char*                 end = nullptr;
    unsigned long const v     = std::strtoul(buf, &end, 16);
    if (errno == ERANGE || end != buf + 4) {
        return false;
    }
    out = static_cast<std::uint32_t>(v);
    return true;
}

bool parse_json_number_token(mc::string_view sv, bool is_float, bool is_negative, variant& out)
{
    if (sv.empty() || sv.size() + 1U > k_json_number_buf) {
        return false;
    }
    char buf[k_json_number_buf];
    std::memcpy(buf, sv.data(), sv.size());
    buf[sv.size()] = '\0';

    errno    = 0;
    char* end = nullptr;
    if (is_float) {
        double const d = std::strtod(buf, &end);
        if (errno == ERANGE || end != buf + sv.size()) {
            return false;
        }
        out = d;
        return true;
    }
    if (is_negative) {
        long long const v = std::strtoll(buf, &end, 10);
        if (errno == ERANGE || end != buf + sv.size()) {
            return false;
        }
        out = v;
        return true;
    }
    unsigned long long const v = std::strtoull(buf, &end, 10);
    if (errno == ERANGE || end != buf + sv.size()) {
        return false;
    }
    out = v;
    return true;
}

} // namespace

// 用于JSON编码的辅助类
class encoder {
public:
    explicit encoder(const json_encode_options& options = json_encode_options::default_encode_options)
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
    mc::string encode(const variant& value)
    {
        encode_value(value);
        return mc::string(m_stream.str());
    }

    // 检查嵌套深度
    void check_depth()
    {
        if (m_current_depth >= m_options.max_depth) {
            MC_THROW(mc::parse_error_exception, "JSON nesting depth exceeds limit");
        }
    }

    // 进入新的嵌套层级
    void enter_scope()
    {
        ++m_current_depth;
        check_depth();
    }

    // 离开嵌套层级
    void leave_scope()
    {
        --m_current_depth;
    }

    // 添加缩进
    void add_indent()
    {
        if (m_options.pretty_print) {
            const auto indent = static_cast<std::size_t>(m_current_depth * m_options.indent_size);
            m_stream << '\n';
            for (std::size_t i = 0; i < indent; ++i) {
                m_stream.put(' ');
            }
        }
    }

    // 添加分隔符
    void add_separator()
    {
        if (m_options.pretty_print) {
            m_stream << ' ';
        }
    }

    // 编码字符串
    void encode_string(mc::string_view str)
    {
        m_stream << '"';
        for (unsigned char c : str) {
            switch (c) {
                case '"':
                    m_stream << "\\\"";
                    break;
                case '\\':
                    m_stream << "\\\\";
                    break;
                case '\b':
                    m_stream << "\\b";
                    break;
                case '\f':
                    m_stream << "\\f";
                    break;
                case '\n':
                    m_stream << "\\n";
                    break;
                case '\r':
                    m_stream << "\\r";
                    break;
                case '\t':
                    m_stream << "\\t";
                    break;
                default:
                    if (c < 0x20) {
                        m_stream << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
                    } else if (m_options.escape_non_ascii && c >= 0x80) {
                        // 转义非ASCII字符
                        m_stream << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
                    } else {
                        m_stream << c;
                    }
            }
        }
        m_stream << '"';
    }

    // 编码数字
    void encode_number(double num)
    {
        if (std::isfinite(num)) {
            if (m_options.float_precision >= 0) {
                // 使用 fixed 和 setprecision 控制小数位数
                m_stream << std::fixed << std::setprecision(m_options.float_precision) << num;
            } else {
                // 使用默认精度
                m_stream << std::defaultfloat << num;
            }
        } else {
            MC_THROW(mc::parse_error_exception, "Invalid number");
        }
    }

    // 编码数组
    void encode_array(const variants& arr)
    {
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
    void encode_object(const dict& obj)
    {
        enter_scope();
        m_stream << '{';

        // 获取所有键并可能排序
        std::vector<mc::string_view> keys;
        keys.reserve(obj.size());
        for (const auto& entry : obj) {
            keys.push_back(entry.key.get_string());
        }
        if (m_options.sort_keys) {
            std::sort(keys.begin(), keys.end(), [](const auto& a, const auto& b) {
                return a < b;
            });
        }

        bool first = true;
        for (const auto& key : keys) {
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
    void encode_value(const variant& value)
    {
        value.visit_with([this](auto&& v) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, std::nullptr_t>) {
                m_stream << "null";
            } else if constexpr (std::is_same_v<T, bool>) {
                m_stream << (v ? "true" : "false");
            } else if constexpr (std::is_integral_v<T>) {
                m_stream << v;
            } else if constexpr (std::is_floating_point_v<T>) {
                encode_number(v);
            } else if constexpr (std::is_same_v<T, mc::string_view>) {
                encode_string(v);
            } else if constexpr (std::is_same_v<T, typename variant::string_type>) {
                encode_string(v.view());
            } else if constexpr (std::is_same_v<T, typename variant::blob_type>) {
                encode_string(v.as_string_view());
            } else if constexpr (std::is_same_v<T, typename variant::array_type>) {
                encode_array(v);
            } else if constexpr (std::is_same_v<T, typename variant::object_type>) {
                encode_object(v);
            } else if constexpr (std::is_same_v<T, typename variant::extension_type>) {
                encode_string(v.as_string());
            } else {
                MC_THROW(mc::parse_error_exception, "Unsupported JSON value type ${type}",
                         ("type", mc::pretty_name<T>()));
            }
        });
    }

    mc::string get_result() const
    {
        return mc::string(m_stream.str());
    }

private:
    std::ostringstream  m_stream;
    json_encode_options m_options;
    int                 m_current_depth;
};

// 用于JSON解码的辅助类
class decoder {
public:
    explicit decoder(mc::string_view           json,
                     const json_decode_options& options = json_decode_options::default_decode_options)
        : m_input(json), m_pos(0), m_options(options), m_current_depth(0)
    {
        m_options.normalize();

        // 检查输入JSON字符串的总长度
        if (m_input.length() > m_options.max_input_length) {
            MC_THROW(mc::parse_error_exception, "Input JSON string length exceeds limit");
        }
    }

    // 解码入口函数
    variant decode()
    {
        skip_whitespace();
        variant result = parse_value();
        skip_whitespace();
        if (m_pos < m_input.length()) {
            MC_THROW(mc::parse_error_exception, "Extra characters after JSON string");
        }
        return result;
    }

    // 跳过空白字符
    void skip_whitespace()
    {
        while (m_pos < m_input.length() && std::isspace(m_input[m_pos])) {
            ++m_pos;
        }
    }

    // 检查是否到达输入末尾
    bool is_eof() const
    {
        return m_pos >= m_input.length();
    }

    // 获取当前字符
    char current() const
    {
        return is_eof() ? '\0' : m_input[m_pos];
    }

    // 移动到下一个字符
    void advance()
    {
        if (!is_eof()) {
            ++m_pos;
        }
    }

    // 检查嵌套深度
    void check_depth()
    {
        if (m_current_depth >= m_options.max_depth) {
            MC_THROW(mc::parse_error_exception, "JSON nesting depth exceeds limit");
        }
    }

    // 进入新的嵌套层级
    void enter_scope()
    {
        ++m_current_depth;
        check_depth();
    }

    // 离开嵌套层级
    void leave_scope()
    {
        --m_current_depth;
    }

    // 解析值
    variant parse_value()
    {
        skip_whitespace();
        char c = current();
        switch (c) {
            case 'n':
                return parse_null();
            case 't':
                return parse_true();
            case 'f':
                return parse_false();
            case '"':
                return parse_string();
            case '[':
                return parse_array();
            case '{':
                return parse_object();
            default:
                if (c == '-' || std::isdigit(c)) {
                    return parse_number();
                }
                MC_THROW(mc::parse_error_exception, "Invalid JSON value");
        }
    }

    // 解析null
    variant parse_null()
    {
        if (m_input.substr(m_pos, 4) == "null") {
            m_pos += 4;
            return variant();
        }
        MC_THROW(mc::parse_error_exception, "Invalid null value");
    }

    // 解析true
    variant parse_true()
    {
        if (m_input.substr(m_pos, 4) == "true") {
            m_pos += 4;
            return variant(true);
        }
        MC_THROW(mc::parse_error_exception, "Invalid true value");
    }

    // 解析false
    variant parse_false()
    {
        if (m_input.substr(m_pos, 5) == "false") {
            m_pos += 5;
            return variant(false);
        }
        MC_THROW(mc::parse_error_exception, "Invalid false value");
    }

    // 解析字符串
    void handle_unicode_escape(mc::string& result)
    {
        if (m_pos + 4 >= m_input.length()) {
            MC_THROW(mc::parse_error_exception, "Invalid Unicode escape sequence");
        }
        const mc::string_view hex = m_input.substr(m_pos + 1, 4);
        std::uint32_t       code_point = 0;
        if (!parse_uint32_hex4(hex, code_point)) {
            MC_THROW(mc::parse_error_exception, "Invalid Unicode escape sequence");
        }
        if (code_point > 0xFFFFu) {
            MC_THROW(mc::parse_error_exception, "Unsupported Unicode character");
        }
        if (code_point <= 0x7Fu) {
            result += static_cast<char>(code_point);
        } else if (code_point <= 0x7FFu) {
            result += static_cast<char>(0xC0u | (code_point >> 6u));
            result += static_cast<char>(0x80u | (code_point & 0x3Fu));
        } else {
            result += static_cast<char>(0xE0u | (code_point >> 12u));
            result += static_cast<char>(0x80u | ((code_point >> 6u) & 0x3Fu));
            result += static_cast<char>(0x80u | (code_point & 0x3Fu));
        }
        m_pos += 4;
    }

    void handle_normal_char(char c, mc::string& result)
    {
        if (static_cast<unsigned char>(c) < 0x20) {
            MC_THROW(mc::parse_error_exception, "String contains invalid character");
        }
        result += c;
    }

    void handle_escape_sequence(mc::string& result)
    {
        if (is_eof()) {
            MC_THROW(mc::parse_error_exception, "String not properly terminated");
        }
        char c = current();
        switch (c) {
            case '"':
                result += '"';
                break;
            case '\\':
                result += '\\';
                break;
            case '/':
                result += '/';
                break;
            case 'b':
                result += '\b';
                break;
            case 'f':
                result += '\f';
                break;
            case 'n':
                result += '\n';
                break;
            case 'r':
                result += '\r';
                break;
            case 't':
                result += '\t';
                break;
            case 'u':
                handle_unicode_escape(result);
                break;
            default:
                MC_THROW(mc::parse_error_exception, "Invalid escape sequence");
        }
    }

    variant parse_string()
    {
        advance(); // 跳过开始的双引号
        mc::string result;
        while (!is_eof()) {
            char c = current();
            if (c == '"') {
                advance(); // 跳过结束的双引号
                if (result.length() > m_options.max_string_length) {
                    MC_THROW(mc::parse_error_exception, "String length exceeds limit");
                }
                return variant(result);
            }
            if (c == '\\') {
                advance(); // 跳过转义字符
                handle_escape_sequence(result);
            } else {
                handle_normal_char(c, result);
            }
            advance();
        }
        MC_THROW(mc::parse_error_exception, "String not properly terminated");
    }

    // 解析数字
    variant parse_number()
    {
        size_t start       = m_pos;
        bool   is_float    = false;
        bool   is_negative = false;

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
            MC_THROW(mc::parse_error_exception, "Invalid number format");
        }

        // 处理小数部分
        if (current() == '.') {
            is_float = true;
            advance();
            if (!std::isdigit(current())) {
                MC_THROW(mc::parse_error_exception, "Decimal point must be followed by digits");
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
                MC_THROW(mc::parse_error_exception, "Exponent part must contain digits");
            }
            while (!is_eof() && std::isdigit(current())) {
                advance();
            }
        }

        const mc::string_view number_sv(m_input.data() + start, m_pos - start);
        variant               parsed;
        if (!parse_json_number_token(number_sv, is_float, is_negative, parsed)) {
            MC_THROW(mc::parse_error_exception, "Number conversion failed");
        }
        return parsed;
    }

    // 解析数组
    variant parse_array()
    {
        enter_scope(); // 进入数组作用域
        advance();     // 跳过开始的方括号
        variants result;
        skip_whitespace();

        if (current() == ']') {
            advance();
            leave_scope(); // 离开数组作用域
            return variant(result);
        }

        while (true) {
            if (result.size() >= m_options.max_array_size) {
                MC_THROW(mc::parse_error_exception, "Array element count exceeds limit");
            }

            result.push_back(parse_value());
            skip_whitespace();

            if (current() == ']') {
                advance();
                leave_scope(); // 离开数组作用域
                return variant(result);
            }

            if (current() != ',') {
                MC_THROW(mc::parse_error_exception, "Array elements must be separated by commas");
            }
            advance();
            skip_whitespace();
        }
    }

    // 解析对象
    variant parse_object()
    {
        enter_scope(); // 进入对象作用域
        advance();     // 跳过开始的大括号
        mc::dict result;
        skip_whitespace();

        if (current() == '}') {
            advance();
            leave_scope(); // 离开对象作用域
            return variant(result);
        }

        size_t count = 0;
        while (true) {
            if (count >= m_options.max_object_size) {
                MC_THROW(mc::parse_error_exception, "Object key-value pair count exceeds limit");
            }
            count++;

            if (current() != '"') {
                MC_THROW(mc::parse_error_exception, "Object key must be a string");
            }

            mc::string key = parse_string().as<mc::string>();
            skip_whitespace();

            if (current() != ':') {
                MC_THROW(mc::parse_error_exception, "Object key-value pairs must be separated by colons");
            }
            advance();

            variant value = parse_value();
            result(key, value);

            skip_whitespace();
            if (current() == '}') {
                advance();
                leave_scope(); // 离开对象作用域
                return variant(result);
            }

            if (current() != ',') {
                MC_THROW(mc::parse_error_exception, "Object key-value pairs must be separated by commas");
            }
            advance();
            skip_whitespace();
        }
    }

private:
    mc::string_view    m_input;
    size_t              m_pos;
    json_decode_options m_options;
    int                 m_current_depth;
};

// 实现json_encode函数
mc::string json_encode(const variant& value, const json_encode_options& options)
{
    try {
        encoder enc(options);
        return enc.encode(value);
    } catch (const mc::parse_error_exception&) {
        // 如果已经是parse_error_exception，直接重新抛出
        throw;
    } catch (const std::exception& e) {
        // 将其他异常转换为parse_error_exception
        MC_THROW(mc::parse_error_exception, "JSON encoding failed: ${error}", ("error", e.what()));
    }
}

mc::string json_encode(const dict& obj, const json_encode_options& options)
{
    try {
        encoder enc(options);
        enc.encode_object(obj);
        return enc.get_result();
    } catch (const mc::parse_error_exception&) {
        throw;
    } catch (const std::exception& e) {
        MC_THROW(mc::parse_error_exception, "JSON encoding failed: ${error}", ("error", e.what()));
    }
}

mc::string json_encode(const std::vector<variant>& arr, const json_encode_options& options)
{
    try {
        encoder enc(options);
        enc.encode_array(arr);
        return enc.get_result();
    } catch (const mc::parse_error_exception&) {
        throw;
    } catch (const std::exception& e) {
        MC_THROW(mc::parse_error_exception, "JSON encoding failed: ${error}", ("error", e.what()));
    }
}

// 实现json_decode函数
mc::variant json_decode(mc::string_view json, const json_decode_options& options)
{
    try {
        decoder dec(json, options);
        return dec.decode();
    } catch (const mc::parse_error_exception&) {
        throw;
    } catch (const std::exception& e) {
        MC_THROW(mc::parse_error_exception, "JSON decoding failed: ${error}", ("error", e.what()));
    }
}

// 初始化全局默认配置
json_encode_options json_encode_options::default_encode_options;
json_decode_options json_decode_options::default_decode_options;

} // namespace json
} // namespace mc