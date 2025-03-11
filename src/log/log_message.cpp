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
#include <iomanip>
#include <sstream>
#include <regex>

namespace mc {
namespace log {

std::string message::formatted_message() const {
    // 使用默认格式化器格式化消息
    default_message_formatter formatter;
    return formatter.format(*this);
}

std::string default_message_formatter::format(const message& msg) const {
    std::ostringstream ss;
    
    // 格式化时间戳
    auto time_t = std::chrono::system_clock::to_time_t(msg.m_timestamp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        msg.m_timestamp.time_since_epoch()) % 1000;
    
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S")
       << '.' << std::setfill('0') << std::setw(3) << ms.count()
       << " [" << to_string(msg.m_level) << "] ";
    
    // 添加上下文信息
    if (!msg.m_context.m_file.empty()) {
        // 提取文件名（不包含路径）
        size_t pos = msg.m_context.m_file.find_last_of("/\\");
        std::string filename = (pos == std::string::npos) ? 
            msg.m_context.m_file : msg.m_context.m_file.substr(pos + 1);
        
        ss << filename << ":" << msg.m_context.m_line << " ";
    }
    
    // 处理消息文本，替换占位符
    std::string message_text = msg.m_message;
    
    // 如果消息中包含占位符并且有参数，则进行替换
    if (!msg.m_args.empty()) {
        std::regex placeholder_regex("\\$\\{([^}]+)\\}");
        std::string::const_iterator search_start(message_text.cbegin());
        std::string formatted_message;
        formatted_message.reserve(message_text.size() * 2);
        
        size_t last_pos = 0;
        std::smatch match;
        
        while (std::regex_search(search_start, message_text.cend(), match, placeholder_regex)) {
            // 添加占位符前的文本
            size_t pos = match.position() + (search_start - message_text.cbegin());
            formatted_message.append(message_text, last_pos, pos - last_pos);
            
            // 获取占位符的键
            std::string key = match[1].str();
            
            // 查找键对应的值
            if (msg.m_args.contains(key)) {
                // 将值转换为字符串并添加到结果中
                formatted_message.append(msg.m_args[key].as_string());
            } else {
                // 如果键不存在，保留原始占位符
                formatted_message.append(match[0].str());
            }
            
            // 更新搜索起点和上次匹配位置
            search_start = match.suffix().first;
            last_pos = pos + match.length();
        }
        
        // 添加最后一个占位符后的文本
        formatted_message.append(message_text, last_pos, message_text.size() - last_pos);
        
        message_text = formatted_message;
    }
    
    ss << message_text;
    
    return ss.str();
}

} // namespace log
} // namespace mc 