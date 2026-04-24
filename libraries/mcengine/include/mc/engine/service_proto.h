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

#ifndef MC_ENGINE_SERVICE_PROTO_H
#define MC_ENGINE_SERVICE_PROTO_H

#include <mc/common.h>
#include <mc/engine/engine_codec.h>
#include <mc/engine/message.h>
#include <mc/engine/message_codec.h>
#include <mc/protocol.h>

#include <functional>

namespace mc::engine {
class MC_API service_proto : public mc::proto::protocol {
public:
    struct message_context : public mc::proto::proto_context {
        message msg;
    };

    using inbound_handler = std::function<message(message)>;

    service_proto();
    ~service_proto() override;

    void set_inbound_handler(inbound_handler handler);
    void clear_inbound_handler();
    bool has_inbound_handler() const noexcept;

    void                          set_decode_options(message_decode_options options);
    const message_decode_options& decode_options() const noexcept;

    const engine_codec& codec() const noexcept;

protected:
    mc::proto::execution_state on_push(mc::proto::proto_request& req) override;
    mc::proto::execution_state on_pop(mc::proto::proto_request& req) override;

    mc::proto::execution_state encode_message(mc::proto::proto_request& req, const message& msg);

private:
    inbound_handler m_inbound_handler;
    engine_codec    m_codec;
};

} // namespace mc::engine

#endif // MC_ENGINE_SERVICE_PROTO_H
