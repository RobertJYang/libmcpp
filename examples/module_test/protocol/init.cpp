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
#include "handler.h"

// 导出消息类型枚举到模块
MC_MODULE_REFLECT_ENUM(mc_protocol, mc::proto::message_type, (REQUEST)(RESPONSE)(NOTIFICATION))

// 导出协议版本枚举到模块
MC_MODULE_REFLECT_ENUM(mc_protocol, mc::proto::protocol_version, (V1_0)(V1_1)(V2_0))

// 导出协议处理器类到模块
MC_MODULE_REFLECT(mc_protocol, mc::proto::protocol_handler,
                  ((handle_request, "handleRequest"))                // 处理请求
                  ((handle_response, "handleResponse"))              // 处理响应
                  ((send_notification, "sendNotification"))          // 发送通知
                  ((get_notification_count, "getNotificationCount")) // 获取通知数量
                  ((get_last_request, "getLastRequest"))             // 获取最后请求
                  ((get_last_response, "getLastResponse"))           // 获取最后响应
                  ((clear_messages, "clearMessages"))                // 清空消息
                  ((get_version_string, "getVersionString")))        // 获取版本字符串

MC_EXPORT_MODULE(mc_protocol)