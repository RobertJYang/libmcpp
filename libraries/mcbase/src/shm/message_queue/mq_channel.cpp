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

#include <algorithm>
#include <atomic>
#include <future>
#include <mutex>
#include <unordered_map>
#include <vector>

#include <mc/future.h>
#include <mc/runtime.h>
#include <mc/shm/message_queue/mq_channel.h>

namespace mc::shm {

class mq_channel::impl {
public:
    mc::proto::protocol*                                   protocol{nullptr};
    mq_queue                                               queue;
    mc::runtime::any_executor                              receive_lane;
    mc::runtime::any_executor                              send_lane;
    std::atomic<bool>                                      started{false};
    std::atomic<std::uint64_t>                             receive_epoch{0};
    std::atomic<mq_transport_proto*>                       transport_proto_layer{nullptr};
    std::unique_ptr<mc::proto::proto_request>              inbound_request;
    std::mutex                                             owned_send_mutex;
    std::vector<std::unique_ptr<mc::proto::proto_request>> owned_send_requests;
    std::mutex                                             pending_reply_mutex;
    std::unordered_map<std::uint64_t, mc::promise<mc::string>> pending_replies;

    mq_proto* mq_proto_layer{nullptr};

    static void drain_executor(const std::shared_ptr<impl>& self, const mc::runtime::any_executor& executor)
    {
        if (!executor.valid()) {
            return;
        }

        std::promise<void> drained;
        auto               done = drained.get_future();
        executor.post([self, promise = std::move(drained)]() mutable {
            MC_UNUSED(self);
            promise.set_value();
        });
        done.wait();
    }

    void schedule_pump(const std::shared_ptr<impl>& self, std::uint64_t epoch)
    {
        if (!receive_lane.valid()) {
            return;
        }

        receive_lane.post([self, epoch]() {
            if (!self || !self->started.load(std::memory_order_acquire) ||
                self->receive_epoch.load(std::memory_order_acquire) != epoch) {
                return;
            }
            self->pump();
        });
    }

    void drain_lane(const std::shared_ptr<impl>& self)
    {
        drain_executor(self, receive_lane);
    }

    void cleanup_owned_sends()
    {
        std::lock_guard lock(owned_send_mutex);
        owned_send_requests.erase(std::remove_if(owned_send_requests.begin(), owned_send_requests.end(),
                                                 [](const auto& req) {
            return req == nullptr || req->state() == mc::proto::execution_state::completed ||
                   req->state() == mc::proto::execution_state::failed;
        }),
                                  owned_send_requests.end());
    }

