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

#ifndef MC_PROTOCOL_BASE_H
#define MC_PROTOCOL_BASE_H

#include <mc/module.h>

MC_MODULE(mc_protocol)

namespace mc::proto {

/**
 * @brief 消息类型枚举
 */
enum class message_type {
    REQUEST,     // 请求消息
    RESPONSE,    // 响应消息
    NOTIFICATION // 通知消息
};

/**
 * @brief 协议版本枚举
 */
enum class protocol_version {
    V1_0, // 版本 1.0
    V1_1, // 版本 1.1
    V2_0  // 版本 2.0
};

} // namespace mc::proto

MC_REFLECTABLE("MessageType", mc::proto::message_type)
MC_REFLECTABLE("ProtocolVersion", mc::proto::protocol_version)

#endif // MC_PROTOCOL_BASE_H