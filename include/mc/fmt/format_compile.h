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

#ifndef MC_FMT_FORMAT_COMPILE_H
#define MC_FMT_FORMAT_COMPILE_H

#include <array>
#include <string_view>
#include <type_traits>

#include <mc/fmt/format_arg.h>
#include <mc/fmt/format_parser.h>

namespace mc::fmt::detail {

// 报告编译期错误信息，暂时什么都不做，只在最后统一报"格式化字符串或参数错误"
// 目前的实现没有办法在编译期精确报告 message 错误，这是因为 parse_format_string 只实现了最终
// 结果是编译期常量，如果想精确报错需要在每个条件分支都用 constexpr 检查，工作量较大暂不考虑
#define MC_COMPILE_TIME_ERROR(message)

template <typename T>
struct compile_arg_type {
    using type = T;
};

template <>
struct compile_arg_type<std::string> {
    using type = std::string_view;
};

enum class compile_arg_enum {
    null_type,
    bool_type,
    char_type,
    signed_type,
    unsigned_type,
    double_type,
    float_type,
    long_double_type,
    string_type,
    pointer_type,
    custom_type
};

struct compile_custom_t {
    constexpr compile_custom_t()
        : parse_fn(nullptr) {
    }

    bool (*parse_fn)(std::string_view, format_spec&);
};

struct compile_format_arg {
    compile_arg_enum type;
    compile_custom_t custom_value;
    bool             used = false;

    constexpr compile_format_arg()
        : type(compile_arg_enum::null_type),
          custom_value() {
    }

    constexpr explicit compile_format_arg(bool v)
        : type(compile_arg_enum::bool_type),
          custom_value() {
    }

    constexpr explicit compile_format_arg(char v)
        : type(compile_arg_enum::char_type),
          custom_value() {
    }

    template <typename T, std::enable_if_t<std::is_integral_v<T> &&
                                               std::is_signed_v<T>,
                                           int> = 0>
    constexpr explicit compile_format_arg(T v)
        : type(compile_arg_enum::signed_type),
          custom_value() {
    }

    template <typename T, std::enable_if_t<std::is_integral_v<T> &&
                                               std::is_unsigned_v<T>,
                                           int> = 0>
    constexpr explicit compile_format_arg(T v)
        : type(compile_arg_enum::unsigned_type),
          custom_value() {
    }

    constexpr explicit compile_format_arg(double v)
        : type(compile_arg_enum::double_type),
          custom_value() {
    }

    constexpr explicit compile_format_arg(float v)
        : type(compile_arg_enum::float_type),
          custom_value() {
    }

    constexpr explicit compile_format_arg(long double v)
        : type(compile_arg_enum::long_double_type),
          custom_value() {
    }

    constexpr explicit compile_format_arg(std::string_view v)
        : type(compile_arg_enum::string_type),
          custom_value() {
    }

    constexpr explicit compile_format_arg(const char* v)
        : type(compile_arg_enum::string_type),
          custom_value() {
    }

    constexpr explicit compile_format_arg(const void* v)
        : type(compile_arg_enum::pointer_type),
          custom_value() {
    }

    template <typename T, std::enable_if_t<
                              has_formatter_v<T> &&
                                  !is_basic_formatter_v<T>,
                              int> = 0>
    explicit compile_format_arg(const T& v)
        : type(compile_arg_enum::custom_type),
          custom_value() {
    }

    template <typename T, std::enable_if_t<
                              has_formatter_v<T> &&
                                  !is_basic_formatter_v<T>,
                              int> = 0>
    constexpr explicit compile_format_arg(const T* v)
        : type(compile_arg_enum::custom_type),
          custom_value(compile_make_custom_value(v)) {
    }

    constexpr compile_format_arg(const compile_format_arg& other)
        : type(other.type),
          custom_value(other.custom_value),
          used(other.used) {
    }

    constexpr compile_format_arg(compile_format_arg&& other)
        : type(other.type),
          custom_value(std::move(other.custom_value)),
          used(other.used) {
    }

    constexpr compile_format_arg& operator=(const compile_format_arg& other) {
        type         = other.type;
        custom_value = other.custom_value;
        used         = other.used;
        return *this;
    }

    constexpr compile_format_arg& operator=(compile_format_arg&& other) {
        type         = other.type;
        custom_value = std::move(other.custom_value);
        used         = other.used;
        other.used   = false;
        return *this;
    }

    constexpr void make_unused() {
        used = true;
    }

    constexpr bool is_used() const {
        return used;
    }

