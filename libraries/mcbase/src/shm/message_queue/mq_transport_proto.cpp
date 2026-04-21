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

#include <mc/shm/message_queue/mq_transport_proto.h>

#include <mc/runtime.h>
#include <mc/shm/message_queue/mq_queue.h>

#include "mq_watcher.h"

#include <algorithm>
#include <deque>
#include <mutex>
#include <utility>

namespace mc::shm {

struct mq_transport_proto::send_runtime : public std::enable_shared_from_this<mq_transport_proto::send_runtime> {
    mq_transport_proto*                   owner{nullptr};
    mq_queue*                             queue{nullptr};
    std::shared_ptr<detail::mq_watcher>   space_watcher;
    std::deque<mc::proto::proto_request*> pending_requests;
    bool                                  processing{false};
    bool                                  kick_scheduled{false};
    bool                                  waiting_for_space{false};
    std::mutex                            mutex;

    ~send_runtime()
    {
        if (space_watcher != nullptr) {
            space_watcher->stop();
            space_watcher.reset();
        }
    }

    void rebind(mq_transport_proto* proto, mq_queue& bound_queue)
    {
        owner = proto;
        queue = &bound_queue;
        if (space_watcher != nullptr) {
            space_watcher->stop();
            space_watcher.reset();
        }

        auto weak_self = weak_from_this();
        space_watcher =
            std::make_shared<detail::mq_watcher>(mc::runtime::get_default_executor(), &bound_queue, [weak_self]() {
            if (auto self = weak_self.lock()) {
                self->process_pending();
            }
        }, detail::mq_watcher::source::space);
        space_watcher->start();
    }

    void enqueue(mc::proto::proto_request& req)
    {
        bool should_schedule_kick = false;
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (std::find(pending_requests.begin(), pending_requests.end(), &req) == pending_requests.end()) {
                pending_requests.push_back(&req);
            }
            if (!processing && !kick_scheduled && !waiting_for_space) {
                kick_scheduled       = true;
                should_schedule_kick = true;
            }
        }

        if (!should_schedule_kick) {
            return;
        }

        auto weak_self = weak_from_this();
        mc::runtime::post([weak_self]() {
            if (auto self = weak_self.lock()) {
                self->process_pending();
            }
        });
    }

    void process_pending()
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (processing || owner == nullptr) {
                return;
            }
            processing        = true;
            kick_scheduled    = false;
            waiting_for_space = false;
        }

        while (true) {
            mc::proto::proto_request* req = nullptr;
            {
                std::lock_guard<std::mutex> lock(mutex);
                while (!pending_requests.empty() &&
                       pending_requests.front()->state() != mc::proto::execution_state::suspended) {
                    pending_requests.pop_front();
                }
                if (pending_requests.empty()) {
                    processing = false;
                    return;
                }
                req = pending_requests.front();
            }

            const auto state = owner->resume(*req);
            if (state == mc::proto::execution_state::suspended) {
                std::lock_guard<std::mutex> lock(mutex);
                processing        = false;
                waiting_for_space = true;
                return;
            }

            {
                std::lock_guard<std::mutex> lock(mutex);
                if (!pending_requests.empty() && pending_requests.front() == req) {
                    pending_requests.pop_front();
                } else {
                    pending_requests.erase(std::remove(pending_requests.begin(), pending_requests.end(), req),
                                           pending_requests.end());
                }
                if (pending_requests.empty()) {
                    waiting_for_space = false;
                }
            }
        }
    }
};

struct mq_transport_proto::receive_runtime : public std::enable_shared_from_this<mq_transport_proto::receive_runtime> {
    mq_transport_proto*                             owner{nullptr};
    mq_queue*                                       queue{nullptr};
    std::shared_ptr<detail::mq_watcher>             data_watcher;
    std::deque<mc::proto::proto_request*>           pending_requests;
    std::shared_ptr<mc::small_function<void(), 64>> completion_handler;
    bool                                            processing{false};
    bool                                            kick_scheduled{false};
    bool                                            waiting_for_data{false};
    std::mutex                                      mutex;

    ~receive_runtime()
    {
        if (data_watcher != nullptr) {
            data_watcher->stop();
            data_watcher.reset();
        }
    }

    void rebind(mq_transport_proto* proto, mq_queue& bound_queue)
    {
        owner = proto;
        queue = &bound_queue;
        std::shared_ptr<detail::mq_watcher> old_watcher;
        {
            std::lock_guard<std::mutex> lock(mutex);
            old_watcher = std::move(data_watcher);
        }
        if (old_watcher != nullptr) {
            old_watcher->stop();
        }
    }

