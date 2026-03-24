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

#ifndef MC_LOG_MESSAGE_H
#define MC_LOG_MESSAGE_H

#include <chrono>
#include <mc/dict.h>
#include <mc/fmt/format.h>
#include <mc/fmt/format_dict.h>
#include <mc/log/log_level.h>
#include <mc/string.h>
#include <mc/variant.h>

#include <memory>
#include <string>
#include <thread>
#include <tuple>
#include <type_traits>
#include <vector>

namespace mc::log {

/**
 * @brief 日志上下文信息
 */
struct context {
    mc::string m_file;     // 文件名
    mc::string m_function; // 函数名
    uint32_t   m_line;     // 行号

    context(mc::string_view file = "", mc::string_view function = "", uint32_t line = 0)
        : m_file(file), m_function(function), m_line(line)
    {}
};

/**
 * @brief 日志消息结构
 */
class MC_API message {
public:
    /**
     * @brief 构造函数
     *
     * @param lvl 日志级别
     * @param msg 日志消息
     * @param ctx 上下文信息
     * @param args 参数字典
     * @param attrs 可选属性，输出时以 key=value 形式追加到消息末尾
     */
    message(level lvl = level::info, mc::string_view msg = "", context ctx = context(), mc::dict args = mc::dict(),
            mc::dict attrs = mc::dict());

    /**
     * @brief 格式化构造函数
     *
     * @param lvl 日志级别
     * @param ctx 上下文信息
     * @param fmt_template 格式模板
     * @param args 参数字典
     * @param attrs 可选属性，输出时以 key=value 形式追加到消息末尾
     */
    message(level lvl, context ctx, mc::string_view fmt_template, mc::dict args = mc::dict(), mc::dict attrs = mc::dict());

    message(const message& other)            = default;
    message& operator=(const message& other) = default;

    message(message&& other) noexcept            = default;
    message& operator=(message&& other) noexcept = default;

    /**
     * @brief 获取日志级别
     *
     * @return level 日志级别
     */
    level get_level() const;

    /**
     * @brief 获取上下文信息
     *
     * @return const context& 上下文信息
     */
    const context& get_context() const;

    /**
     * @brief 获取时间戳
     *
     * @return const std::chrono::system_clock::time_point& 时间戳
     */
    const std::chrono::system_clock::time_point& get_timestamp() const;

    /**
     * @brief 获取参数字典
     *
     * @return const dict& 参数字典
     */
    const dict& get_args() const;

    /**
     * @brief 获取参数字典（非 const 版本，用于修改）
     *
     * @return dict& 参数字典
     */
    dict& get_args();

    /**
     * @brief 获取属性字典（可选 attrs，输出时为 key=value）
     *
     * @return const dict& 属性字典
     */
    const dict& get_attrs() const;

    /**
     * @brief 获取日志类别
     *
     * @return log_category 日志类别
     */
    log_category get_category() const noexcept;

    /**
     * @brief 设置日志类别
     *
     * @param category 日志类别
     */
    void set_category(log_category category) noexcept;

    /**
     * @brief 获取格式模板
     *
     * @return mc::string_view 格式模板
     */
    mc::string_view get_format_template() const;

    /**
     * @brief 获取线程ID
     *
     * @return const std::thread::id& 线程ID
     */
    mc::thread_id get_thread_id() const;

    /**
     * @brief 获取消息内容
     *
     * @return mc::string_view 消息内容
     */
    mc::string_view get_message() const;

    /**
     * @brief 获取结构化数据
     *
     * @return dict 结构化数据
     */
    dict to_structured_data() const;

    /**
     * @brief 获取是否限流
     *
     * @return bool true 表示限流，false 表示不限流（_easy）
     */
    bool get_limit() const noexcept;

