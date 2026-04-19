/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

/*
 * 协议栈公开入口：
 *   - 协议栈对象：`auto proto = mc::proto::make_instance<...>();`，持有各层 protocol 实例，可承载长期协议状态；
 *   - 请求对象：`auto req = mc::proto::make_request<...>();`，默认可直接在栈上构建；
 *   - 出站：`req.prepare_buffer().append_payload(...)`，再 `proto->push(req)`；
 *   - 每层结构化请求数据使用 `req.packet<protocol>()` 访问；
 *   - 入站：准备好 `req` 的 inbound buffer 后，调用 `proto->pop(req)`；
 *   - 旁路观测、错误态与每层 typed context 属于 `req`；queue/driver 等长期状态属于各层 protocol 实例。
 */

#ifndef MC_PROTOCOL_H
#define MC_PROTOCOL_H

#include <mc/protocol/common.h>
#include <mc/protocol/instance.h>
#include <mc/protocol/packet.h>
#include <mc/protocol/trace.h>

#endif // MC_PROTOCOL_H
