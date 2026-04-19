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

#ifndef MC_SHM_MESSAGE_QUEUE_MQ_CHANNEL_H
#define MC_SHM_MESSAGE_QUEUE_MQ_CHANNEL_H

#include <cstdint>
#include <memory>

#include <mc/protocol.h>
#include <mc/shm/message_queue/mq_queue.h>
#include <mc/shm/shm_runtime.h>
#include <mc/string.h>

namespace mc::shm {

class mq_proto;
class mq_transport_proto;

class mq_channel {
public:
    mq_channel();
    ~mq_channel();

    void set_protocol(const mc::proto_ptr& proto);
    void start(std::shared_ptr<shm_runtime> runtime, const endpoint& ep, uint32_t instance_id);
    void stop();

    template <typename Protocol>
    Protocol* get_protocol() const noexcept
    {
        auto proto = protocol_instance();
        if (proto == nullptr) {
            return nullptr;
        }
        return mc::proto::detail::instance_access::try_get_protocol<Protocol>(*proto);
    }

    template <typename... Protocols>
    mc::proto::execution_state send(mc::proto::request_for_t<Protocols...>& req)
    {
        auto proto = protocol_instance();
        if (proto == nullptr) {
            return mc::proto::execution_state::failed;
        }
        return proto->push(req);
    }

private:
    class impl;
    mc::proto_ptr         protocol_instance() const noexcept;
    std::shared_ptr<impl> m_impl;
};

} // namespace mc::shm

#endif // MC_SHM_MESSAGE_QUEUE_MQ_CHANNEL_H
