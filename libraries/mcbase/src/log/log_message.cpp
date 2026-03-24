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
#include <mc/fmt/format_context.h>
#include <mc/fmt/format_dict.h>
#include <mc/fmt/formatter_mc.h>
#include <mc/log/log_message.h>
#include <mc/string.h>
#include <mc/variant.h>

#include <cstdio>
#include <iostream>

namespace mc::log {

message::message(level lvl, mc::string_view msg, context ctx, mc::dict args, mc::dict attrs)
    : m_level(lvl), m_category(log_category::debug), m_message(mc::string(msg)), m_context(std::move(ctx)),
      m_timestamp(std::chrono::system_clock::now()), m_args(std::move(args)), m_attrs(std::move(attrs)), m_format(),
      m_thread_id(mc::get_thread_id()), m_formatted(true), m_attrs_appended(false), m_system_id_appended(false),
      m_period_appended(false)
{}

message::message(level lvl, context ctx, mc::string_view fmt_template, mc::dict args, mc::dict attrs)
    : m_level(lvl), m_category(log_category::debug), m_message(""), // 初始为空，将在需要时格式化
      m_context(std::move(ctx)), m_timestamp(std::chrono::system_clock::now()), m_args(std::move(args)),
      m_attrs(std::move(attrs)), m_format(mc::string(fmt_template)), m_thread_id(mc::get_thread_id()),
      m_formatted(false), m_attrs_appended(false), m_system_id_appended(false), m_period_appended(false)
{}

namespace {
// 递归将 variant 按 key=value 风格追加（嵌套 dict 输出为 key={ ... }，与顶层 attrs 一致）
void append_attr_value(mc::fmt::format_context& ctx, const mc::variant& v, const mc::fmt::detail::format_spec& spec)
{
    if (v.is_object()) {
        const mc::dict& d = v.get_object();
        ctx.append('{');
        bool first = true;
        for (const auto& e : d) {
            if (!first) {
                ctx.append(' ');
            }
            first = false;
            mc::fmt::detail::format_variant(e.key, ctx, spec);
            ctx.append('=');
            append_attr_value(ctx, e.value, spec);
        }
        ctx.append('}');
    } else {
        mc::fmt::detail::format_variant(v, ctx, spec);
    }
}

void append_attrs_key_value(mc::string& result, const mc::dict& attrs)
{
    if (attrs.empty()) {
        return; // 提前返回，避免不必要的操作
    }
    mc::fmt::format_context      ctx(result);
    mc::fmt::detail::format_spec spec{};
    for (const auto& e : attrs) {
        ctx.append(' ');
        mc::fmt::detail::format_variant(e.key, ctx, spec);
        ctx.append('=');
        append_attr_value(ctx, e.value, spec);
    }
}
} // namespace

mc::string_view message::get_message() const
{
    if (!m_formatted && !m_format.empty()) {
        // 如果未格式化且有格式模板，执行格式化
        m_message   = mc::format_dict(m_format, m_args);
        m_formatted = true;
    }

    // 检查并添加 system_id 前缀
    if (m_args.contains("system_id") && !m_system_id_appended && m_args["system_id"].is_integer()) {
        int system_id        = m_args["system_id"].as<int>();
        m_message            = mc::string("[System") + mc::to_string(system_id) + "]" + m_message;
        m_system_id_appended = true;
    }

    // 检查并添加 period 后缀
    if (m_args.contains("period") && !m_period_appended && m_args["period"].is_integer()) {
        int period        = m_args["period"].as<int>();
        m_message         = m_message + mc::string(" [period:") + mc::to_string(period) + "(s)]";
        m_period_appended = true;
    }

    if (!m_attrs.empty() && !m_attrs_appended) {
        append_attrs_key_value(m_message, m_attrs);
        m_attrs_appended = true;
    }
    return m_message;
}

level message::get_level() const
{
    return m_level;
}

mc::string_view message::get_format_template() const
{
    return m_format;
}

const dict& message::get_args() const
{
    return m_args;
}

dict& message::get_args()
{
    return m_args;
}

const dict& message::get_attrs() const
{
    return m_attrs;
}

log_category message::get_category() const noexcept
{
    return m_category;
}

void message::set_category(log_category category) noexcept
{
    m_category = category;
}

bool message::get_limit() const noexcept
{
    return m_limit;
}

void message::set_limit(bool limit) noexcept
{
    m_limit = limit;
}

const context& message::get_context() const
{
    return m_context;
}

mc::thread_id message::get_thread_id() const
{
    return m_thread_id;
}

const std::chrono::system_clock::time_point& message::get_timestamp() const
{
    return m_timestamp;
}

mc::dict message::to_structured_data() const
{
    mc::dict result;

    // 基本元数据
    result["level"]    = static_cast<int>(m_level);
    result["category"] = static_cast<int>(m_category);

    // 上下文信息
    mc::dict context_dict;
    context_dict["file"]     = mc::string_view(m_context.m_file.view());
    context_dict["function"] = mc::string_view(m_context.m_function.view());
    context_dict["line"]     = m_context.m_line;
    // 使用stringstream转换thread::id为字符串
    std::ostringstream thread_id_stream;
    thread_id_stream << m_thread_id;
    context_dict["thread_id"] = mc::string(thread_id_stream.str());
    result["context"]         = context_dict;

    // 消息内容
    if (!m_format.empty()) {
        result["message_template"] = m_format;
        result["args"]             = m_args;
    }

    // 获取消息（如果需要会自动格式化）
    result["message"] = mc::string_view(get_message());

    if (!m_attrs.empty()) {
        result["attrs"] = m_attrs;
    }

    // 直接返回，会自动调用转换操作符
    return result;
}

} // namespace mc::log