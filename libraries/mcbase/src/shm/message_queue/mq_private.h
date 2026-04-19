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

#ifndef MC_SHM_MESSAGE_QUEUE_MQ_PRIVATE_H
#define MC_SHM_MESSAGE_QUEUE_MQ_PRIVATE_H

#include <atomic>

#include <mc/shm/message_queue/mq_queue.h>
#include <mc/shm/shared_mapping_view.h>

#include "mq_notifier.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace mc::shm::detail {

struct mq_notifier::impl {
    mc::string name;
#ifdef _WIN32
    HANDLE handle = nullptr;
#else
    int fd = -1;
#endif
};

#ifdef _WIN32
inline HANDLE duplicate_wait_handle(HANDLE handle) noexcept
{
    if (handle == nullptr) {
        return nullptr;
    }

    HANDLE     duplicated = nullptr;
    const auto ok =
        DuplicateHandle(GetCurrentProcess(), handle, GetCurrentProcess(), &duplicated, 0, FALSE, DUPLICATE_SAME_ACCESS);
    return ok == 0 ? nullptr : duplicated;
}
#endif

struct queue_header {
    std::atomic<std::uint32_t> magic{0};
    std::atomic<std::uint32_t> version{0};
    std::uint32_t              slot_count{0};
    std::uint32_t              max_payload_size{0};
    std::atomic<std::uint64_t> head_seq{0};
    std::atomic<std::uint64_t> tail_reserve_seq{0};
    std::atomic<std::uint64_t> next_message_id{1};
};

struct queue_slot {
    std::atomic<std::uint64_t> sequence{0};
    std::atomic<std::uint32_t> state{0};
    std::uint32_t              writer_id{0};
    std::uint32_t              reserved0{0};
    std::uint64_t              writer_instance_id{0};
    std::uint64_t              message_id{0};
    std::uint32_t              fragment_index{0};
    std::uint32_t              fragment_count{0};
    std::uint32_t              payload_size{0};
    std::uint8_t               priority{0};
    std::uint8_t               reserved1[7]{};
    char                       payload[];
};

inline std::size_t slot_size(std::size_t max_payload) noexcept
{
    constexpr std::size_t alignment = alignof(queue_slot);
    const std::size_t     raw       = offsetof(queue_slot, payload) + max_payload;
    return (raw + alignment - 1U) & ~(alignment - 1U);
}

} // namespace mc::shm::detail

namespace mc::shm {

struct mq_queue::impl {
    mq_queue_options      options;
    detail::mq_notifier   data_notifier;
    detail::mq_notifier   space_notifier;
    shared_mapping_view   mapping;
    detail::queue_header* header{nullptr};
    char*                 slots_base{nullptr};
    std::size_t           slot_stride{0};
    std::size_t           max_payload{0};

    detail::queue_slot& slot_at(std::uint64_t sequence) const noexcept
    {
        const auto index = static_cast<std::size_t>(sequence % header->slot_count);
        return *reinterpret_cast<detail::queue_slot*>(slots_base + index * slot_stride);
    }

    void release_range(std::uint64_t start_seq, std::uint32_t fragment_count)
    {
        for (std::uint32_t i = 0; i < fragment_count; ++i) {
            const auto seq          = start_seq + i;
            auto&      slot         = slot_at(seq);
            slot.payload_size       = 0;
            slot.message_id         = 0;
            slot.fragment_index     = 0;
            slot.fragment_count     = 0;
            slot.writer_id          = 0;
            slot.writer_instance_id = 0;
            slot.priority           = 0;
            slot.state.store(0, std::memory_order_release);
            slot.sequence.store(seq + header->slot_count, std::memory_order_release);
        }
        header->head_seq.store(start_seq + fragment_count, std::memory_order_release);
        space_notifier.notify();
    }
};

} // namespace mc::shm

#endif // MC_SHM_MESSAGE_QUEUE_MQ_PRIVATE_H
