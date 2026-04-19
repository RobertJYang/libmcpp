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
#include <mc/shm/message_queue/mq_protocol.h>

#include "mq_protocol_private.h"

namespace mc::shm {

class mq_channel::impl {
public:
    mc::proto_ptr protocol;
    mq_queue      queue;
    bool          started{false};

    mq_proto*           mq_proto_layer{nullptr};
    mq_transport_proto* transport_proto_layer{nullptr};
};

mq_channel::mq_channel() : m_impl(std::make_shared<impl>())
{}

mq_channel::~mq_channel()
{
    stop();
}

mc::proto_ptr mq_channel::protocol_instance() const noexcept
{
    return m_impl == nullptr ? nullptr : m_impl->protocol;
}

void mq_channel::set_protocol(const mc::proto_ptr& proto)
{
    m_impl->protocol = proto;
    if (proto) {
        m_impl->mq_proto_layer = mc::proto::detail::instance_access::try_get_protocol<mq_proto>(*proto);
        m_impl->transport_proto_layer =
            mc::proto::detail::instance_access::try_get_protocol<mq_transport_proto>(*proto);
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
        auto validator = [runtime](std::uint32_t writer_id, std::uint64_t writer_instance_id) {
            return runtime != nullptr &&
                   runtime->writer_instance_is_current(static_cast<std::uint16_t>(writer_id), writer_instance_id);
        };
        m_impl->transport_proto_layer->configure(m_impl->queue, ep.endpoint_id, instance_id, std::move(validator));
    }
    if (m_impl->mq_proto_layer != nullptr) {
        const auto queue_max_payload = m_impl->queue.max_payload();
        const auto fragment_payload  = queue_max_payload > mq_proto::fragment_header_size
                                           ? queue_max_payload - mq_proto::fragment_header_size
                                           : mq_proto::default_max_fragment_payload;
        m_impl->mq_proto_layer->configure(instance_id, ep.endpoint_id, fragment_payload);
    }
    if (m_impl->protocol != nullptr) {
        m_impl->protocol->start();
    }

    m_impl->started = true;
}

void mq_channel::stop()
{
    if (!m_impl->started) {
        return;
    }

    if (m_impl->protocol != nullptr) {
        m_impl->protocol->stop();
    }

    m_impl->started = false;
}

} // namespace mc::shm