    template <typename Visitor>
    constexpr void visit(Visitor&& vis) const {
        switch (type) {
        case compile_arg_enum::null_type:
            vis(nullptr);
            break;
        case compile_arg_enum::bool_type:
            vis(false);
            break;
        case compile_arg_enum::char_type:
            vis(char{});
            break;
        case compile_arg_enum::signed_type:
            vis(int64_t{});
            break;
        case compile_arg_enum::unsigned_type:
            vis(uint64_t{});
            break;
        case compile_arg_enum::double_type:
            vis(double{});
            break;
        case compile_arg_enum::float_type:
            vis(float{});
            break;
        case compile_arg_enum::long_double_type:
            vis(static_cast<long double>(0));
            break;
        case compile_arg_enum::string_type:
            vis(std::string_view{});
            break;
        case compile_arg_enum::pointer_type:
            vis(static_cast<const void*>(nullptr));
            break;
        case compile_arg_enum::custom_type:
            vis(custom_value);
            break;
        }
    }

    constexpr bool is_custom() const {
        return type == compile_arg_enum::custom_type;
    }

    constexpr const compile_custom_t& as_custom() const {
        return custom_value;
    }

    explicit operator bool() const {
        return type != compile_arg_enum::null_type;
    }

    constexpr bool parse_custom(const char*& ptr, const char* end, format_spec& spec) const {
        std::string_view format_str(ptr, end - ptr - 1);
        return as_custom().parse_fn(format_str, spec);
    }

    constexpr bool parser(const char*& ptr, const char* end, format_spec& spec) const {
        if (!is_custom() || !as_custom().parse_fn) {
            return false;
        }

        if (!parse_custom(ptr, end, spec)) {
            ptr = nullptr;
        }

        return true;
    }

private:
    template <typename T>
    constexpr compile_custom_t compile_make_custom_value(const T*) const {
        compile_custom_t custom_val;
        if constexpr (has_parse_v<T>) {
            custom_val.parse_fn = [](std::string_view fmt_str, format_spec& spec) -> bool {
                return formatter<T>{}.template parse<true>(fmt_str, spec);
            };
        } else {
            custom_val.parse_fn = nullptr;
        }
        return custom_val;
    }
};

using compile_arg_store = detail::arg_store<compile_format_arg>;

class compile_format_context {
public:
    using arg_type = compile_format_arg;

    constexpr compile_format_context(compile_arg_store& args)
        : m_args(args) {
    }

    constexpr bool get_arg(size_t index, compile_format_arg& arg) const {
        if (m_args.get_positional(index, arg)) {
            return true;
        }

        return false;
    }

    constexpr bool get_named_arg(std::string_view name, compile_format_arg& arg, size_t& index) const {
        if (m_args.get_named(name, arg, index)) {
            return true;
        }

        return false;
    }

    constexpr void format_arg(const compile_format_arg& arg, detail::format_spec& spec) {
    }

    constexpr void append(char) {
    }

    constexpr void set_invalid() {
        m_is_valid = false;
    }

    constexpr bool is_valid() const {
        return m_is_valid;
    }

    constexpr std::size_t used_size() const {
        std::size_t count = 0;
        for (std::size_t i = 0; i < m_args.size(); ++i) {
            if (m_args.is_used(i)) {
                ++count;
            }
        }
        return count; // 返回已使用的参数数量
    }

    constexpr void set_used(size_t index) {
        m_args.set_used(index);
    }

    constexpr size_t arg_count() const {
        return m_args.size();
    }

    constexpr bool resolve_dynamic_param(size_t index, std::string_view name, int& out) {
        if (!m_args.resolve_dynamic_param(index, name, out)) {
            return false;
        }

        set_used(index);
        return true;
    }

    constexpr bool process_result(const detail::parser_result& result) {
        return !result.has_error();
    }