    void pump()
    {
        if (!started.load(std::memory_order_acquire)) {
            return;
        }
        if (protocol == nullptr) {
            return;
        }
        auto* transport = transport_proto_layer.load(std::memory_order_acquire);
        if (transport == nullptr) {
            return;
        }

        while (true) {
            if (!started.load(std::memory_order_acquire) ||
                transport_proto_layer.load(std::memory_order_acquire) != transport) {
                return;
            }

            if (!inbound_request) {
                inbound_request = std::make_unique<mc::proto::proto_request>();
            }

            if (inbound_request->state() == mc::proto::execution_state::completed ||
                inbound_request->state() == mc::proto::execution_state::failed) {
                inbound_request = std::make_unique<mc::proto::proto_request>();
            }

            auto state = transport->pop(*inbound_request);
            if (state == mc::proto::execution_state::suspended) {
                return;
            }
            if (state == mc::proto::execution_state::failed &&
                (!started.load(std::memory_order_acquire) ||
                 transport_proto_layer.load(std::memory_order_acquire) != transport)) {
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

void mq_channel::send_owned(std::unique_ptr<mc::proto::proto_request> req)
{
    if (req == nullptr || m_impl == nullptr) {
        return;
    }

    auto* raw = req.get();
    {
        std::lock_guard lock(m_impl->owned_send_mutex);
        m_impl->owned_send_requests.erase(std::remove_if(m_impl->owned_send_requests.begin(),
                                                         m_impl->owned_send_requests.end(),
                                                         [](const auto& current) {
            return current == nullptr || current->state() == mc::proto::execution_state::completed ||
                   current->state() == mc::proto::execution_state::failed;
        }),
                                          m_impl->owned_send_requests.end());
        m_impl->owned_send_requests.push_back(std::move(req));
    }

    if (!m_impl->send_lane.valid()) {
        return;
    }

    auto self = m_impl;
    m_impl->send_lane.post([self, raw]() {
        if (!self || !self->started.load(std::memory_order_acquire) || raw == nullptr || self->protocol == nullptr) {
            return;
        }

        const auto state = self->protocol->push(*raw);
        if (state != mc::proto::execution_state::suspended) {
            self->cleanup_owned_sends();
        }
    });
}

mc::future<mc::string> mq_channel::register_pending_reply(std::uint64_t key, mc::milliseconds timeout,
                                                          mc::string timeout_payload)
{
    auto promise = mc::make_promise<mc::string>(mc::runtime::get_io_executor());
    auto future  = promise.get_future();
    {
        std::lock_guard lock(m_impl->pending_reply_mutex);
        m_impl->pending_replies.emplace(key, std::move(promise));
    }

    auto self = m_impl;
    mc::delay(timeout, mc::runtime::get_io_executor()).then([self, key, timeout_payload = std::move(timeout_payload)]() mutable {
        if (!self) {
            return;
        }
        mc::promise<mc::string> pending;
        {
            std::lock_guard lock(self->pending_reply_mutex);
            auto            it = self->pending_replies.find(key);
            if (it == self->pending_replies.end()) {
                return;
            }
            pending = std::move(it->second);
            self->pending_replies.erase(it);
        }
        pending.set_value(std::move(timeout_payload));
    });

    return future;
}

bool mq_channel::complete_pending_reply(std::uint64_t key, mc::string payload)
{
    mc::promise<mc::string> pending;
    {
        std::lock_guard lock(m_impl->pending_reply_mutex);
        auto            it = m_impl->pending_replies.find(key);
        if (it == m_impl->pending_replies.end()) {
            return false;
        }
        pending = std::move(it->second);
        m_impl->pending_replies.erase(it);
    }
    pending.set_value(std::move(payload));
    return true;
}

void mq_channel::cancel_pending_reply(std::uint64_t key, mc::string payload)
{
    (void)complete_pending_reply(key, std::move(payload));
}

void mq_channel::set_protocol(mc::proto::protocol* proto)
{
    m_impl->protocol = proto;
    if (proto) {
        m_impl->mq_proto_layer = protocol_instance<mq_proto>();
        m_impl->transport_proto_layer.store(protocol_instance<mq_transport_proto>(), std::memory_order_release);
    } else {
        m_impl->mq_proto_layer = nullptr;
        m_impl->transport_proto_layer.store(nullptr, std::memory_order_release);
    }
}

void mq_channel::start(std::shared_ptr<shm_runtime> runtime, const endpoint& ep, uint32_t instance_id)
{
    if (m_impl->started) {
        return;
    }

    m_impl->receive_lane = mc::runtime::make_io_strand();
    m_impl->send_lane    = mc::runtime::make_io_strand();
    const auto epoch     = m_impl->receive_epoch.fetch_add(1, std::memory_order_acq_rel) + 1;
    m_impl->started.store(true, std::memory_order_release);

    if (m_impl->protocol != nullptr &&
        (m_impl->mq_proto_layer == nullptr || m_impl->transport_proto_layer.load() == nullptr)) {
        m_impl->mq_proto_layer = protocol_instance<mq_proto>();
        m_impl->transport_proto_layer.store(protocol_instance<mq_transport_proto>(), std::memory_order_release);
    }

    m_impl->queue = runtime->open_queue(ep);
    m_impl->queue.set_writer_liveness_probe([runtime](std::uint32_t endpoint_id, std::uint64_t instance_id) {
        return runtime != nullptr &&
               runtime->writer_instance_is_current(static_cast<std::uint16_t>(endpoint_id), instance_id);
    });
    auto* transport = m_impl->transport_proto_layer.load(std::memory_order_acquire);
    if (transport != nullptr) {
        transport->configure(m_impl->queue, ep.endpoint_id, instance_id);
        transport->set_receive_completion_handler([impl = m_impl, epoch]() {
            if (!impl || !impl->started.load(std::memory_order_acquire) ||
                impl->receive_epoch.load(std::memory_order_acquire) != epoch) {
                return;
            }
            impl->schedule_pump(impl, epoch);
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
    if (transport != nullptr) {
        m_impl->inbound_request = std::make_unique<mc::proto::proto_request>();
        m_impl->schedule_pump(m_impl, epoch);
    }
}

void mq_channel::stop()
{
    if (!m_impl->started) {
        return;
    }

    m_impl->started.store(false, std::memory_order_release);
    m_impl->receive_epoch.fetch_add(1, std::memory_order_acq_rel);
    impl::drain_executor(m_impl, m_impl->send_lane);
    {
        std::lock_guard lock(m_impl->pending_reply_mutex);
        m_impl->pending_replies.clear();
    }

    auto* transport = m_impl->transport_proto_layer.exchange(nullptr, std::memory_order_acq_rel);
    if (transport != nullptr) {
        transport->set_receive_completion_handler({});
        transport->shutdown();
    }

    m_impl->drain_lane(m_impl);

    m_impl->inbound_request.reset();
    m_impl->mq_proto_layer = nullptr;
    {
        std::lock_guard lock(m_impl->owned_send_mutex);
        m_impl->owned_send_requests.clear();
    }
}

} // namespace mc::shm
