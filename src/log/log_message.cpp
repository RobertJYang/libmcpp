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

#include <mc/log/log_message.h>
#include <mc/string.h>
namespace mc::log {

message::message(level lvl, std::string msg, context ctx, mc::mutable_dict args)
    : m_level(lvl), m_message(std::move(msg)), m_context(std::move(ctx)),
      m_timestamp(std::chrono::system_clock::now()), m_args(std::move(args)),
      m_thread_id(mc::get_thread_id()), m_formatted(true) {
}

message::message(level lvl, context ctx, std::string fmt_template,
                 mc::mutable_dict args)
    : m_level(lvl), m_message(""), // 初始为空，将在需要时格式化
      m_context(std::move(ctx)), m_timestamp(std::chrono::system_clock::now()),
      m_args(std::move(args)), m_format(std::move(fmt_template)),
      m_thread_id(mc::get_thread_id()), m_formatted(false) {
}

const std::string& message::get_message() const {
    if (!m_formatted && !m_format.empty()) {
        // 如果未格式化且有格式模板，执行格式化
        m_message   = mc::format_dict(m_format, m_args);
        m_formatted = true;
    }
    return m_message;
}

dict message::to_structured_data() const {
    mc::mutable_dict result;

    // 基本元数据
    result["level"] = static_cast<int>(m_level);

    // 上下文信息
    mc::mutable_dict context_dict;
    context_dict["file"]     = m_context.m_file;
    context_dict["function"] = m_context.m_function;
    context_dict["line"]     = m_context.m_line;
    // 使用stringstream转换thread::id为字符串
    std::ostringstream thread_id_stream;
    thread_id_stream << m_thread_id;
    context_dict["thread_id"] = thread_id_stream.str();
    result["context"]         = context_dict;

    // 消息内容
    if (!m_format.empty()) {
        result["message_template"] = m_format;
        result["args"]             = m_args;
    }

    // 获取消息（如果需要会自动格式化）
    result["message"] = get_message();

    // 直接返回，会自动调用转换操作符
    return result;
}

} // namespace mc::log