    /**
     * @brief 设置是否限流
     *
     * @param limit true 表示限流，false 表示不限流（_easy）
     */
    void set_limit(bool limit) noexcept;

private:
    level                                 m_level;                         // 日志级别
    log_category                          m_category{log_category::debug}; // 日志类别
    mutable mc::string                   m_message;                       // 消息内容
    context                               m_context;                       // 上下文信息
    std::chrono::system_clock::time_point m_timestamp;                     // 时间戳
    dict                                  m_args;                          // 参数字典
    dict                                  m_attrs;                         // 可选属性，输出时 key=value 追加
    mc::string                           m_format;                        // 格式模板
    mc::thread_id                         m_thread_id;                     // 线程ID
    mutable bool                          m_formatted;                     // 是否已格式化
    mutable bool                          m_attrs_appended;                // attrs 是否已追加到 m_message
    mutable bool                          m_system_id_appended{false};     // system_id 前缀是否已追加
    mutable bool                          m_period_appended{false};        // period 后缀是否已追加
    bool                                  m_limit{true}; // 是否限流，true 限流，false 不限流（_easy）
};

/**
 * @brief 日志消息列表
 */
using messages = std::vector<message>;

namespace detail {

// 添加位置参数
template <typename T, std::enable_if_t<!mc::fmt::detail::is_named_arg_v<T>, int> = 0>
constexpr void add_arg(mc::dict& out, const T& value)
{
    out.emplace(out.size(), value);
}

// 添加命名参数的特化
template <typename T>
void add_arg(mc::dict& out, const mc::fmt::detail::named_arg<T>& arg)
{
    out.emplace(arg.name(), arg.value());
}

inline void add_arg(mc::dict&, const std::monostate&)
{}

template <typename... Args>
mc::dict make_args(Args&&... args)
{
    mc::dict out;
    (add_arg(out, std::forward<Args>(args)), ...);
    return out;
}

template <typename Tuple, size_t... I>
mc::dict make_args_from_tuple(Tuple&& tuple, std::index_sequence<I...>)
{
    return make_args(std::get<I>(std::forward<Tuple>(tuple))...);
}

// 辅助函数：创建带可选 attrs 的消息
// 当最后一个参数是 dict 时，将其作为 attrs，其余参数作为格式参数
// 使用函数重载让编译器自动选择正确的重载
template <typename... FormatArgs>
message make_message_with_optional_attrs(level lvl, context ctx, mc::string_view format, FormatArgs&&... format_args,
                                         const mc::dict& attrs)
{
    // 最后一个参数是 dict，将其作为 attrs，其余参数作为格式参数
    auto args = make_args(std::forward<FormatArgs>(format_args)...);
    return message(lvl, ctx, format, args, attrs);
}

template <typename... FormatArgs>
message make_message_with_optional_attrs(level lvl, context ctx, mc::string_view format, FormatArgs&&... format_args,
                                         mc::dict& attrs)
{
    auto args = make_args(std::forward<FormatArgs>(format_args)...);
    return message(lvl, ctx, format, args, attrs);
}

template <typename... FormatArgs>
message make_message_with_optional_attrs(level lvl, context ctx, mc::string_view format, FormatArgs&&... format_args,
                                         mc::dict&& attrs)
{
    auto args = make_args(std::forward<FormatArgs>(format_args)...);
    return message(lvl, ctx, format, args, std::move(attrs));
}

// 当最后一个参数不是 dict 时，所有参数都是格式参数
// 这个重载会被选择当最后一个参数不是 dict 类型时
template <typename... Args>
message make_message_with_optional_attrs(level lvl, context ctx, mc::string_view format, Args&&... args)
{
    auto format_args = make_args(std::forward<Args>(args)...);
    return message(lvl, ctx, format, format_args, mc::dict{});
}

template <typename... Args>
message make_message_from_args_seq(level lvl, context ctx, mc::string_view format, Args&&... args)
{
    auto tuple = std::forward_as_tuple(std::forward<Args>(args)...);

    constexpr size_t arg_count = sizeof...(Args);
    using tuple_type           = std::remove_reference_t<decltype(tuple)>;
    using last_arg_type =
        std::remove_cv_t<std::remove_reference_t<std::tuple_element_t<arg_count - 1, tuple_type>>>;

    if constexpr (std::is_same_v<last_arg_type, std::monostate>) {
        if constexpr (arg_count >= 2) {
            using prev_arg_type =
                std::remove_cv_t<std::remove_reference_t<std::tuple_element_t<arg_count - 2, tuple_type>>>;

            if constexpr (std::is_same_v<prev_arg_type, mc::dict>) {
                auto format_args = make_args_from_tuple(tuple, std::make_index_sequence<arg_count - 2>{});
                return message(lvl, ctx, format, std::move(format_args), mc::dict(std::get<arg_count - 2>(tuple)));
            }
        }

        auto format_args = make_args_from_tuple(tuple, std::make_index_sequence<arg_count - 1>{});
        return message(lvl, ctx, format, std::move(format_args), mc::dict{});
    }

    return make_message_with_optional_attrs(lvl, ctx, format, std::forward<Args>(args)...);
}

} // namespace detail

} // namespace mc::log

