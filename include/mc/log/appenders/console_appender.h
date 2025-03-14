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

namespace mc {
namespace log {

/**
 * @brief 控制台追加器
 *
 * 将日志消息输出到标准输出
 */
class console_appender : public appender {
public:
    /**
     * @brief 构造函数
     */
    console_appender() = default;

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
};

} // namespace log
} // namespace mc

#endif // MC_LOG_CONSOLE_APPENDER_H