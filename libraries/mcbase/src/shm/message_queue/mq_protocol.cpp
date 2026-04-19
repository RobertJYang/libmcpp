/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 * http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>

#include <mc/exception.h>
#include <mc/io/native_waiter.h>
#include <mc/runtime.h>
#include <mc/runtime/steady_timer.h>
#include <mc/shm/message_queue/mq_protocol.h>

#include "mq_private.h"
#include "mq_protocol_private.h"
#include "mq_watcher.h"

namespace mc::shm {
namespace {

mc::string payload_from_buffer(const mc::proto::buffer& buffer)
{
    const auto base = buffer.payload_base();
    const auto size = buffer.length() >= base ? buffer.length() - base : 0U;
    if (size == 0) {
        return {};
    }
    return mc::string(reinterpret_cast<const char*>(buffer.data() + base), size);
}

void pack_fragment_header(char* out, std::uint32_t msg_id, std::uint32_t src, std::uint16_t part)
{
    std::memcpy(out, &msg_id, sizeof(msg_id));
    std::memcpy(out + 4, &src, sizeof(src));
    std::memcpy(out + 8, &part, sizeof(part));
    out[10] = 0;
    out[11] = 0;
    out[12] = 0;
}

void unpack_fragment_header(const char* data, std::uint32_t& msg_id, std::uint32_t& src, std::uint16_t& part)
{
    std::memcpy(&msg_id, data, sizeof(msg_id));
    std::memcpy(&src, data + 4, sizeof(src));
    std::memcpy(&part, data + 8, sizeof(part));
}

std::uint32_t extract_instance_id(std::uint32_t src)
{
    return src >> 16U;
}

std::uint32_t pack_src(std::uint32_t instance_id, std::uint16_t endpoint_id)
{
    return (instance_id << 16U) | endpoint_id;
}

void post_continue(mc::proto::detail::request_core* request, std::size_t layer_index)
{
    if (request == nullptr) {
        return;
    }

    mc::get_io_executor().post([request, layer_index]() {
        (void)mc::proto::detail::continue_request(*request, layer_index);
    });
}

void post_failed(mc::proto::detail::request_core* request, std::size_t layer_index)
{
    if (request == nullptr) {
        return;
    }

    mc::get_io_executor().post([request, layer_index]() {
        (void)mc::proto::detail::fail_request(*request, layer_index);
    });
}

#ifndef _WIN32
constexpr auto space_wait_type = mc::io::native_waiter::wait_type::read;
#else
constexpr auto space_wait_type = mc::io::native_waiter::wait_type::signal;
#endif

} // namespace

void mq_proto::impl::configure(std::uint32_t instance_id, std::uint16_t endpoint_id, std::size_t max_fragment_payload)
{
    m_instance_id          = instance_id;
    m_endpoint_id          = endpoint_id;
    m_max_fragment_payload = max_fragment_payload == 0 ? mq_proto::default_max_fragment_payload : max_fragment_payload;
}

bool mq_proto::impl::ingest_fragment(mc::string_view fragment, mc::string& out_assembled)
{
    if (fragment.size() < mq_proto::fragment_header_size) {
        return false;
    }

    std::uint32_t msg_id = 0;
    std::uint32_t src    = 0;
    std::uint16_t part   = 0;
    unpack_fragment_header(fragment.data(), msg_id, src, part);

    if (extract_instance_id(src) != m_instance_id) {
        return false;
    }

    const auto            payload = fragment.substr(mq_proto::fragment_header_size);
    const mq_assembly_key key{msg_id, src};
    auto                  it = m_inflight.find(key);

    if (it == m_inflight.end()) {
        if (part == 0) {
            return false;
        }

        mq_assembly_state state;
        state.total_parts    = part;
        state.received_count = 1;
        state.received.resize(part, false);
        state.parts.resize(part);
        state.received[0] = true;
        state.parts[0].clear();
        state.parts[0].append(payload.data(), payload.size());
        it = m_inflight.emplace(key, std::move(state)).first;
    } else {
        auto& state = it->second;
        if (part == 0 || part >= state.total_parts || state.received[part]) {
            return false;
        }

        state.received[part] = true;
        state.parts[part].clear();
        state.parts[part].append(payload.data(), payload.size());
        ++state.received_count;
    }

    auto& state = it->second;
    if (state.received_count != state.total_parts) {
        return false;
    }

    mc::string assembled;
    for (const auto& current : state.parts) {
        assembled.append(current.data(), current.size());
    }

    out_assembled = std::move(assembled);
    m_inflight.erase(it);
    return true;
}

bool mq_proto::build_fragments(const mc::proto::buffer& buffer, packet& packet, mc::string& error)
{
    auto payload = payload_from_buffer(buffer);

    const std::size_t max_fragment_payload = m_impl->m_max_fragment_payload;
    if (max_fragment_payload == 0) {
        error = "fragment payload size is zero";
        return false;
    }

    const auto total_parts_raw =
        payload.empty() ? 1U : ((payload.size() + max_fragment_payload - 1U) / max_fragment_payload);
    if (total_parts_raw > std::numeric_limits<std::uint16_t>::max()) {
        error = "message too large for mq fragment header";
        return false;
    }

    const auto total_parts     = static_cast<std::uint16_t>(total_parts_raw);
    packet.payload             = std::move(payload);
    packet.total_parts         = total_parts;
    packet.msg_id              = m_impl->m_next_msg_id++;
    packet.src                 = pack_src(m_impl->m_instance_id, m_impl->m_endpoint_id);
    packet.next_fragment_index = 0;
    packet.payload_offset      = 0;
    return true;
}

bool mq_proto::build_next_fragment(packet& packet, mc::string& fragment) const
{
    const std::size_t max_fragment_payload = m_impl->m_max_fragment_payload;
    if (packet.next_fragment_index >= packet.total_parts) {
        fragment.clear();
        return false;
    }

    const auto chunk_size =
        packet.payload.empty() ? 0U : std::min(max_fragment_payload, packet.payload.size() - packet.payload_offset);
    fragment = mc::string(fragment_header_size + chunk_size, '\0');
    const auto part =
        packet.next_fragment_index == 0 ? packet.total_parts : static_cast<std::uint16_t>(packet.next_fragment_index);
    pack_fragment_header(fragment.data(), packet.msg_id, packet.src, part);
    if (chunk_size != 0) {
        std::memcpy(fragment.data() + fragment_header_size, packet.payload.data() + packet.payload_offset, chunk_size);
    }

    packet.payload_offset += chunk_size;
    ++packet.next_fragment_index;
    return true;
}

bool mq_proto::accept_fragment(mc::string_view fragment, mc::string& assembled)
{
    return m_impl->ingest_fragment(fragment, assembled);
}

void mq_transport_proto::impl::configure(mq_queue& queue, std::uint32_t writer_id, std::uint64_t writer_instance_id,
                                         mq_queue_writer_validator              validator,
                                         std::unique_ptr<mc::io::native_waiter> space_waiter,
                                         std::function<void()>                  drain_space)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_queue              = &queue;
    m_writer_id          = writer_id;
    m_writer_instance_id = writer_instance_id;
    m_validator          = std::move(validator);
    m_space_waiter       = std::move(space_waiter);
    m_drain_space        = std::move(drain_space);
    m_wait_armed         = false;
    m_stopping           = false;
}

