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

#ifndef MC_SHM_MESSAGE_QUEUE_MQ_QUEUE_H
#define MC_SHM_MESSAGE_QUEUE_MQ_QUEUE_H

#include <chrono>
#include <memory>
#include <optional>

#include <mc/common.h>
#include <mc/shm/message_queue/mq_types.h>

namespace mc::shm {

namespace detail {
class mq_watcher;
}

class mq_transport_proto;

class MC_API mq_queue {
public:
    mq_queue() noexcept;
    explicit mq_queue(const mq_queue_options& options);
    ~mq_queue();

    mq_queue(const mq_queue&)            = delete;
    mq_queue& operator=(const mq_queue&) = delete;

    mq_queue(mq_queue&& other) noexcept;
    mq_queue& operator=(mq_queue&& other) noexcept;

    bool            is_valid() const noexcept;
    std::size_t     slot_count() const noexcept;
    std::size_t     max_payload() const noexcept;
    mc::string_view shared_memory_name() const noexcept;

    bool send_message(std::uint32_t writer_id, std::uint64_t writer_instance_id, mc::string_view payload,
                      std::uint8_t priority = 0);
    bool send_message_wait(std::uint32_t writer_id, std::uint64_t writer_instance_id, mc::string_view payload,
                           std::chrono::steady_clock::duration timeout, std::uint8_t priority = 0);
    std::optional<mq_queue_message> try_receive_message(const mq_queue_writer_validator& validator);

private:
    friend class detail::mq_watcher;
    friend class mq_transport_proto;

    struct impl;
    std::shared_ptr<impl> m_impl;
};

} // namespace mc::shm

#endif // MC_SHM_MESSAGE_QUEUE_MQ_QUEUE_H
