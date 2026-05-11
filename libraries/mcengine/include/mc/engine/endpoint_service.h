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

#ifndef MC_ENGINE_ENDPOINT_SERVICE_H
#define MC_ENGINE_ENDPOINT_SERVICE_H

#include <mc/common.h>
#include <mc/engine/match/table.h>
#include <mc/engine/service.h>
#include <mc/future.h>
#include <mc/string.h>
#include <mc/string_view.h>

#include <cstdint>
#include <memory>

namespace mc::shm {
class shm_runtime;
class mq_channel;
} // namespace mc::shm

// endpoint_service
// ---------------------------------------------------------------------------
// 进程内拓扑（SHM ON 模式）：
//
//   ┌─────────────────────────────────────────────────────────────────┐
//   │ process                                                         │
//   │                                                                 │
//   │  ┌──────────────────┐   ┌────────────┐   ┌────────────┐         │
//   │  │ endpoint_service │   │ service_A  │   │ service_B  │ ...     │
//   │  │  (harbor, 唯一)  │   │ (business) │    │ (business) │         │
//   │  └────────┬─────────┘   └─────┬──────┘   └─────┬──────┘          │
//   │           │ owns唯一           │                │                 │
//   │           ▼                   │                │                 │
//   │   ┌──────────────┐            │                │                 │
//   │   │  mq_channel  │◀───── app_proto 把 mq 通道复用给 ─────┐         │
//   │   │ (process-wide│                                        │      │
//   │   │  in/out queue│                                        │      │
//   │   └──────┬───────┘                                        │      │
//   │          │ inbound msg                                    │      │
//   │          ▼                                                │      │
//   │   engine::route_inbound(msg)                              │      │
//   │          │                                                │      │
//   │          ├─ 解 mc.match.ids.{services,ids}                │      │
//   │          ├─ 按 service_name 分组                          │      │
//   │          └─▶ 每个 service::dispatch_event(msg)            │      │
//   │                                                           │      │
//   │   outbound（service::emit）                              │      │
//   │     ├─ match::table::find_targets                         │      │
//   │     ├─ 按 (endpoint_id, instance_id) 分组合并             │      │
//   │     ├─ 本地组 → engine::route_inbound                     │      │
//   │     └─ 远端组 → service::get_proto()→ app_proto 下挂      │      │
//   │                 mq_proto→ endpoint_service.mq_channel     │      │
//   └─────────────────────────────────────────────────────────────────┘
//
// 角色：
//   - SHM ON 模式下，每个 mcengine 进程有且仅有一个 endpoint_service；
//   - 它是本进程在 shm_runtime 里的 endpoint 注册点（endpoint_id + instance_id
//     + queue_name 都挂在这个 service 身上）；
//   - 它独占该进程**唯一**的 mq_channel；其他 business service 不再各自开
//     mq_channel，而是通过 app 层的 app_proto（mcapp）共享这条通道；
//   - 它作为进程级元数据的挂载点 —— 后续可以在这个 service 上挂 object 暴露
//     "本进程启动时间 / 版本 / service 列表 / 心跳状态" 等管理对象。
//
// 生命周期与 service 基类一致（init → start → stop → cleanup），具体流程：
//   on_start():
//     1. 名字冲突检测：如果 shm_runtime 中已经有同名 endpoint 且属于活进程
//        （state ∈ {starting, running} + owner_pid alive），直接拒绝启动，
//        明确报错 —— 让 app 自己处理多实例冲突，绝不挤掉活实例；
//     2. shm_runtime.register_endpoint(endpoint_name) → 拿到 endpoint_id +
//        instance_id；若存在同名 aborted slot，自动 takeover（崩溃恢复路径）；
//     3. shm_runtime.mark_endpoint_running(endpoint);
//     4. 打开唯一的 mq_channel（队列名来源：shm_runtime 为此 endpoint
//        推导的 queue_name，"mc_shm_queue_<endpoint_name>"）；
//     5. 绑定 mq inbound → engine::route_inbound(msg)，实现进程级入口合并。
//
//   on_stop():
//     1. 关 mq_channel；
//     2. shm_runtime.abort_endpoint(endpoint)（标记状态，不释放 slot；同名
//        进程重启时自动接管）。
//
// 非 SHM 模式：endpoint_service 不会被创建，engine 回落到 local_table +
// 原有 service::dispatch_event 路径；整个类在 MCENGINE_USE_SHM=false 下不参
// 与链接（由 meson 条件编译控制）。

namespace mc::engine {

struct message;

// endpoint 名字冲突策略。
enum class endpoint_name_conflict_policy {
    // 活进程冲突：直接拒启，on_start 返回 false。
    refuse,
    // 活进程冲突：把 "${endpoint_name}-${pid}" 作为最终名字再试一次。
    append_pid,
};

class MC_API endpoint_service : public service {
public:
    // endpoint_name 为本进程在 SHM 中的唯一名字。一旦绑定，崩溃重启同名进
    // 程可接管原 slot 与原 mq 段（订阅者不需要改 endpoint_id 引用）。
    // 空串 / 非法字符应由调用方在构造前校验好；本 service 在 on_start 失败时
    // 返回 false，不抛异常。
    endpoint_service(mc::string_view endpoint_name, std::shared_ptr<mc::shm::shm_runtime> runtime,
                     mc::object* parent = nullptr);

    explicit endpoint_service(mc::string_view endpoint_name, mc::object* parent = nullptr);
    ~endpoint_service() override;

    std::uint16_t endpoint_id() const noexcept;
    std::uint32_t instance_id() const noexcept;

    mc::string_view queue_name() const noexcept;

    std::shared_ptr<mc::shm::shm_runtime> get_runtime() const noexcept;
    mc::shm::mq_channel*                  get_mq_channel() const noexcept;
    bool                                  send_to_endpoint(std::uint16_t endpoint_id, std::uint32_t instance_id,
                                                           const message& msg);
    mc::future<message>                   send_with_reply_to_endpoint(std::uint16_t endpoint_id,
                                                                      std::uint32_t instance_id, message msg,
                                                                      mc::milliseconds timeout);

    match::table_ptr create_match_table() const;
    void             push_outbound(const message& msg);
    void             set_name_conflict_policy(endpoint_name_conflict_policy policy) noexcept;
    mc::string_view  effective_endpoint_name() const noexcept;

protected:
    bool on_start() override;
    bool on_stop() override;

private:
    struct endpoint_service_impl;
    void handle_inbound_message(message msg);
    bool complete_pending_reply(const message& msg);
    std::unique_ptr<endpoint_service_impl> m_endpoint_impl;
};

} // namespace mc::engine

#endif // MC_ENGINE_ENDPOINT_SERVICE_H