void mq_transport_proto::impl::start_inbound(mc::proto::instance& host)
{
    auto session = mc::proto::detail::instance_access::make_session(host);
    if (session == nullptr) {
        return;
    }

    auto self = shared_from_this();
    auto pump = [self]() {
        {
            std::lock_guard<std::mutex> lock(self->m_mutex);
            if (self->m_stopping || self->m_inbound_session == nullptr) {
                return;
            }
        }

        while (true) {
            auto message = self->try_receive_message();
            if (!message.has_value()) {
                break;
            }
            std::lock_guard<std::mutex> lock(self->m_mutex);
            self->m_inbound_fragments.push_back(std::move(message->payload));
        }

        while (true) {
            mc::shared_ptr<mc::proto::detail::request_session> session;
            bool                                               has_buffered_fragments = false;
            {
                std::lock_guard<std::mutex> lock(self->m_mutex);
                if (self->m_stopping || self->m_inbound_session == nullptr) {
                    return;
                }
                session                = self->m_inbound_session;
                has_buffered_fragments = !self->m_inbound_fragments.empty();
            }
            const auto next_state = session->pop();
            if (next_state == mc::proto::execution_state::completed) {
                continue;
            }
            if (next_state == mc::proto::execution_state::suspended && has_buffered_fragments) {
                continue;
            }
            return;
        }
    };

    std::shared_ptr<detail::mq_watcher> watcher;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_stopping || m_queue == nullptr || m_inbound_watcher != nullptr) {
            return;
        }
        m_inbound_session = std::move(session);
        watcher           = std::make_shared<detail::mq_watcher>(mc::get_io_executor(), m_queue, std::move(pump));
        m_inbound_watcher = watcher;
    }

    watcher->start();
}

