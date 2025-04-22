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

#ifndef MC_DBUS_ENUMS_H
#define MC_DBUS_ENUMS_H

#include <stdint.h>
#include <string>

namespace mc::dbus {

/**
 * DBus总线类型
 */
enum class bus_type {
    session, // 会话总线
    system,  // 系统总线
    starter  // 启动器总线
};

/**
 * @brief DBus消息类型枚举
 */
enum class message_type {
    invalid       = 0, /**< 无效消息类型 */
    method_call   = 1, /**< 方法调用消息 */
    method_return = 2, /**< 方法返回消息 */
    error         = 3, /**< 错误消息 */
    signal        = 4  /**< 信号消息 */
};

/**
 * @brief DBus消息头字段类型枚举
 */
enum class message_header_field {
    invalid      = 0, /**< 无效字段 */
    path         = 1, /**< 对象路径 */
    interface    = 2, /**< 接口名称 */
    member       = 3, /**< 成员名称(方法/信号) */
    error_name   = 4, /**< 错误名称 */
    reply_serial = 5, /**< 回复的序列号 */
    destination  = 6, /**< 目标服务 */
    sender       = 7, /**< 发送者 */
    signature    = 8, /**< 消息体签名 */
    unix_fds     = 9  /**< UNIX文件描述符数量 */
};

/**
 * 属性访问类型
 */
enum class property_access {
    read,      // 只读
    write,     // 只写
    read_write // 读写
};

/**
 * @brief 消息标志位
 */
enum class message_flag {
    no_reply_expected               = 0x01, /**< 不期望回复 */
    no_auto_start                   = 0x02, /**< 不自动启动服务 */
    allow_interactive_authorization = 0x04  /**< 允许交互式授权 */
};

} // namespace mc::dbus

#endif // MC_DBUS_ENUMS_H