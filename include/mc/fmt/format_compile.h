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

class compile_format_context {
public:
    constexpr compile_format_context(detail::arg_store& args)
        : m_args(args) {
    }

    constexpr bool get_arg(size_t index, detail::format_arg& arg) const {
        if (m_args.get_positional(index, arg)) {
            return true;
        }

        MC_COMPILE_TIME_ERROR("找不到位置参数");
        return false;
    }

    constexpr bool get_named_arg(std::string_view name, detail::format_arg& arg, size_t& index) const {
        if (m_args.get_named(name, arg, index)) {
            return true;
        }

        MC_COMPILE_TIME_ERROR("找不到命名参数");
        return false;
    }

    constexpr void format_arg(const detail::format_arg& arg, detail::format_spec& spec) {
    }

    constexpr void append(char) {
    }

    constexpr void dynamic_width_param_type_error() {
        MC_COMPILE_TIME_ERROR("动态宽度参数类型错误，必须为整数");
        set_invalid();
    }

    constexpr void dynamic_precision_param_type_error() {
        MC_COMPILE_TIME_ERROR("动态精度参数类型错误，必须为整数");
        set_invalid();
    }

    constexpr void invalid_brace_arg() {
        MC_COMPILE_TIME_ERROR("未找到对应的 '}'");
        set_invalid();
    }

    constexpr void invalid_named_arg_name() {
        MC_COMPILE_TIME_ERROR("命名参数名称不能为空");
        set_invalid();
    }

    constexpr void invalid_index_arg() {
        MC_COMPILE_TIME_ERROR("位置参数索引必须是数字");
        set_invalid();
    }

    constexpr void invalid_single_brace_arg() {
        MC_COMPILE_TIME_ERROR("单独的 '}' 在格式化字符串中");
        set_invalid();
    }

    constexpr void invalid_named_arg(std::string_view) {
        MC_COMPILE_TIME_ERROR("找不到命名参数");
        set_invalid();
    }

    constexpr void invalid_index_arg(size_t index) {
        MC_COMPILE_TIME_ERROR("位置参数索引必须是数字");
        set_invalid();
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

private:
    detail::arg_store& m_args;
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

// 编译期参数构建器 - 专门处理编译期参数结构
class compile_args_builder {
public:
    constexpr compile_args_builder() = default;

    // 处理命名参数
    template <typename T>
    constexpr compile_args_builder& operator()(const compile_named_arg<T>& arg) {
        if constexpr (has_formatter_v<T> && !is_basic_formatter_v<T>) {
            using value_type = typename compile_arg_type<T>::type;
            m_args.add_arg(arg.name, format_arg(static_cast<value_type*>(nullptr)));
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
            m_args.add_arg(std::string_view(), format_arg(static_cast<value_type*>(nullptr)));
        } else {
            m_args.add_arg(typename compile_arg_type<T>::type());
        }
        return *this;
    }

    constexpr compile_args_builder& operator()(const compile_null_arg&) {
        return *this;
    }

    constexpr detail::arg_store& arg_store() {
        return m_args;
    }

    constexpr size_t size() const {
        return m_args.size();
    }

private:
    detail::arg_store m_args;
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