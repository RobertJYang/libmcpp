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
 *   - 出站：`request.prepare_packet().append_payload(...)`，再 `mc::protocol::runtime<stack>::push(request)`；
 *   - 入站：写入 io_buffer 后 `ctx.prepare_inbound()`，再 `mc::protocol::runtime<stack>::pop(...)`；
 *   - headroom/tailroom 由各层 `push_*` / `pop_*` 编译期汇总至 `stack_spec`，`prepare_*` 无额外参数。
 *   - 旁路观测：`request.set_trace(sink)`；可选 `sink.layer_mask` 门闸后再 filter/emit（见 trace.h）。
 */

#ifndef MC_PROTOCOL_H
#define MC_PROTOCOL_H

#include <mc/protocol/common.h>
#include <mc/protocol/context.h>
#include <mc/protocol/packet.h>
#include <mc/protocol/trace.h>
#include <mc/protocol/request.h>
#include <mc/protocol/runtime.h>
#include <mc/protocol/stack_spec.h>

#endif // MC_PROTOCOL_H