    constexpr void raise_error(const detail::parser_result& result) {
        if (!result.has_error()) {
            return;
        }

        set_invalid();
        switch (result.err) {
        case detail::parser_error::invalid_brace_arg:
            MC_COMPILE_TIME_ERROR("未找到对应的 '}'");
            break;
        case detail::parser_error::invalid_named_arg_name:
            MC_COMPILE_TIME_ERROR("命名参数名称不能为空");
            break;
        case detail::parser_error::invalid_index_arg:
            MC_COMPILE_TIME_ERROR("位置参数索引必须是数字");
            break;
        case detail::parser_error::invalid_single_brace_arg:
            MC_COMPILE_TIME_ERROR("单独的 '}' 在格式化字符串中");
            break;
        case detail::parser_error::name_arg_not_found:
            MC_COMPILE_TIME_ERROR("找不到命名参数");
            break;
        case detail::parser_error::index_arg_not_found:
            MC_COMPILE_TIME_ERROR("找不到位置参数");
            break;
        case detail::parser_error::invalid_spec_arg:
            MC_COMPILE_TIME_ERROR("格式化字符串或参数错误");
            break;
        case detail::parser_error::invalid_dynamic_param:
            MC_COMPILE_TIME_ERROR("动态参数类型错误");
            break;
        default:
            break;
        }
    }

private:
    compile_arg_store& m_args;
    bool               m_is_valid{true};
};

// 编译期参数表示结构 - 只包含类型信息，不包含值
template <typename T>
struct compile_named_arg {
    std::string_view name;
    using value_type = T;

    constexpr compile_named_arg(std::string_view n) : name(n) {
    }
};

template <typename T>
struct compile_positional_arg {
    using value_type = T;

    constexpr compile_positional_arg() = default;
};

struct compile_null_arg {
    constexpr compile_null_arg() = default;
};

// 编译期检查专用的 arg 函数 - 使用类型指针避免构造实际对象
template <typename T>
constexpr auto compile_arg(std::string_view name, T*) {
    return compile_named_arg<T>{name};
}

template <typename T>
constexpr auto compile_arg(T*) {
    return compile_positional_arg<T>{};
}

constexpr auto compile_arg() {
    return compile_null_arg{};
}

// 编译期参数构建器
class compile_args_builder {
public:
    constexpr compile_args_builder() = default;

    // 处理命名参数
    template <typename T>
    constexpr compile_args_builder& operator()(const compile_named_arg<T>& arg) {
        if constexpr (has_formatter_v<T> && !is_basic_formatter_v<T>) {
            using value_type = typename compile_arg_type<T>::type;
            m_args.add_arg(arg.name, compile_format_arg(static_cast<value_type*>(nullptr)));
        } else {
            m_args.add_arg(named_arg{arg.name, typename compile_arg_type<T>::type{}});
        }
        return *this;
    }

    // 处理位置参数
    template <typename T>
    constexpr compile_args_builder& operator()(const compile_positional_arg<T>&) {
        if constexpr (has_formatter_v<T> && !is_basic_formatter_v<T>) {
            using value_type = typename compile_arg_type<T>::type;
            m_args.add_arg(std::string_view(), compile_format_arg(static_cast<value_type*>(nullptr)));
        } else {
            m_args.add_arg(typename compile_arg_type<T>::type());
        }
        return *this;
    }

    constexpr compile_args_builder& operator()(const compile_null_arg&) {
        return *this;
    }

    constexpr compile_arg_store& arg_store() {
        return m_args;
    }

    constexpr size_t size() const {
        return m_args.size();
    }

private:
    compile_arg_store m_args;
};

template <typename T>
struct extract_value_type {
    using type = T;
};

template <typename T>
struct extract_value_type<compile_named_arg<T>> {
    using type = T;
};

template <typename T>
struct extract_value_type<compile_positional_arg<T>> {
    using type = T;
};

template <>
struct extract_value_type<compile_null_arg> {
    using type = void;
};

template <typename T>
using extract_value_type_t = typename extract_value_type<T>::type;

template <typename T>
struct compile_type_checker {
    using actual_type                    = extract_value_type_t<std::decay_t<T>>;
    static constexpr bool is_formattable = std::is_void_v<actual_type> || has_formatter_v<actual_type>;
};

template <typename... Args>
constexpr bool compile_check(std::string_view fmt_str, Args&&... args) {
    // 首先检查所有参数类型是否可格式化
    constexpr bool all_formattable = (compile_type_checker<std::decay_t<Args>>::is_formattable && ...);
    if (!all_formattable) {
        return false;
    }

    compile_args_builder builder;
    (..., builder(std::forward<Args>(args)));

    // 检查格式字符串解析和验证是否成功
    compile_format_context ctx(builder.arg_store());
    parse_format_string(fmt_str, ctx);
    if (!ctx.is_valid()) {
        return false;
    }

    // 检查是否还有未使用的参数，如果有则说明格式字符串的占位符缺失
    if (ctx.arg_count() != ctx.used_size()) {
        return false;
    }

    return true;
}

} // namespace mc::fmt::detail

#endif // MC_FMT_FORMAT_COMPILE_H