void mq_transport_proto::impl::arm_space_wait()
{
    auto self = shared_from_this();
    m_space_waiter->async_wait(space_wait_type, [self](const std::error_code& ec) {
        {
            std::lock_guard<std::mutex> lock(self->m_mutex);
            self->m_wait_armed = false;
            if (ec || self->m_stopping) {
                return;
            }
        }

        if (self->m_drain_space) {
            self->m_drain_space();
        }
        self->drain_pending(self->m_layer_index);
    });
}

void mq_transport_proto::impl::complete_entry(const std::shared_ptr<mq_transport_entry>& entry, std::size_t layer_index)
{
    if (entry->timer) {
        entry->timer->cancel();
    }
    post_continue(entry->request, layer_index);
}

void mq_transport_proto::impl::fail_entry(const std::shared_ptr<mq_transport_entry>& entry, std::size_t layer_index)
{
    bool removed = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (entry->completed) {
            return;
        }
        auto it = std::find(m_pending.begin(), m_pending.end(), entry);
        if (it != m_pending.end()) {
            m_pending.erase(it);
            removed = true;
        }
        entry->completed = true;
    }

    if (entry->timer) {
        entry->timer->cancel();
    }
    post_failed(entry->request, layer_index);
    if (removed) {
        drain_pending(layer_index);
    }
}

void mq_transport_proto::impl::drain_pending(std::size_t layer_index)
{
    while (true) {
        std::shared_ptr<mq_transport_entry> completed_entry;
        std::shared_ptr<mq_transport_entry> failed_entry;
        bool                                should_arm = false;

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_layer_index = layer_index;
            while (!m_pending.empty() && m_pending.front()->completed) {
                m_pending.pop_front();
            }

            if (m_stopping || m_queue == nullptr) {
                return;
            }

            if (m_pending.empty()) {
                m_wait_armed = false;
                return;
            }

            auto& entry = m_pending.front();
            if ((entry->request != nullptr && entry->request->is_cancelled()) ||
                (entry->deadline.has_value() && *entry->deadline <= mc::time_point::now())) {
                failed_entry            = entry;
                failed_entry->completed = true;
                m_pending.pop_front();
            } else if (m_queue->send_message(m_writer_id, m_writer_instance_id, entry->fragment)) {
                completed_entry            = entry;
                completed_entry->completed = true;
                m_pending.pop_front();
            } else if (!m_wait_armed && m_space_waiter != nullptr) {
                m_wait_armed = true;
                should_arm   = true;
            } else {
                return;
            }
        }

        if (failed_entry) {
            if (failed_entry->timer) {
                failed_entry->timer->cancel();
            }
            post_failed(failed_entry->request, layer_index);
            continue;
        }

        if (completed_entry) {
            if (completed_entry->timer) {
                completed_entry->timer->cancel();
            }
            post_continue(completed_entry->request, layer_index);
            continue;
        }

        if (should_arm) {
            arm_space_wait();
        }
        return;
    }
}

