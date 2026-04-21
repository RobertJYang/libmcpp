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

#include <mc/runtime.h>
#include <mc/shm/message_queue/mq_channel.h>

namespace mc::shm {

class mq_channel::impl {
public:
    mc::proto::protocol*                      protocol{nullptr};
    mq_queue                                  queue;
    bool                                      started{false};
    std::unique_ptr<mc::proto::proto_request> inbound_request;

    mq_proto*           mq_proto_layer{nullptr};
    mq_transport_proto* transport_proto_layer{nullptr};

    void pump()
    {
        if (protocol == nullptr || transport_proto_layer == nullptr) {
            return;
        }

        while (true) {
            if (!inbound_request) {
                inbound_request = std::make_unique<mc::proto::proto_request>();
            }

            if (inbound_request->state() == mc::proto::execution_state::completed ||
                inbound_request->state() == mc::proto::execution_state::failed) {
                inbound_request = std::make_unique<mc::proto::proto_request>();
            }

            auto state = transport_proto_layer->pop(*inbound_request);
            if (state == mc::proto::execution_state::suspended) {
                return;
            }
        }
    }
};

mq_channel::mq_channel() : m_impl(std::make_shared<impl>())
{}

mq_channel::~mq_channel()
{
    stop();
}

mc::proto::protocol* mq_channel::protocol_instance() const noexcept
{
    return m_impl == nullptr ? nullptr : m_impl->protocol;
}

mc::proto::execution_state mq_channel::send(mc::proto::proto_request& req)
{
    auto* proto = protocol_instance();
    if (proto == nullptr) {
        return mc::proto::execution_state::failed;
    }
    return proto->push(req);
}

void mq_channel::set_protocol(mc::proto::protocol* proto)
{
    m_impl->protocol = proto;
    if (proto) {
        m_impl->mq_proto_layer        = protocol_instance<mq_proto>();
        m_impl->transport_proto_layer = protocol_instance<mq_transport_proto>();
    } else {
        m_impl->mq_proto_layer        = nullptr;
        m_impl->transport_proto_layer = nullptr;
    }
}

void mq_channel::start(std::shared_ptr<shm_runtime> runtime, const endpoint& ep, uint32_t instance_id)
{
    if (m_impl->started) {
        return;
    }

    m_impl->queue = runtime->open_queue(ep);
    if (m_impl->transport_proto_layer != nullptr) {
        m_impl->transport_proto_layer->configure(m_impl->queue, ep.endpoint_id, instance_id);
        m_impl->transport_proto_layer->set_receive_completion_handler([impl = m_impl]() {
            if (impl) {
                impl->pump();
            }
        });
    }
    if (m_impl->mq_proto_layer != nullptr) {
        const auto queue_max_payload = m_impl->queue.max_payload();
        const auto fragment_payload  = queue_max_payload > mq_proto::fragment_header_size
                                           ? queue_max_payload - mq_proto::fragment_header_size
                                           : mq_proto::default_max_fragment_payload;
        m_impl->mq_proto_layer->set_source_liveness_probe(
            [runtime](std::uint16_t endpoint_id, std::uint64_t instance_id) {
            return runtime != nullptr && runtime->writer_instance_is_current(endpoint_id, instance_id);
        });
        m_impl->mq_proto_layer->configure(instance_id, ep.endpoint_id, fragment_payload);
    }
    if (m_impl->transport_proto_layer != nullptr) {
        m_impl->inbound_request = std::make_unique<mc::proto::proto_request>();
        m_impl->pump();
    }

    m_impl->started = true;
}

void mq_channel::stop()
{
    if (!m_impl->started) {
        return;
    }

    if (m_impl->transport_proto_layer != nullptr) {
        m_impl->transport_proto_layer->set_receive_completion_handler({});
    }

    m_impl->inbound_request.reset();
    m_impl->started = false;
}

} // namespace mc::shm
