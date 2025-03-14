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

#include <iomanip>
#include <mc/log/log_message.h>
#include <regex>
#include <sstream>

namespace mc {
namespace log {

std::string default_message_formatter::format(const message& msg) const {
    std::ostringstream ss;

    // 格式化时间戳
    auto time_t = std::chrono::system_clock::to_time_t(msg.get_timestamp());
    auto ms     = std::chrono::duration_cast<std::chrono::milliseconds>(
                  msg.get_timestamp().time_since_epoch()) %
              1000;

    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") << '.' << std::setfill('0')
       << std::setw(3) << ms.count() << " [" << to_string(msg.get_level()) << "] ";

    // 添加上下文信息
    const auto& ctx = msg.get_context();
    if (!ctx.m_file.empty()) {
        // 提取文件名（不包含路径）
        size_t      pos      = ctx.m_file.find_last_of("/\\");
        std::string filename = (pos == std::string::npos) ? ctx.m_file : ctx.m_file.substr(pos + 1);

        ss << filename << ":" << ctx.m_line << " ";
    }

    // 使用延迟格式化的消息
    ss << msg.get_formatted_message();

    return ss.str();
}

std::string structured_message_formatter::format(const message& msg) const {
    // 将消息转换为结构化数据
    dict structured_data = msg.to_structured_data();

    // 添加时间戳
    auto time_t = std::chrono::system_clock::to_time_t(msg.get_timestamp());
    auto ms     = std::chrono::duration_cast<std::chrono::milliseconds>(
                  msg.get_timestamp().time_since_epoch()) %
              1000;

    std::ostringstream time_ss;
    time_ss << std::put_time(std::localtime(&time_t), "%Y-%m-%dT%H:%M:%S") << '.'
            << std::setfill('0') << std::setw(3) << ms.count();

    // 修改可变的字典副本以添加时间戳
    mc::mutable_dict result(structured_data);
    result["timestamp"] = time_ss.str();

    return dict_to_string(result);
}

// 辅助函数：字典转字符串
std::string dict_to_string(const dict& d) {
    std::ostringstream ss;
    ss << "{";
    bool first = true;

    for (const auto& key : d.keys()) {
        if (!first) {
            ss << ", ";
        }
        ss << "\"" << key << "\": ";

        const variant& value = d[key];

        if (value.is_object()) {
            // 递归处理嵌套字典
            ss << dict_to_string(value.as<dict>());
        } else if (value.is_string()) {
            // 字符串值需要引号
            ss << "\"" << value.as<std::string>() << "\"";
        } else {
            // 其他类型值
            ss << value.as_string();
        }

        first = false;
    }

    ss << "}";
    return ss.str();
}

} // namespace log
} // namespace mc