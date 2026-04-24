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

#ifndef MC_APP_DBUS_PROTO_H
#define MC_APP_DBUS_PROTO_H

#include <mc/dbus/connection.h>
#include <mc/protocol.h>
#include <mc/string.h>

#include <memory>

namespace mc::app {

// dbus_proto —— per-service 的 DBus 协议节点。作为 mc::proto::protocol 子节点
// 挂载在 app_proto 下；业务层通过 service::set_proto(app_proto) 将整条出站
// 链路接入。app_proto 根据 message header context 中的来源标记做路由：
// dbus 来源的消息 push 到 dbus_proto，由 dbus_proto 自行将 mcengine message
// 翻译为 DBus 报文。dbus_proto 的 on_push 目前提供编译骨架，真正的 DBus 报文
// 翻译在 C8 测试完善后迭代补齐。
class MC_API dbus_proto : public mc::proto::protocol {
public:
    dbus_proto(mc::string service_name, mc::dbus::connection conn);
    ~dbus_proto() override;

    dbus_proto(const dbus_proto&)            = delete;
    dbus_proto& operator=(const dbus_proto&) = delete;
    dbus_proto(dbus_proto&&)                 = delete;
    dbus_proto& operator=(dbus_proto&&)      = delete;

    mc::string_view       service_name() const noexcept;
    mc::dbus::connection& connection() noexcept;

    // 出站计数器仅用于单元测试可观测性；真实 DBus 投递在后续迭代补齐。
    std::size_t outbound_count() const noexcept;

protected:
    mc::proto::execution_state on_push(mc::proto::proto_request& req) override;
    mc::proto::execution_state on_pop(mc::proto::proto_request& req) override;

private:
    mc::string           m_service_name;
    mc::dbus::connection m_connection;
    std::size_t          m_outbound_count{0};
};

using dbus_proto_ptr = std::shared_ptr<dbus_proto>;

} // namespace mc::app

#endif // MC_APP_DBUS_PROTO_H
