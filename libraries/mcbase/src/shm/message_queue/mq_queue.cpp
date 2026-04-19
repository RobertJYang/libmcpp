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

#include <mc/shm/message_queue/mq_queue.h>

#include "mq_private.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <new>
#include <thread>

#include <mc/shm/shared_mapping_view.h>

namespace mc::shm {
namespace {

constexpr std::uint32_t queue_magic    = 0x4d43514d;
constexpr std::uint32_t queue_version  = 1;
constexpr std::size_t   max_slot_count = 4096;

std::size_t queue_mapping_size(std::size_t slot_count, std::size_t max_payload) noexcept
{
    return sizeof(detail::queue_header) + slot_count * detail::slot_size(max_payload);
}

} // namespace

mq_queue::mq_queue() noexcept = default;

mq_queue::mq_queue(const mq_queue_options& options) : m_impl(std::make_shared<impl>())
{
    m_impl->options = options;
    if (m_impl->options.shared_memory_name.empty()) {
        m_impl.reset();
        return;
    }
    if (m_impl->options.max_payload_size == 0) {
        m_impl->options.max_payload_size = 256;
    }
    const auto requested_payload = m_impl->options.max_payload_size;

    m_impl->data_notifier =
        detail::mq_notifier(detail::mq_notifier::make_default_name(m_impl->options.shared_memory_name));
    m_impl->space_notifier =
        detail::mq_notifier(detail::mq_notifier::make_space_name(m_impl->options.shared_memory_name));
    if (!m_impl->data_notifier.is_valid() || !m_impl->space_notifier.is_valid()) {
        m_impl.reset();
        return;
    }

    const auto slot_count = std::clamp<std::size_t>(options.slot_count, 1, max_slot_count);
    m_impl->mapping       = shared_mapping_view(options.shared_memory_name,
                                                queue_mapping_size(slot_count, requested_payload),
                                                detail::open_mode::create_or_open);
    if (!m_impl->mapping.is_valid()) {
        m_impl.reset();
        return;
    }

    m_impl->header     = static_cast<detail::queue_header*>(m_impl->mapping.data());
    m_impl->slots_base = static_cast<char*>(m_impl->mapping.data()) + sizeof(detail::queue_header);

    if (m_impl->mapping.created()) {
        new (m_impl->header) detail::queue_header{};
        m_impl->header->magic.store(queue_magic, std::memory_order_release);
        m_impl->header->version.store(queue_version, std::memory_order_release);
        m_impl->header->slot_count       = static_cast<std::uint32_t>(slot_count);
        m_impl->header->max_payload_size = static_cast<std::uint32_t>(requested_payload);
        m_impl->slot_stride              = detail::slot_size(requested_payload);
        m_impl->max_payload              = requested_payload;
        for (std::size_t i = 0; i < slot_count; ++i) {
            auto& slot = m_impl->slot_at(i);
            std::memset(&slot, 0, m_impl->slot_stride);
            slot.sequence.store(i, std::memory_order_release);
        }
    } else if (m_impl->header->magic.load(std::memory_order_acquire) != queue_magic ||
               m_impl->header->version.load(std::memory_order_acquire) != queue_version) {
        m_impl.reset();
    } else {
        m_impl->slot_stride = detail::slot_size(m_impl->header->max_payload_size);
        m_impl->max_payload = m_impl->header->max_payload_size;
    }
}

mq_queue::~mq_queue() = default;

mq_queue::mq_queue(mq_queue&& other) noexcept = default;

mq_queue& mq_queue::operator=(mq_queue&& other) noexcept
{
    m_impl = std::move(other.m_impl);
    return *this;
}

bool mq_queue::is_valid() const noexcept
{
    return m_impl != nullptr;
}

std::size_t mq_queue::slot_count() const noexcept
{
    return is_valid() ? m_impl->header->slot_count : 0;
}

std::size_t mq_queue::max_payload() const noexcept
{
    return is_valid() ? m_impl->max_payload : 0;
}

mc::string_view mq_queue::shared_memory_name() const noexcept
{
    return is_valid() ? mc::string_view(m_impl->options.shared_memory_name) : mc::string_view{};
}

bool mq_queue::send_message(std::uint32_t writer_id, std::uint64_t writer_instance_id, mc::string_view payload,
                            std::uint8_t priority)
{
    if (!is_valid() || payload.size() > m_impl->max_payload) {
        return false;
    }

    std::uint64_t start_seq = 0;
    while (true) {
        const auto head = m_impl->header->head_seq.load(std::memory_order_acquire);
        auto       tail = m_impl->header->tail_reserve_seq.load(std::memory_order_acquire);
        if (tail + 1 > head + m_impl->header->slot_count) {
            return false;
        }

        if (m_impl->header->tail_reserve_seq.compare_exchange_weak(tail, tail + 1, std::memory_order_acq_rel,
                                                                   std::memory_order_acquire)) {
            start_seq = tail;
            break;
        }
    }

    const auto message_id   = m_impl->header->next_message_id.fetch_add(1, std::memory_order_acq_rel);
    auto&      slot         = m_impl->slot_at(start_seq);
    slot.writer_id          = writer_id;
    slot.writer_instance_id = writer_instance_id;
    slot.message_id         = message_id;
    slot.fragment_index     = 0;
    slot.fragment_count     = 1;
    slot.priority           = priority;
    if (!payload.empty()) {
        std::memcpy(slot.payload, payload.data(), payload.size());
    }
    if (payload.size() < m_impl->max_payload) {
        std::memset(slot.payload + payload.size(), 0, m_impl->max_payload - payload.size());
    }
    slot.payload_size = static_cast<std::uint32_t>(payload.size());
    slot.state.store(2, std::memory_order_release);

    if (!m_impl->data_notifier.notify()) {
        m_impl->release_range(start_seq, 1);
        return false;
    }
    return true;
}

bool mq_queue::send_message_wait(std::uint32_t writer_id, std::uint64_t writer_instance_id, mc::string_view payload,
                                 std::chrono::steady_clock::duration timeout, std::uint8_t priority)
{
    if (!is_valid() || payload.size() > m_impl->max_payload) {
        return false;
    }

    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (true) {
        if (send_message(writer_id, writer_instance_id, payload, priority)) {
            return true;
        }
        if (std::chrono::steady_clock::now() >= deadline) {
            return false;
        }
        if (!m_impl->space_notifier.wait_until(deadline)) {
            return false;
        }
        m_impl->space_notifier.drain();
    }
}

std::optional<mq_queue_message> mq_queue::try_receive_message(const mq_queue_writer_validator& validator)
{
    if (!is_valid()) {
        return std::nullopt;
    }

    const auto writer_is_current = [&](std::uint32_t writer_id, std::uint64_t writer_instance_id) {
        return !validator || validator(writer_id, writer_instance_id);
    };

    while (true) {
        const auto head  = m_impl->header->head_seq.load(std::memory_order_acquire);
        auto&      first = m_impl->slot_at(head);
        if (first.sequence.load(std::memory_order_acquire) != head) {
            return std::nullopt;
        }

        const auto state = first.state.load(std::memory_order_acquire);
        if (state == 0) {
            return std::nullopt;
        }

        if (state == 1) {
            if (!writer_is_current(first.writer_id, first.writer_instance_id)) {
                m_impl->release_range(head, 1);
                continue;
            }
            return std::nullopt;
        }

        if (state != 2) {
            return std::nullopt;
        }

        if (!writer_is_current(first.writer_id, first.writer_instance_id)) {
            m_impl->release_range(head, 1);
            continue;
        }

        mq_queue_message message;
        message.message_id         = first.message_id;
        message.writer_id          = first.writer_id;
        message.writer_instance_id = first.writer_instance_id;
        message.priority           = first.priority;
        message.payload.append(first.payload, first.payload_size);

        m_impl->release_range(head, 1);
        return message;
    }
}

} // namespace mc::shm
