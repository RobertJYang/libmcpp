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

#ifndef MC_LOG_CONSOLE_APPENDER_H
#define MC_LOG_CONSOLE_APPENDER_H

#include <mc/log/appender.h>
#include <mc/log/logger.h>
#include <mc/reflect.h>
#include <mc/variant.h>
#include <memory>
#include <string>
#include <vector>

namespace mc {
namespace log {

/**
 * @brief 控制台追加器
 *
 * 将日志消息输出到标准输出或标准错误，支持颜色输出
 */
class MC_API console_appender : public appender {
public:
    /**
     * @brief 输出流类型
     */
    enum class stream_type {
        std_out,  // 标准输出
        std_error // 标准错误
    };

    /**
     * @brief 颜色类型
     */
    enum class color_type {
        console_default, // 默认颜色
        red,             // 红色
        green,           // 绿色
        brown,           // 棕色
        blue,            // 蓝色
        magenta,         // 洋红色
        cyan,            // 青色
        white            // 白色
    };

    /**
     * @brief 日志级别颜色配置
     */
    struct level_color {
        MC_REFLECTABLE("mc.log.console_appender.level_color");

        log::level level; // 日志级别
        color_type color; // 对应颜色

        level_color() = default;
        level_color(log::level lvl, color_type clr) : level(lvl), color(clr)
        {}
    };

    /**
     * @brief 控制台追加器配置
     */
    struct config {
        MC_REFLECTABLE("mc.log.console_appender.config");

        stream_type              stream{stream_type::std_out}; // 输出流
        bool                     use_color{true};              // 是否使用颜色
        bool                     flush{true};                  // 是否每次输出后刷新
        std::vector<level_color> level_colors;                 // 日志级别颜色配置
    };

    /**
     * @brief 默认构造函数
     */
    console_appender();

    /**
     * @brief 析构函数
     */
    ~console_appender() override;

    /**
     * @brief 初始化追加器
     *
     * @param args 配置参数
     * @return bool 初始化是否成功
     */
    bool init(const variant& args) override;

    /**
     * @brief 追加日志消息
     *
     * @param msg 日志消息
     */
    void append(const message& msg) override;

private:
    /**
     * @brief 配置追加器
     *
     * @param cfg 配置
     */
    void configure(const config& cfg);

    /**
     * @brief 打印文本
     *
     * @param text 文本内容
     * @param text_color 文本颜色
     */
    void print(const std::string& text, color_type color);

    class impl;
    std::unique_ptr<impl> m_impl;
};

} // namespace log
} // namespace mc

MC_REFLECTABLE("mc.log.console_appender.color_type", mc::log::console_appender::color_type)
MC_REFLECTABLE("mc.log.console_appender.stream_type", mc::log::console_appender::stream_type)

#endif // MC_LOG_CONSOLE_APPENDER_H