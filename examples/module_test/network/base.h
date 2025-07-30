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

#ifndef MC_NETWORK_BASE_H
#define MC_NETWORK_BASE_H

#include <mc/module.h>

MC_MODULE(mc_network)

namespace mc::network {

/**
 * @brief 网络连接状态枚举
 */
enum class connection_status {
    DISCONNECTED, // 未连接
    CONNECTING,   // 连接中
    CONNECTED,    // 已连接
    ERROR         // 错误状态
};

/**
 * @brief 网络协议类型枚举
 */
enum class protocol_type {
    TCP,  // TCP协议
    UDP,  // UDP协议
    HTTP, // HTTP协议
    HTTPS // HTTPS协议
};

} // namespace mc::network

MC_REFLECTABLE("ConnectionStatus", mc::network::connection_status)
MC_REFLECTABLE("ProtocolType", mc::network::protocol_type)

#endif // MC_NETWORK_BASE_H