void mq_transport_proto::impl::submit(mc::string_view fragment, mc::proto::detail::request_core* request,
                                      std::size_t layer_index)
{
    const auto deadline = request != nullptr ? request->deadline() : std::nullopt;
    const auto now      = mc::time_point::now();
    if ((request != nullptr && request->is_cancelled()) || (deadline.has_value() && *deadline <= now)) {
        post_failed(request, layer_index);
        return;
    }

    bool fail_now     = false;
    bool complete_now = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_layer_index = layer_index;
        if (m_stopping || m_queue == nullptr || (request != nullptr && request->is_cancelled())) {
            fail_now = true;
        } else if (m_pending.empty() && !m_wait_armed &&
                   m_queue->send_message(m_writer_id, m_writer_instance_id, fragment)) {
            complete_now = true;
        }
    }

    if (fail_now) {
        post_failed(request, layer_index);
        return;
    }

    if (complete_now) {
        post_continue(request, layer_index);
        return;
    }

    auto entry      = std::make_shared<mq_transport_entry>();
    entry->fragment = mc::string(fragment.data(), fragment.size());
    entry->deadline = deadline;
    entry->request  = request;

    if (deadline.has_value()) {
        auto self    = shared_from_this();
        entry->timer = std::make_shared<mc::runtime::steady_timer>(mc::get_io_executor());
        entry->timer->expires_after(static_cast<std::chrono::milliseconds>(*deadline - now));
        entry->timer->async_wait(
            [self, weak_entry = std::weak_ptr<mq_transport_entry>(entry), layer_index](const std::error_code& ec) {
            if (ec == std::make_error_code(std::errc::operation_canceled)) {
                return;
            }
            if (auto locked = weak_entry.lock()) {
                self->fail_entry(locked, layer_index);
            }
        });
    }

    if (request != nullptr) {
        auto self = shared_from_this();
        request->add_cancel_callback([self, weak_entry = std::weak_ptr<mq_transport_entry>(entry), layer_index]() {
            if (auto locked = weak_entry.lock()) {
                self->fail_entry(locked, layer_index);
            }
        });
    }

    fail_now     = false;
    complete_now = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_layer_index = layer_index;
        if (m_stopping || m_queue == nullptr || (request != nullptr && request->is_cancelled()) ||
            (entry->deadline.has_value() && *entry->deadline <= mc::time_point::now())) {
            fail_now = true;
        } else if (m_pending.empty() && !m_wait_armed &&
                   m_queue->send_message(m_writer_id, m_writer_instance_id, entry->fragment)) {
            complete_now = true;
        } else {
            m_pending.push_back(entry);
        }
    }

    if (fail_now || complete_now) {
        entry->completed = true;
        if (entry->timer) {
            entry->timer->cancel();
        }
        if (fail_now) {
            post_failed(entry->request, layer_index);
        } else {
            post_continue(entry->request, layer_index);
        }
        return;
    }

    drain_pending(layer_index);
}

void mq_transport_proto::impl::stop(std::size_t layer_index)
{
    std::deque<std::shared_ptr<mq_transport_entry>> pending;
    std::shared_ptr<detail::mq_watcher>             inbound_watcher;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_stopping) {
            return;
        }
        m_stopping   = true;
        m_wait_armed = false;
        pending.swap(m_pending);
        inbound_watcher = std::move(m_inbound_watcher);
        m_inbound_session.reset();
    }

    if (m_space_waiter != nullptr) {
        m_space_waiter->close();
    }
    if (inbound_watcher != nullptr) {
        inbound_watcher->stop();
    }

    for (const auto& entry : pending) {
        if (entry->timer) {
            entry->timer->cancel();
        }
        post_failed(entry->request, layer_index);
    }
}

std::optional<mq_queue_message> mq_transport_proto::impl::try_receive_message() const
{
    if (m_queue == nullptr) {
        return std::nullopt;
    }

    if (m_validator) {
        return m_queue->try_receive_message(m_validator);
    }

    return m_queue->try_receive_message([](std::uint32_t, std::uint64_t) {
        return true;
    });
}

bool mq_transport_proto::impl::take_inbound_fragment(mc::string& fragment)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_inbound_fragments.empty()) {
        return false;
    }
    fragment = std::move(m_inbound_fragments.front());
    m_inbound_fragments.pop_front();
    return true;
}

mq_proto::mq_proto() : m_impl(std::make_shared<impl>())
{}

mq_proto::~mq_proto() = default;

void mq_proto::configure(std::uint32_t instance_id, std::uint16_t endpoint_id, std::size_t max_fragment_payload)
{
    m_impl->configure(instance_id, endpoint_id, max_fragment_payload);
}

mc::proto::command mq_proto::on_push(request_type& req)
{
    auto&      current = req.packet();
    mc::string error;

    if (current.total_parts == 0) {
        current.assembled.clear();
        current.payload.clear();
        current.next_fragment_index = 0;
        current.payload_offset      = 0;
        current.total_parts         = 0;
        if (!build_fragments(req.buffer(), current, error)) {
            return req.fail("mq_proto", error.c_str());
        }
    }

    mc::string fragment;
    if (!build_next_fragment(current, fragment)) {
        current.payload.clear();
        current.payload_offset      = 0;
        current.total_parts         = 0;
        current.next_fragment_index = 0;
        req.unsafe_core().resolve_async_completion();
        return req.complete();
    }

    req.prepare_buffer();
    if (!fragment.empty()) {
        req.buffer().append_payload(fragment.data(), fragment.size());
    }
    return req.push_next();
}

