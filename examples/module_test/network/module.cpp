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

#include "base.h"
#include "client.h"
#include <iostream>

// 导出网络状态枚举到模块
MC_MODULE_REFLECT_ENUM(mc_network, mc::network::connection_status, (DISCONNECTED)(CONNECTING)(CONNECTED)(ERROR))

// 导出协议类型枚举到模块
MC_MODULE_REFLECT_ENUM(mc_network, mc::network::protocol_type, (TCP)(UDP)(HTTP)(HTTPS))

// 导出网络客户端类到模块
MC_MODULE_REFLECT(mc_network, mc::network::network_client,
                  ((connect, "connect"))          // 连接
                  ((disconnect, "disconnect"))    // 断开连接
                  ((send_data, "sendData"))       // 发送数据
                  ((receive_data, "receiveData")) // 接收数据
                  ((get_host, "getHost"))         // 获取主机
                  ((get_port, "getPort")))        // 获取端口

MC_EXPORT_MODULE(mc_network)