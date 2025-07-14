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

#ifndef MC_FMT_FORMAT_H
#define MC_FMT_FORMAT_H

#include <mc/exception.h>
#include <mc/fmt/format_compile.h>
#include <mc/fmt/format_context.h>
#include <mc/fmt/format_parser.h>
#include <mc/fmt/format_spec.h>
#include <mc/fmt/formatter.h>

#include <boost/preprocessor.hpp>

#include <type_traits>

#ifndef MC_FMT_COMPILE_TIME_CHECK_ENABLED
#define MC_FMT_COMPILE_TIME_CHECK_ENABLED 0
#endif

namespace mc::fmt {
using string_view = std::string_view;
using monostate   = std::monostate;
using format_spec = detail::format_spec;

class direct_outputbuf : public std::streambuf {
public:
    explicit direct_outputbuf(std::string& target) : m_target(target) {
        setp(nullptr, nullptr);
    }

protected:
    int_type overflow(int_type ch) override {
        if (ch != traits_type::eof()) {
            m_target.push_back(static_cast<char>(ch));
            return ch;
        }
        return traits_type::eof();
    }

    std::streamsize xsputn(const char* s, std::streamsize n) override {
        m_target.append(s, n);
        return n;
    }

private:
    std::string& m_target;
};

namespace detail {

template <typename T>
std::string& format_to(std::string& out, const T& value) {
    format_context ctx(out);
    return format_to(ctx, format_spec{}, value);
}

template <typename Context, typename T>
std::string& format_to(Context& ctx, const format_spec& spec, const T& value) {
    mc::fmt::formatter<T>{}.format(value, ctx, spec);
    return ctx.out();
}
} // namespace detail

template <typename... Args>
std::string& format_to(std::string& out, std::string_view fmt, Args&&... args) {
    detail::arg_store store(std::forward<Args>(args)...);
    format_context    ctx(out, store);
    detail::format_parser::parse(fmt, ctx);
    return out;
}

} // namespace mc::fmt

#define MC_FORMAT_AUXILIARY_0(...) ((__VA_ARGS__)) MC_FORMAT_AUXILIARY_1
#define MC_FORMAT_AUXILIARY_1(...) ((__VA_ARGS__)) MC_FORMAT_AUXILIARY_0
#define MC_FORMAT_AUXILIARY_0_END
#define MC_FORMAT_AUXILIARY_1_END

// 去除外层括号的宏
#define MC_FORMAT_REMOVE_PARENTHESES(...) __VA_ARGS__

// 将 (a)(b)("name", c) 形式转换为 ((a))((b))(("name", c)) 形式，确保 BOOST_PP_* 宏能正确处理中间带逗号的参数
#define MC_FORMAT_CONVERT_TO_SEQ(seq) \
    BOOST_PP_SEQ_POP_FRONT(BOOST_PP_CAT(MC_FORMAT_AUXILIARY_0(0) seq, _END))

#define MC_FORMAT_CHECK_ARG_POS(expr) \
    mc::fmt::detail::compile_arg(static_cast<std::decay_t<decltype(expr)>*>(nullptr))