mc::proto::command mq_proto::on_pop(request_type& req)
{
    auto&       current = req.packet();
    const auto  base    = req.buffer().payload_base();
    const auto  size    = req.buffer().length() >= base ? req.buffer().length() - base : 0U;
    const auto* data    = reinterpret_cast<const char*>(req.buffer().data() + base);
    mc::string  assembled;
    if (!accept_fragment(mc::string_view(data, size), assembled)) {
        return req.suspend();
    }

    current.assembled = assembled;
    req.prepare_inbound_buffer();
    if (!assembled.empty()) {
        req.buffer().append_payload(assembled.data(), assembled.size());
    }
    return req.pop_next();
}

bool mq_proto::on_continue(mc::proto::detail::request_core& request) noexcept
{
    const auto state = request.is_suspended()
                           ? mc::proto::detail::resume_request(request)
                           : mc::proto::detail::drive_request(request, layer_index(), mc::proto::flow_direction::push);
    if (state == mc::proto::execution_state::failed) {
        return on_failed(request);
    }
    return true;
}

bool mq_proto::on_failed(mc::proto::detail::request_core& request) noexcept
{
    auto* current = static_cast<packet*>(request.unsafe_layer_ptr(layer_index()));
    if (current == nullptr) {
        return true;
    }

    current->payload.clear();
    current->payload_offset      = 0;
    current->total_parts         = 0;
    current->next_fragment_index = 0;
    if (!request.is_cancelled()) {
        request.fail_async_completion(std::make_exception_ptr(std::runtime_error("mq transport send failed")));
    }
    return true;
}

mq_transport_proto::mq_transport_proto() : m_impl(std::make_shared<impl>())
{}

mq_transport_proto::~mq_transport_proto() = default;

void mq_transport_proto::configure(mq_queue& queue, std::uint32_t writer_id, std::uint64_t writer_instance_id,
                                   mq_queue_writer_validator validator)
{
    auto drain_space = [&queue]() {
        if (queue.m_impl != nullptr) {
            queue.m_impl->space_notifier.drain();
        }
    };
#ifndef _WIN32
    auto space_waiter = std::make_unique<mc::io::native_waiter>(
        mc::get_io_executor(), mc::io::native_waiter::from_descriptor(queue.m_impl->space_notifier.native_handle()));
#else
    auto space_waiter = std::make_unique<mc::io::native_waiter>(
        mc::get_io_executor(), mc::io::native_waiter::from_waitable_handle(
                                   duplicate_wait_handle(queue.m_impl->space_notifier.m_impl->handle)));
#endif
    m_impl->configure(queue, writer_id, writer_instance_id, std::move(validator), std::move(space_waiter),
                      std::move(drain_space));
}

void mq_transport_proto::stop()
{
    m_impl->stop(layer_index());
}

void mq_transport_proto::on_start(mc::proto::instance& host)
{
    m_impl->start_inbound(host);
}

void mq_transport_proto::on_stop(mc::proto::instance&)
{
    stop();
}

mc::proto::command mq_transport_proto::on_push(request_type& req)
{
    const auto  base = req.buffer().payload_base();
    const auto  size = req.buffer().length() >= base ? req.buffer().length() - base : 0U;
    const auto* data = reinterpret_cast<const char*>(req.buffer().data() + base);
    if (size == 0) {
        return req.complete();
    }
    submit_fragment(mc::string_view(data, size), &req.unsafe_core());
    return req.complete();
}

mc::proto::command mq_transport_proto::on_pop(request_type& req)
{
    req.prepare_inbound_buffer();
    if (!receive_fragment(req.buffer())) {
        return req.suspend();
    }
    return req.pop_next();
}

void mq_transport_proto::submit_fragment(mc::string_view fragment, mc::proto::detail::request_core* request)
{
    m_impl->submit(fragment, request, layer_index());
}

bool mq_transport_proto::receive_fragment(mc::proto::buffer& buffer) const
{
    mc::string fragment;
    if (m_impl->take_inbound_fragment(fragment)) {
        if (!fragment.empty()) {
            buffer.append_payload(fragment.data(), fragment.size());
        }
        return true;
    }

    auto message = m_impl->try_receive_message();
    if (!message.has_value()) {
        return false;
    }

    if (!message->payload.empty()) {
        buffer.append_payload(message->payload.data(), message->payload.size());
    }
    return true;
}

} // namespace mc::shm
