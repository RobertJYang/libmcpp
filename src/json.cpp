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
#include <iomanip>
#include <mc/dict.h>
#include <mc/exception.h>
#include <mc/json.h>
#include <mc/variant.h>
#include <sstream>

namespace mc {
namespace json {

// 用于JSON编码的辅助类
class encoder {
public:
    explicit encoder(
        const json_encode_options& options = json_encode_options::default_encode_options)
        : m_stream(), m_options(options), m_current_depth(0) {
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

    // 检查嵌套深度
    void check_depth() {
        if (m_current_depth >= m_options.max_depth) {
            MC_THROW(mc::parse_error_exception, "JSON嵌套深度超过限制");
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
            m_stream << '\n'
                     << std::string(m_current_depth * m_options.indent_size, ' ');
        }
    }

    // 添加分隔符
    void add_separator() {
        if (m_options.pretty_print) {
            m_stream << ' ';
        }
    }

    // 编码字符串
    void encode_string(std::string_view str) {
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
                    m_stream << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                             << static_cast<int>(c);
                } else if (m_options.escape_non_ascii && c >= 0x80) {
                    // 转义非ASCII字符
                    m_stream << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                             << static_cast<int>(c);
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
            MC_THROW(mc::parse_error_exception, "无效的数值");
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
            keys.push_back(std::cref(entry.key.get_string()));
        }
        if (m_options.sort_keys) {
            std::sort(keys.begin(), keys.end(), [](const auto& a, const auto& b) {
                return a.get() < b.get();
            });
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
            } else if constexpr (std::is_same_v<T, typename variant::string_type>) {
                encode_string(v);
            } else if constexpr (std::is_same_v<T, typename variant::blob_type>) {
                encode_string(v.as_string_view());
            } else if constexpr (std::is_same_v<T, typename variant::array_type>) {
                encode_array(v);
            } else if constexpr (std::is_same_v<T, typename variant::object_type>) {
                encode_object(v);
            } else if constexpr (std::is_same_v<T, typename variant::extension_type>) {
                encode_string(v.as_string());
            } else {
                MC_THROW(mc::parse_error_exception, "不支持的 JSON 值类型 ${type}",
                         ("type", mc::pretty_name<T>()));
            }
        });
    }

    std::string get_result() const {
        return m_stream.str();
    }

private:
    std::ostringstream  m_stream;
    json_encode_options m_options;
    int                 m_current_depth;
};

// 用于JSON解码的辅助类
class decoder {
public:
    explicit decoder(std::string_view json, const json_decode_options& options =
                                                json_decode_options::default_decode_options)
        : m_input(json), m_pos(0), m_options(options), m_current_depth(0) {
        m_options.normalize();

        // 检查输入JSON字符串的总长度
        if (m_input.length() > m_options.max_input_length) {
            MC_THROW(mc::parse_error_exception, "输入JSON字符串长度超过限制");
        }
    }