    void set_completion_handler(mc::small_function<void(), 64> handler)
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (handler) {
            completion_handler = std::make_shared<mc::small_function<void(), 64>>(std::move(handler));
            return;
        }
        completion_handler.reset();
    }

    void enqueue(mc::proto::proto_request& req)
    {
        bool should_schedule_kick = false;
        bool should_start_watcher = false;
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (std::find(pending_requests.begin(), pending_requests.end(), &req) == pending_requests.end()) {
                pending_requests.push_back(&req);
            }
            should_start_watcher = data_watcher == nullptr;
            if (!processing && !kick_scheduled && !waiting_for_data) {
                kick_scheduled       = true;
                should_schedule_kick = true;
            }
        }

        if (should_start_watcher) {
            ensure_watcher_started();
        }

        if (!should_schedule_kick) {
            return;
        }

        auto weak_self = weak_from_this();
        mc::runtime::post([weak_self]() {
            if (auto self = weak_self.lock()) {
                self->process_pending();
            }
        });
    }

    void process_pending()
    {
        std::shared_ptr<detail::mq_watcher> watcher_to_stop;
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (processing || owner == nullptr) {
                return;
            }
            processing       = true;
            kick_scheduled   = false;
            waiting_for_data = false;
        }

        while (true) {
            mc::proto::proto_request* req = nullptr;
            {
                std::lock_guard<std::mutex> lock(mutex);
                while (!pending_requests.empty() &&
                       pending_requests.front()->state() != mc::proto::execution_state::suspended) {
                    pending_requests.pop_front();
                }
                if (pending_requests.empty()) {
                    processing      = false;
                    watcher_to_stop = std::move(data_watcher);
                    break;
                }
                req = pending_requests.front();
            }

            if (req == nullptr) {
                break;
            }

            const auto state = owner->resume(*req);
            if (state == mc::proto::execution_state::suspended) {
                std::lock_guard<std::mutex> lock(mutex);
                processing       = false;
                waiting_for_data = true;
                return;
            }

            std::shared_ptr<mc::small_function<void(), 64>> completion;
            {
                std::lock_guard<std::mutex> lock(mutex);
                if (!pending_requests.empty() && pending_requests.front() == req) {
                    pending_requests.pop_front();
                } else {
                    pending_requests.erase(std::remove(pending_requests.begin(), pending_requests.end(), req),
                                           pending_requests.end());
                }
                if (pending_requests.empty()) {
                    waiting_for_data = false;
                }
                completion = completion_handler;
            }

            if (completion != nullptr && *completion) {
                (*completion)();
            }
        }

        if (watcher_to_stop != nullptr) {
            watcher_to_stop->stop();
        }
    }

    void ensure_watcher_started()
    {
        std::shared_ptr<detail::mq_watcher> watcher;
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (data_watcher != nullptr || queue == nullptr) {
                return;
            }
            auto weak_self = weak_from_this();
            watcher = std::make_shared<detail::mq_watcher>(mc::runtime::get_default_executor(), queue, [weak_self]() {
                if (auto self = weak_self.lock()) {
                    self->process_pending();
                }
            }, detail::mq_watcher::source::data);
            data_watcher = watcher;
        }
        watcher->start();
    }
};

void mq_transport_proto::configure(mq_queue& queue, std::uint32_t writer_id, std::uint64_t writer_instance_id)
{
    m_queue              = &queue;
    m_writer_id          = writer_id;
    m_writer_instance_id = writer_instance_id;
    if (m_send_runtime == nullptr) {
        m_send_runtime = std::make_shared<send_runtime>();
    }
    m_send_runtime->rebind(this, queue);
    if (m_receive_runtime == nullptr) {
        m_receive_runtime = std::make_shared<receive_runtime>();
    }
    m_receive_runtime->rebind(this, queue);
}

void mq_transport_proto::set_receive_completion_handler(mc::small_function<void(), 64> handler)
{
    if (m_receive_runtime == nullptr) {
        m_receive_runtime = std::make_shared<receive_runtime>();
    }
    m_receive_runtime->set_completion_handler(std::move(handler));
}

mc::proto::execution_state mq_transport_proto::on_push(mc::proto::proto_request& req)
{
    if (!_is_configured()) {
        return fail(req, "mq_transport_not_configured", "mq transport 未配置队列");
    }

    const auto payload = _payload_from_buffer(req.buffer());
    if (payload.empty()) {
        return complete(req);
    }

    if (payload.size() > m_queue->max_payload()) {
        return fail(req, "mq_transport_payload_too_large", "mq payload 超过队列容量");
    }

    if (!_submit_payload(payload)) {
        const auto state = suspend(req);
        _queue_pending_request(req);
        return state;
    }
    return complete(req);
}

mc::proto::execution_state mq_transport_proto::on_pop(mc::proto::proto_request& req)
{
    if (!_is_configured()) {
        return fail(req, "mq_transport_not_configured", "mq transport 未配置队列");
    }

    mc::string payload;
    if (!_try_receive_payload(payload)) {
        const auto state = suspend(req);
        if (m_receive_runtime != nullptr) {
            m_receive_runtime->enqueue(req);
        }
        return state;
    }

    _replace_payload(req.buffer(), payload);
    return pop_next(req);
}

bool mq_transport_proto::_is_configured() const noexcept
{
    return m_queue != nullptr;
}

mc::string mq_transport_proto::_payload_from_buffer(const mc::proto::buffer& buffer) const
{
    const auto base = buffer.payload_base();
    const auto size = buffer.length() >= base ? buffer.length() - base : 0U;
    if (size == 0) {
        return {};
    }

    return mc::string(reinterpret_cast<const char*>(buffer.data() + base), size);
}

bool mq_transport_proto::_submit_payload(mc::string_view payload) const
{
    return m_queue->send_message(m_writer_id, m_writer_instance_id, payload);
}

bool mq_transport_proto::_try_receive_payload(mc::string& payload) const
{
    auto message = m_queue->try_receive_message();
    if (!message.has_value()) {
        return false;
    }

    payload = message->payload;
    return true;
}

void mq_transport_proto::_replace_payload(mc::proto::buffer& buffer, mc::string_view payload) const
{
    buffer.clear();
    if (!payload.empty()) {
        buffer.append_payload(payload.data(), payload.size());
    }
}

void mq_transport_proto::_queue_pending_request(mc::proto::proto_request& req)
{
    if (m_send_runtime != nullptr) {
        m_send_runtime->enqueue(req);
    }
}

} // namespace mc::shm
