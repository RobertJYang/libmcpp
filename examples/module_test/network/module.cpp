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

namespace mc::network {

// 模块加载计数器
static int   s_load_count     = 0;
static void* s_module_address = nullptr;

/**
 * @brief 模块加载时调用的构造函数
 */
__attribute__((constructor)) void module_load_detector() {
    // 记录模块加载
    ++s_load_count;
    s_module_address = (void*)&s_load_count; // 使用一个静态变量的地址作为模块地址标识
    std::cout << "[NETWORK MODULE] 模块加载 #" << s_load_count
              << " (地址: " << s_module_address << ")" << std::endl;
}

/**
 * @brief 模块卸载时调用的析构函数
 */
__attribute__((destructor)) void module_unload_detector() {
    // 记录模块卸载
    std::cout << "[NETWORK MODULE] 动态库真正卸载! 地址: "
              << s_module_address << std::endl;
}

} // namespace mc::network

// 导出网络状态枚举到模块
MC_MODULE_REFLECT_ENUM(mc_network,
                       mc::network::connection_status,
                       (DISCONNECTED)(CONNECTING)(CONNECTED)(ERROR))

// 导出协议类型枚举到模块
MC_MODULE_REFLECT_ENUM(mc_network,
                       mc::network::protocol_type,
                       (TCP)(UDP)(HTTP)(HTTPS))

// 导出网络客户端类到模块
MC_MODULE_REFLECT(mc_network,
                  mc::network::network_client,
                  ((connect, "connect"))           // 连接
                  ((disconnect, "disconnect"))     // 断开连接
                  ((send_data, "sendData"))        // 发送数据
                  ((receive_data, "receiveData"))  // 接收数据
                  ((get_status, "getStatus"))      // 获取状态
                  ((get_host, "getHost"))          // 获取主机
                  ((get_port, "getPort"))          // 获取端口
                  ((set_protocol, "setProtocol"))  // 设置协议
                  ((get_protocol, "getProtocol"))) // 获取协议

MC_EXPORT_MODULE(mc_network)