#define MC_FORMAT_CHECK_ARG_NAMED(name, ...)                                                                    \
    BOOST_PP_IF(BOOST_PP_GREATER(BOOST_PP_VARIADIC_SIZE(dummy, ##__VA_ARGS__), 1),                              \
                mc::fmt::detail::compile_arg(name, static_cast<std::decay_t<decltype(__VA_ARGS__)>*>(nullptr)), \
                MC_FORMAT_CHECK_ARG_POS(name))

#define MC_FORMAT_CHECK_ARG_ELENEMT(r, macro, param) \
    macro param

#define MC_FORMAT_CHECK_ARG_SEQ_DIRECT(macro, seq) \
    BOOST_PP_SEQ_ENUM(BOOST_PP_SEQ_TRANSFORM(MC_FORMAT_CHECK_ARG_ELENEMT, macro, seq))

#define MC_FORMAT_CONVERT_TO_SEQ_IMPL(param) MC_FORMAT_CONVERT_TO_SEQ(param)
#define MC_FORMAT_WRAP_PARAM(param)          ((param))

#define MC_FORMAT_PARAM_TO_SEQ(param)            \
    BOOST_PP_IF(BOOST_PP_IS_BEGIN_PARENS(param), \
                MC_FORMAT_CONVERT_TO_SEQ_IMPL,   \
                MC_FORMAT_WRAP_PARAM)(param)

#define MC_FORMAT_CHECK_ARG(r, macro, param) \
    MC_FORMAT_CHECK_ARG_SEQ_DIRECT(macro, MC_FORMAT_PARAM_TO_SEQ(param)),

#define MC_FORMAT_APPLY_ARG(r, data, param)      \
    BOOST_PP_IF(BOOST_PP_IS_BEGIN_PARENS(param), \
                mc::fmt::detail::arg param,      \
                mc::fmt::detail::arg(param))

#define MC_FORMAT_APPLY_ARG_NAMED(name, ...)                                       \
    BOOST_PP_IF(BOOST_PP_GREATER(BOOST_PP_VARIADIC_SIZE(dummy, ##__VA_ARGS__), 1), \
                mc::fmt::detail::arg(name, __VA_ARGS__),                           \
                mc::fmt::detail::arg(name))

// 编译期检查宏实现
#define MC_FORMAT_COMPILE_CHECK(fmt_str, ...)                                     \
    BOOST_PP_IF(                                                                  \
        BOOST_PP_GREATER(BOOST_PP_VARIADIC_SIZE(dummy, ##__VA_ARGS__), 1),        \
        mc::fmt::detail::compile_check(                                           \
            fmt_str,                                                              \
            BOOST_PP_SEQ_FOR_EACH(MC_FORMAT_CHECK_ARG, MC_FORMAT_CHECK_ARG_NAMED, \
                                  BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__))          \
                mc::fmt::detail::compile_arg()),                                  \
        mc::fmt::detail::compile_check(fmt_str))

// 无参数版本的实现
#define MC_FORMAT_IMPL_NO_ARGS(fmt_str)                                            \
    [&] {                                                                          \
        static_assert(MC_FORMAT_COMPILE_CHECK(fmt_str), "格式化字符串或参数错误"); \
        return fmt_str;                                                            \
    }()

// 有参数版本的实现 - 使用新的序列处理方法
#define MC_FORMAT_IMPL_WITH_ARGS(fmt_str, ...)                                                  \
    [&] {                                                                                       \
        static_assert(MC_FORMAT_COMPILE_CHECK(fmt_str, __VA_ARGS__), "格式化字符串或参数错误"); \
        std::string result;                                                                     \
        mc::fmt::format_to(                                                                     \
            result, fmt_str,                                                                    \
            BOOST_PP_SEQ_FOR_EACH(MC_FORMAT_CHECK_ARG, MC_FORMAT_APPLY_ARG_NAMED,               \
                                  BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__)) std::monostate{});     \
        return result;                                                                          \
    }()

// 一个参数版本 - 只有 fmt_str
#define MC_FORMAT_1(fmt_str) MC_FORMAT_IMPL_NO_ARGS(fmt_str)

// 多个参数版本 - 有 fmt_str 和参数列表
#define MC_FORMAT_N(fmt_str, ...) MC_FORMAT_IMPL_WITH_ARGS(fmt_str, __VA_ARGS__)

// 参数分发宏 - 修复版本
#define MC_FORMAT_DISPATCH(fmt_str, ...)                                         \
    BOOST_PP_IF(BOOST_PP_EQUAL(BOOST_PP_VARIADIC_SIZE(dummy, ##__VA_ARGS__), 1), \
                MC_FORMAT_1(fmt_str),                                            \
                MC_FORMAT_N(fmt_str, __VA_ARGS__))

// sformat 宏定义 - 根据参数个数分发
#define sformat(...) MC_FORMAT_DISPATCH(__VA_ARGS__)

#include <mc/fmt/format_compile.h>
#include <mc/fmt/formatter_chrono.h>
#include <mc/fmt/formatter_chrono_inl.h>
#include <mc/fmt/formatter_mc.h>
#include <mc/fmt/formatter_std.h>
#endif // MC_FMT_FORMAT_H