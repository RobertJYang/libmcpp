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
#include <mc/filesystem.h>
#include <mc/log/appenders/console_appender.h>
#include <mc/reflect.h>
#include <mc/string.h>
#include <mc/time.h>
#include <mc/variant.h>

#include <unistd.h>

#define COLOR_CONSOLE 1
#include "console_defines.h"

#include <chrono>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>

MC_REFLECT_ENUM(mc::log::console_appender::color_type,
                (console_default)(red)(green)(brown)(blue)(magenta)(cyan)(white))
MC_REFLECT_ENUM(mc::log::console_appender::stream_type, (std_out)(std_error))
MC_REFLECT(mc::log::console_appender::config, (stream)(use_color)(flush)(level_colors))
MC_REFLECT(mc::log::console_appender::level_color, (level)(color))

namespace mc {
namespace log {

class console_appender::impl {
public:
    config     cfg;
    color_type level_colors[static_cast<int>(level::off) + 1] = {
        color_type::console_default, color_type::console_default, color_type::cyan,
        color_type::green, color_type::brown, color_type::red,
        color_type::magenta, color_type::console_default};

    std::mutex mutex;
};

console_appender::console_appender() : m_impl(new impl()) {
}

console_appender::~console_appender() = default;

bool console_appender::init(const variant& args) {
    try {
        configure(args.as<config>());
        return true;
    } catch (const mc::exception& e) {
        std::cerr << "console_appender 初始化失败: " << e.what() << std::endl;
        return false;
    }
}

void console_appender::configure(const config& cfg) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);

    m_impl->cfg = cfg;
    for (const auto& lc : m_impl->cfg.level_colors) {
        if (static_cast<int>(lc.level) < static_cast<int>(level::off) + 1) {
            m_impl->level_colors[static_cast<int>(lc.level)] = lc.color;
        }
    }
}

// 获取控制台颜色代码
static const char* get_console_color(console_appender::color_type clr) {
    switch (clr) {
    case console_appender::color_type::red:
        return CONSOLE_RED;
    case console_appender::color_type::green:
        return CONSOLE_GREEN;
    case console_appender::color_type::brown:
        return CONSOLE_BROWN;
    case console_appender::color_type::blue:
        return CONSOLE_BLUE;
    case console_appender::color_type::magenta:
        return CONSOLE_MAGENTA;
    case console_appender::color_type::cyan:
        return CONSOLE_CYAN;
    case console_appender::color_type::white:
        return CONSOLE_WHITE;
    case console_appender::color_type::console_default:
    default:
        return CONSOLE_DEFAULT;
    }
}

void console_appender::append(const message& msg) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);

    // 获取输出流
    FILE* out = m_impl->cfg.stream == stream_type::std_error ? stderr : stdout;

    // 获取上下文信息
    const context& ctx = msg.get_context();

    // 构建日志行 - 预分配足够的空间避免重新分配
    std::string line;
    line.reserve(512); // 增加预留空间，避免多次重分配

    // 格式化日志消息
    // 日志级别 (5字符宽度)
    mc::string::fixed_width_append(line, 5, mc::log::to_string(msg.get_level()));
    line.push_back(' ');

    // 时间戳（使用当前时间而不是消息时间，避免长时间处理导致的时间差）
    std::string_view time_str = mc::time_point::now();
    line.append(time_str);
    line.push_back(' ');

    // 添加线程ID
    line.append("[" + std::to_string(msg.get_thread_id()) + "] ");

    // 文件和行号 (25字符宽度)
    std::string file_line;
    file_line.reserve(64); // 预分配足够空间

    if (ctx.m_file.empty()) {
        file_line.append("unknown");
    } else {
        file_line.append(mc::filesystem::basename(ctx.m_file));
    }
    file_line.push_back(':');
    file_line.append(std::to_string(ctx.m_line));

    mc::string::fixed_width_append(line, 25, file_line);
    line.push_back(' ');

    // 函数名 (20字符宽度)
    if (!ctx.m_function.empty()) {
        std::string_view func_view(ctx.m_function);
        // 去除命名空间前缀
        size_t pos = func_view.find_last_of(':');
        if (pos != std::string::npos && pos + 1 < func_view.size()) {
            func_view = func_view.substr(pos + 1);
        }
        mc::string::fixed_width_append(line, 20, func_view);
        line.push_back(' ');
    }

    // 消息内容
    line.append("| ");
    line.append(msg.get_message());

    // 输出带颜色的日志
    color_type text_color = m_impl->level_colors[static_cast<int>(msg.get_level())];
    print(line, text_color);

    // 输出换行符
    fprintf(out, "\n");

    // 如果需要，刷新输出
    if (m_impl->cfg.flush) {
        fflush(out);
    }
}

void console_appender::print(const std::string& text, color_type text_color) {
    if (text.empty()) {
        return;
    }

    FILE* out = m_impl->cfg.stream == stream_type::std_error ? stderr : stdout;

    // 如果启用了颜色
    if (m_impl->cfg.use_color) {
        // ANSI 颜色设置（仅在终端时）
        if (isatty(fileno(out))) {
            fprintf(out, "%s", get_console_color(text_color));
        }
    }

    // 输出文本
    fprintf(out, "%s", text.c_str());

    // 如果启用了颜色，重置为默认颜色
    if (m_impl->cfg.use_color) {
        if (isatty(fileno(out))) {
            fprintf(out, "%s", CONSOLE_DEFAULT);
        }
    }
}

} // namespace log
} // namespace mc