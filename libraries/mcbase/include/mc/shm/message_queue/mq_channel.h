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
#include <vector>

#include <mc/future.h>
#include <mc/protocol.h>
#include <mc/shm/message_queue/mq_proto.h>
#include <mc/shm/message_queue/mq_transport_proto.h>
#include <mc/shm/message_queue/mq_queue.h>
#include <mc/shm/shm_runtime.h>
#include <mc/string.h>

namespace mc::shm {

class mq_channel {
public:
    mq_channel();
    ~mq_channel();

    void set_protocol(mc::proto::protocol* proto);
    void start(std::shared_ptr<shm_runtime> runtime, const endpoint& ep, uint32_t instance_id);
    void stop();
    mc::proto::execution_state send(mc::proto::proto_request& req);
    void                       send_owned(std::unique_ptr<mc::proto::proto_request> req);
    mc::future<mc::string>     register_pending_reply(std::uint64_t key, mc::milliseconds timeout,
                                                       mc::string timeout_payload);
    bool                       complete_pending_reply(std::uint64_t key, mc::string payload);
    void                       cancel_pending_reply(std::uint64_t key, mc::string payload);

    template <typename Protocol>
    Protocol* get_protocol() const noexcept
    {
        return protocol_instance<Protocol>();
    }

private:
    class impl;
    mc::proto::protocol* protocol_instance() const noexcept;

    template <typename Protocol>
    Protocol* protocol_instance() const noexcept;

    std::shared_ptr<impl> m_impl;
};

template <typename Protocol>
Protocol* mq_channel::protocol_instance() const noexcept
{
    auto* root = protocol_instance();
    if (root == nullptr) {
        return nullptr;
    }

    if (auto* typed = dynamic_cast<Protocol*>(root)) {
        return typed;
    }

    std::vector<mc::proto::protocol*> pending{root};
    for (std::size_t i = 0; i < pending.size(); ++i) {
        auto* current = pending[i];
        for (auto* child : current->children()) {
            if (auto* typed = dynamic_cast<Protocol*>(child)) {
                return typed;
            }
            pending.push_back(child);
        }
    }
    return nullptr;
}

} // namespace mc::shm

#endif // MC_SHM_MESSAGE_QUEUE_MQ_CHANNEL_H