/**
 * @brief 基础日志宏 - 所有级别共用（支持可选 attrs）
 * 如果最后一个参数是 mc::dict 类型，则将其作为 attrs，其余参数作为格式参数
 */
#define MC_LOG_MESSAGE_N(LEVEL, CATEGORY, COMPILE_CHECK, FORMAT, ...)                                                  \
    [&]() -> mc::log::message {                                                                                        \
        static_assert(COMPILE_CHECK(FORMAT, __VA_ARGS__), "格式化字符串或参数错误");                                   \
        auto __m = mc::log::detail::make_message_from_args_seq(                                                        \
            mc::log::level::LEVEL, mc::log::context(__FILE__, __FUNCTION__, __LINE__), FORMAT,                         \
            BOOST_PP_SEQ_FOR_EACH(MC_FORMAT_CHECK_ARG, MC_FORMAT_APPLY_ARG_NAMED,                                      \
                                  BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__)) std::monostate{});                            \
        __m.set_category(CATEGORY);                                                                                    \
        return __m;                                                                                                    \
    }()

#define MC_LOG_MESSAGE_0(LEVEL, CATEGORY, COMPILE_CHECK, FORMAT)                                                       \
    [&]() -> mc::log::message {                                                                                        \
        static_assert(COMPILE_CHECK(FORMAT), "格式化字符串或参数错误");                                                \
        auto log_msg =                                                                                                 \
            mc::log::message(mc::log::level::LEVEL, mc::log::context(__FILE__, __FUNCTION__, __LINE__), FORMAT);       \
        log_msg.set_category(CATEGORY);                                                                                \
        return log_msg;                                                                                                \
    }()

#define MC_LOG_DISPATCH(LEVEL, CATEGORY, COMPILE_CHECK, FORMAT, ...)                                                   \
    BOOST_PP_IF(BOOST_PP_EQUAL(BOOST_PP_VARIADIC_SIZE(dummy, ##__VA_ARGS__), 1),                                       \
                MC_LOG_MESSAGE_0(LEVEL, CATEGORY, COMPILE_CHECK, FORMAT),                                              \
                MC_LOG_MESSAGE_N(LEVEL, CATEGORY, COMPILE_CHECK, FORMAT, __VA_ARGS__))

// 暂不做检查，等后续优化，使用默认调试日志类别，指定日志级别
#define MC_LOG_MESSAGE(LEVEL, ...)                                                                                     \
    MC_LOG_DISPATCH(LEVEL, mc::log::log_category::debug, MC_FORMAT_EMPTY_CHECK, __VA_ARGS__)

// 暂不做检查，等后续优化，使用默认所有日志级别，指定日志类别
#define MC_LOG_MESSAGE_WITH_CATEGORY(CATEGORY, ...) MC_LOG_DISPATCH(all, CATEGORY, MC_FORMAT_EMPTY_CHECK, __VA_ARGS__)

#define MC_LOG_MESSAGE_UNSAFE(LEVEL, ...)                                                                              \
    MC_LOG_DISPATCH(LEVEL, mc::log::log_category::debug, MC_FORMAT_EMPTY_CHECK, __VA_ARGS__)

// 不限流（_easy）的日志消息：internal_log_handler 第二个参数为 false
#define MC_LOG_MESSAGE_EASY(LEVEL, ...)                                                                                \
    [&]() -> mc::log::message {                                                                                        \
        auto __m = MC_LOG_MESSAGE(LEVEL, __VA_ARGS__);                                                                 \
        __m.set_limit(false);                                                                                          \
        return __m;                                                                                                    \
    }()

#endif // MC_LOG_MESSAGE_H