    // 解码入口函数
    variant decode() {
        skip_whitespace();
        variant result = parse_value();
        skip_whitespace();
        if (m_pos < m_input.length()) {
            MC_THROW(mc::parse_error_exception, "JSON字符串后有多余字符");
        }
        return result;
    }

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
            MC_THROW(mc::parse_error_exception, "JSON嵌套深度超过限制");
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
            MC_THROW(mc::parse_error_exception, "无效的JSON值");
        }
    }

    // 解析null
    variant parse_null() {
        if (m_input.substr(m_pos, 4) == "null") {
            m_pos += 4;
            return variant();
        }
        MC_THROW(mc::parse_error_exception, "无效的null值");
    }

    // 解析true
    variant parse_true() {
        if (m_input.substr(m_pos, 4) == "true") {
            m_pos += 4;
            return variant(true);
        }
        MC_THROW(mc::parse_error_exception, "无效的true值");
    }

    // 解析false
    variant parse_false() {
        if (m_input.substr(m_pos, 5) == "false") {
            m_pos += 5;
            return variant(false);
        }
        MC_THROW(mc::parse_error_exception, "无效的false值");
    }

    // 解析字符串
    void handle_unicode_escape(std::string& result) {
        if (m_pos + 4 >= m_input.length()) {
            MC_THROW(mc::parse_error_exception, "无效的Unicode转义序列");
        }
        std::string hex = std::string(m_input.substr(m_pos + 1, 4));

        for (char c : hex) {
            if (!std::isxdigit(c)) {
                MC_THROW(mc::parse_error_exception, "无效的Unicode转义序列");
            }
        }

        try {
            int code_point = std::stoi(hex, nullptr, 16);
            if (code_point <= 0x7F) {
                result += static_cast<char>(code_point);
            } else if (code_point <= 0x7FF) {
                result += static_cast<char>(0xC0 | (code_point >> 6));
                result += static_cast<char>(0x80 | (code_point & 0x3F));
            } else if (code_point <= 0xFFFF) {
                result += static_cast<char>(0xE0 | (code_point >> 12));
                result += static_cast<char>(0x80 | ((code_point >> 6) & 0x3F));
                result += static_cast<char>(0x80 | (code_point & 0x3F));
            } else {
                MC_THROW(mc::parse_error_exception, "不支持的Unicode字符");
            }
            m_pos += 4;
        } catch (const std::exception&) {
            MC_THROW(mc::parse_error_exception, "无效的Unicode转义序列");
        }
    }

    void handle_normal_char(char c, std::string& result) {
        if (static_cast<unsigned char>(c) < 0x20) {
            MC_THROW(mc::parse_error_exception, "字符串包含无效字符");
        }
        result += c;
    }

    void handle_escape_sequence(std::string& result) {
        if (is_eof()) {
            MC_THROW(mc::parse_error_exception, "字符串未正确终止");
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
            MC_THROW(mc::parse_error_exception, "无效的转义序列");
        }
    }

    variant parse_string() {
        advance(); // 跳过开始的双引号
        std::string result;
        while (!is_eof()) {
            char c = current();
            if (c == '"') {
                advance(); // 跳过结束的双引号
                if (result.length() > m_options.max_string_length) {
                    MC_THROW(mc::parse_error_exception, "字符串长度超过限制");
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
        MC_THROW(mc::parse_error_exception, "字符串未正确终止");
    }

    // 解析数字
    variant parse_number() {
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
            MC_THROW(mc::parse_error_exception, "无效的数字格式");
        }

        // 处理小数部分
        if (current() == '.') {
            is_float = true;
            advance();
            if (!std::isdigit(current())) {
                MC_THROW(mc::parse_error_exception, "小数点后必须跟随数字");
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
                MC_THROW(mc::parse_error_exception, "指数部分必须包含数字");
            }
            while (!is_eof() && std::isdigit(current())) {
                advance();
            }
        }

        // 获取数字字符串
        std::string number_str(m_input.substr(start, m_pos - start));

        try {
            if (is_float) {
                return variant(std::stod(number_str));
            } else if (is_negative) {
                return variant(std::stoll(number_str));
            } else {
                return variant(std::stoull(number_str));
            }
        } catch (const std::exception&) {
            MC_THROW(mc::parse_error_exception, "数字转换失败");
        }
    }

    // 解析数组
    variant parse_array() {
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
                MC_THROW(mc::parse_error_exception, "数组元素数量超过限制");
            }

            result.push_back(parse_value());
            skip_whitespace();

            if (current() == ']') {
                advance();
                leave_scope(); // 离开数组作用域
                return variant(result);
            }

            if (current() != ',') {
                MC_THROW(mc::parse_error_exception, "数组元素必须用逗号分隔");
            }
            advance();
            skip_whitespace();
        }
    }

    // 解析对象
    variant parse_object() {
        enter_scope(); // 进入对象作用域
        advance();     // 跳过开始的大括号
        mutable_dict result;
        skip_whitespace();

        if (current() == '}') {
            advance();
            leave_scope(); // 离开对象作用域
            return variant(result);
        }

        size_t count = 0;
        while (true) {
            if (count >= m_options.max_object_size) {
                MC_THROW(mc::parse_error_exception, "对象键值对数量超过限制");
            }
            count++;

            if (current() != '"') {
                MC_THROW(mc::parse_error_exception, "对象键必须是字符串");
            }

            std::string key = parse_string().as<std::string>();
            skip_whitespace();

            if (current() != ':') {
                MC_THROW(mc::parse_error_exception, "对象键值对必须用冒号分隔");
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
                MC_THROW(mc::parse_error_exception, "对象键值对必须用逗号分隔");
            }
            advance();
            skip_whitespace();
        }
    }

private:
    std::string_view    m_input;
    size_t              m_pos;
    json_decode_options m_options;
    int                 m_current_depth;
};

// 实现json_encode函数
std::string json_encode(const variant& value, const json_encode_options& options) {
    try {
        encoder enc(options);
        return enc.encode(value);
    } catch (const mc::parse_error_exception&) {
        // 如果已经是parse_error_exception，直接重新抛出
        throw;
    } catch (const std::exception& e) {
        // 将其他异常转换为parse_error_exception
        MC_THROW(mc::parse_error_exception, "JSON编码失败: ${error}", ("error", e.what()));
    }
}

std::string json_encode(const dict& obj, const json_encode_options& options) {
    try {
        encoder enc(options);
        enc.encode_object(obj);
        return enc.get_result();
    } catch (const mc::parse_error_exception&) {
        throw;
    } catch (const std::exception& e) {
        MC_THROW(mc::parse_error_exception, "JSON编码失败: ${error}", ("error", e.what()));
    }
}

std::string json_encode(const std::vector<variant>& arr, const json_encode_options& options) {
    try {
        encoder enc(options);
        enc.encode_array(arr);
        return enc.get_result();
    } catch (const mc::parse_error_exception&) {
        throw;
    } catch (const std::exception& e) {
        MC_THROW(mc::parse_error_exception, "JSON编码失败: ${error}", ("error", e.what()));
    }
}

// 实现json_decode函数
mc::variant json_decode(std::string_view json, const json_decode_options& options) {
    try {
        decoder dec(json, options);
        return dec.decode();
    } catch (const mc::parse_error_exception&) {
        throw;
    } catch (const std::exception& e) {
        MC_THROW(mc::parse_error_exception, "JSON解码失败: ${error}", ("error", e.what()));
    }
}

// 初始化全局默认配置
json_encode_options json_encode_options::default_encode_options;
json_decode_options json_decode_options::default_decode_options;

} // namespace json
} // namespace mc