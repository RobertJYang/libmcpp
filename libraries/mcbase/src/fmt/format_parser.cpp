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

#include <mc/exception.h>
#include <mc/fmt/format.h>
#include <mc/fmt/format_parser.h>
#include <mc/fmt/format_spec.h>
#include <sstream>

namespace mc::fmt::detail {

// 应用对齐和填充
static void apply_alignment_and_padding(mc::string& out, size_t content_start, size_t content_len,
                                        const format_spec& spec)
{
    if (spec.width <= static_cast<int>(content_len)) {
        return;
    }
    int padding = spec.width - static_cast<int>(content_len);
    switch (spec.alignment) {
        case format_spec::align::left:
            out.append(padding, spec.fill);
            break;
        case format_spec::align::center: {
            int left  = padding / 2;
            int right = padding - left;
            out.insert(content_start, left, spec.fill);
            out.append(right, spec.fill);
            break;
        }
        case format_spec::align::right:
            out.insert(content_start, padding, spec.fill);
            break;
        case format_spec::align::none:
        default:
            // 默认右对齐
            if (spec.zero_pad) {
                // 对于补0的场景，右对齐需要填充到符号位后面
                size_t sign_pos = content_start;
                while (sign_pos < out.size() &&
                       (out[sign_pos] == ' ' || out[sign_pos] == '+' || out[sign_pos] == '-')) {
                    ++sign_pos; // 跳过符号
                }
                out.insert(sign_pos, padding, spec.fill);
            } else {
                out.insert(content_start, padding, spec.fill);
            }
            break;
    }
}

// 转换为无符号类型
template <typename T>
auto to_unsigned_type(T value)
{
    if constexpr (std::is_unsigned_v<T>) {
        return value;
    } else {
        using unsigned_type = std::make_unsigned_t<T>;
        if (value >= 0) {
            return static_cast<unsigned_type>(value);
        }

        if (value == std::numeric_limits<T>::min()) {
            // 对于最小值，直接使用最大值+1
            return static_cast<unsigned_type>(std::numeric_limits<T>::max()) + 1;
        }

        return static_cast<unsigned_type>(-value);
    }
}

// 写入二进制值
static void write_bit_value(std::ostream& os, const std::bitset<64>& bits, const format_spec& spec)
{
    if (spec.alternate_form) {
        os << (spec.type == 'B' ? "0B" : "0b");
    }

    mc::string s = bits.to_string();
    s.erase(0, s.find_first_not_of('0'));
    if (s.empty()) {
        s = "0";
    }
    if (spec.type == 'B') {
        std::transform(s.begin(), s.end(), s.begin(), ::toupper);
    }
    os << s;
}

// 格式化整数
template <typename T>
void format_integer(format_context& ctx, T value, const format_spec& spec)
{
    bool is_negative = value < 0;
    auto abs_val     = to_unsigned_type(value);
    format_number(ctx.out(), spec, is_negative, [&](std::ostream& os) {
        switch (spec.type) {
            case '\0':
            case 'd':
                os << std::dec << abs_val;
                break;
            case 'b':
            case 'B': {
                write_bit_value(os, std::bitset<64>(abs_val), spec);
                break;
            }
            case 'o':
                if (spec.alternate_form) {
                    os << '0';
                }
                os << std::oct << abs_val;
                break;
            case 'x':
            case 'X':
                if (spec.alternate_form) {
                    os << (spec.type == 'X' ? "0X" : "0x");
                }
                os << std::hex << (spec.type == 'X' ? std::uppercase : std::nouppercase) << abs_val;
                break;
            default:
                MC_THROW(mc::format_error, "invalid format specifier for integer");
        }
    });
}

// 移除十六进制前缀
static void remove_hex_prefix(mc::string& s, size_t pos)
{
    if (s.size() < (pos + 2) || s[pos] != '0' || (s[pos + 1] != 'x' && s[pos + 1] != 'X')) {
        return;
    }

    s.erase(pos, 2);
}

// 写入浮点数值
template <typename T>
void write_double_value(std::ostream& os, mc::string& out, char type_char, int precision, T abs_val)
{
    switch (type_char) {
        case 'f':
        case 'F':
            os << std::fixed << std::setprecision(precision) << abs_val;
            break;
        case 'e':
        case 'E': {
            os << std::scientific << (type_char == 'E' ? std::uppercase : std::nouppercase)
               << std::setprecision(precision) << abs_val;
            break;
        }
        case 'g':
        case 'G': {
            if (precision == 0) {
                os << std::fixed;
            } else if (abs_val >= std::pow(10.0, precision) || (abs_val < 1e-4 && abs_val != 0)) {
                // 大于 10^precision 或者小于 1e-4 的数，使用科学计数法
                os << std::scientific;
            }
            os << (type_char == 'G' ? std::uppercase : std::nouppercase);
            os << std::setprecision(precision) << abs_val;
            break;
        }
        case 'a':
        case 'A': {
            auto start_pos = out.size();
            os << std::hexfloat << (type_char == 'A' ? std::uppercase : std::nouppercase)
               << std::setprecision(precision) << abs_val;
            remove_hex_prefix(out, start_pos);
            break;
        }
        case '\0':
            // 默认格式：对于大数使用固定点格式，对于小数使用科学计数法
            if (abs_val < 1e-4 && abs_val != 0) {
                os << std::scientific << std::nouppercase << std::setprecision(precision) << abs_val;
            } else {
                os << std::fixed << std::nouppercase << std::setprecision(precision) << abs_val;
            }
            break;
        default:
            MC_THROW(mc::format_error, "invalid format specifier for float");
    }
}

// 处理 g/G 格式的替代形式
static void handle_g_format_alternate(mc::string& s, std::size_t pos, int precision, double abs_val,
                                      const format_spec& spec)
{
    if (!spec.alternate_form) {
        return;
    }

    auto dot = s.find('.', pos);

    // 零值需要补足有效数字
    if (std::fpclassify(abs_val) == FP_ZERO && precision > 0) {
        if (dot == mc::string::npos) {
            s += ".";
            dot = s.size() - 1;
        }
        const std::size_t frac_len = s.size() - dot - 1;
        const int frac              = static_cast<int>(frac_len);
        for (int i = 0; i < precision - 1 - frac; ++i) {
            s += "0";
        }
    }

    // .0g 格式需要补小数点
    if (precision == 0 && dot == mc::string::npos) {
        s += ".";
    }
}

// 处理其他格式的替代形式
static void handle_format_alternate(mc::string& s, std::size_t pos, const format_spec& spec)
{
    if (!spec.alternate_form) {
        return;
    }

    auto dot = s.find('.', pos);
    if (dot != mc::string::npos) {
        return; // 已有小数点
    }

    auto type = spec.type;
    if (type == 'e' || type == 'E' || type == 'a' || type == 'A') {
        auto ep = s.find(type == 'a' || type == 'A' ? 'p' : type, pos);
        if (ep != mc::string::npos) {
            s.insert(ep, 1, '.');
        }
    } else {
        s += ".";
    }
}

// 去除尾零
static void remove_trailing_zeros(mc::string& s, std::size_t pos, const format_spec& spec)
{
    auto dot = s.find('.', pos);
    if (dot == mc::string::npos) {
        return;
    }

    std::size_t i = s.size() - 1;
    if (spec.type == 'g' || spec.type == 'G' || spec.type == '\0') {
        if (spec.alternate_form) {
            return; // 替代形式不去除尾零
        }

        for (; i > dot && s.at(i) == '0'; --i) {
        }
        if (i > dot) {
            s.resize(i + 1);
            return;
        }
    }

    // 处理只有小数点的情况
    if (i == dot) {
        s.resize(spec.alternate_form ? i + 1 : i);
    }
}

// 调整浮点数尾部格式
static void adjust_double_trailing(mc::string& s, std::size_t pos, int precision, double abs_val,
                                   const format_spec& spec)
{
    // 处理 g/G 格式的替代形式
    if (spec.type == 'g' || spec.type == 'G') {
        handle_g_format_alternate(s, pos, precision, abs_val, spec);
        return;
    }

    handle_format_alternate(s, pos, spec);
    remove_trailing_zeros(s, pos, spec);
}

// 格式化浮点数
template <typename T>
void format_double(format_context& ctx, T value, const format_spec& spec)
{
    bool is_negative = std::signbit(value);
    T    abs_val     = is_negative ? -value : value;
    int  precision   = (spec.precision >= 0) ? spec.precision : MC_FLOAT_PRECISION;
    format_number(ctx.out(), spec, is_negative, [&](std::ostream& os) {
        auto& out       = ctx.out();
        auto  start_pos = out.size();
        write_double_value(os, out, spec.type, precision, abs_val);
        adjust_double_trailing(out, start_pos, precision, abs_val, spec);
    });
}

// 格式化数字（带符号处理）
template <typename WriteAbs>
void format_number(mc::string& out, const format_spec& spec, bool is_negative, WriteAbs&& write_abs)
{
    size_t           start_pos = out.size();
    direct_outputbuf buf(out);
    std::ostream     os(&buf);

    if (is_negative) {
        os << '-';
    } else if (spec.sign_mode == format_spec::sign::plus) {
        os << '+';
    } else if (spec.sign_mode == format_spec::sign::space) {
        os << ' ';
    }

    write_abs(os);

    size_t content_len = out.size() - start_pos;
    apply_alignment_and_padding(out, start_pos, content_len, spec);
}

// 格式化通用值
template <typename WriteValue>
void format_value(mc::string& out, const format_spec& spec, WriteValue&& f)
{
    size_t start_pos = out.size();
    f(out);
    size_t content_len = out.size() - start_pos;
    apply_alignment_and_padding(out, start_pos, content_len, spec);
}

void format_parser::parse(string_view fmt_str, format_context& ctx)
{
    parse_format_string(fmt_str, ctx);
}

// 格式化浮点数
void format_parser::format_double(format_context& ctx, float value, const format_spec& spec)
{
    mc::fmt::detail::format_double(ctx, value, spec);
}

void format_parser::format_double(format_context& ctx, double value, const format_spec& spec)
{
    mc::fmt::detail::format_double(ctx, value, spec);
}

void format_parser::format_double(format_context& ctx, long double value, const format_spec& spec)
{
    mc::fmt::detail::format_double(ctx, value, spec);
}

// 格式化指针
void format_parser::format_pointer(format_context& ctx, const void* ptr, const format_spec& spec)
{
    if (spec.type != '\0' && spec.type != 'p') {
        MC_THROW(mc::format_error, "invalid format specifier for pointer");
    }
    format_value(ctx.out(), spec, [&](mc::string& out) {
        if (ptr == nullptr) {
            out += "0x0"; // 有些平台空指针打印为 0，我们统一成 0x0
        } else {
            direct_outputbuf buf(out);
            std::ostream     os(&buf);
            os << ptr;
        }
    });
}

// 格式化字符串
void format_parser::format_string(format_context& ctx, string_view str, const format_spec& spec)
{
    if (spec.type != '\0' && spec.type != 's') {
        MC_THROW(mc::format_error, "invalid format specifier for string");
    }
    if (spec.precision >= 0 && str.size() > static_cast<size_t>(spec.precision)) {
        str = str.substr(0, spec.precision);
    }

    // 字符串默认左对齐
    format_spec string_spec = spec;
    if (string_spec.alignment == format_spec::align::none) {
        string_spec.alignment = format_spec::align::left;
    }

    format_value(ctx.out(), string_spec, [&](mc::string& out) {
        out.append(str.data(), str.size());
    });
}

// 格式化布尔值
void format_parser::format_bool(format_context& ctx, bool value, const format_spec& spec)
{
    if (spec.type == '\0') {
        // 默认格式输出 true/false
        format_string(ctx, value ? string_view("true") : string_view("false"), spec);
    } else {
        // 其它格式按整数处理
        format_integer(ctx, static_cast<uint64_t>(value), spec);
    }
}

// 格式化自定义类型
void format_parser::format_custom(format_context& ctx, const custom_t& custom, const format_spec& spec)
{
    custom.format_fn(custom.obj, ctx, spec);
}

// 格式化字符
void format_parser::format_char(format_context& ctx, char c, const format_spec& spec)
{
    if (spec.type != '\0' && spec.type != 'c' && spec.type != 'd') {
        MC_THROW(mc::format_error, "invalid format specifier for char");
    }

    if (spec.type == 'd') {
        // 输出字符的 ASCII 值
        format_integer(ctx, static_cast<uint64_t>(c), spec);
    } else {
        format_value(ctx.out(), spec, [&](mc::string& out) {
            out.push_back(c);
        });
    }
}

// 格式化有符号整数
void format_parser::format_int(format_context& ctx, int64_t value, const format_spec& spec)
{
    mc::fmt::detail::format_integer(ctx, value, spec);
}

// 格式化无符号整数
void format_parser::format_uint(format_context& ctx, uint64_t value, const format_spec& spec)
{
    mc::fmt::detail::format_integer(ctx, value, spec);
}

// 格式化参数
void format_parser::format_arg(format_context& ctx, const detail::format_arg& arg, format_spec& spec)
{
    arg.visit([&](auto&& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, monostate>) {
            // 无值类型，不做任何处理
        } else if constexpr (std::is_same_v<T, bool>) {
            format_parser::format_bool(ctx, value, spec);
        } else if constexpr (mc::fmt::detail::is_integer<T>::value) {
            mc::fmt::detail::format_integer(ctx, value, spec);
        } else if constexpr (std::is_floating_point_v<T>) {
            mc::fmt::detail::format_double(ctx, value, spec);
        } else if constexpr (mc::fmt::detail::is_char<T>::value) {
            format_parser::format_char(ctx, static_cast<char>(value), spec);
        } else if constexpr (std::is_same_v<T, string_view> || std::is_same_v<T, const char*>) {
            format_parser::format_string(ctx, value, spec);
        } else if constexpr (std::is_same_v<T, const void*>) {
            format_parser::format_pointer(ctx, value, spec);
        } else if constexpr (std::is_same_v<T, custom_t>) {
            format_parser::format_custom(ctx, value, spec);
        } else {
            // 不支持的参数类型，运行时抛出异常
            MC_THROW(mc::format_error, "不支持的参数类型");
        }
    });
}

} // namespace mc::fmt::detail
