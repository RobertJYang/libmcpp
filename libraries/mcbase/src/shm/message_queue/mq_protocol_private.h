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

#ifndef MC_SHM_MESSAGE_QUEUE_MQ_PROTOCOL_PRIVATE_H
#define MC_SHM_MESSAGE_QUEUE_MQ_PROTOCOL_PRIVATE_H

#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

#include <mc/io/native_waiter.h>
#include <mc/runtime/steady_timer.h>
#include <mc/string.h>
#include <mc/string_view.h>
#include <mc/time.h>

#include <mc/shm/message_queue/mq_queue.h>

namespace mc::proto::detail {
class request_core;
class request_session;
} // namespace mc::proto::detail

namespace mc::shm {

namespace detail {
class mq_watcher;
}

struct mq_assembly_key {
    std::uint32_t msg_id{0};
    std::uint32_t src{0};

    bool operator==(const mq_assembly_key& other) const
    {
        return msg_id == other.msg_id && src == other.src;
    }
};

struct mq_assembly_key_hash {
    std::size_t operator()(const mq_assembly_key& key) const
    {
        return std::size_t(key.msg_id) * 131U + std::size_t(key.src);
    }
};

struct mq_assembly_state {
    std::uint16_t           total_parts{0};
    std::vector<bool>       received;
    std::vector<mc::string> parts;
    std::size_t             received_count{0};
};

class mq_proto::impl {
public:
    void configure(std::uint32_t instance_id, std::uint16_t endpoint_id, std::size_t max_fragment_payload);

    bool ingest_fragment(mc::string_view fragment, mc::string& out_assembled);

private:
    friend class mq_proto;

    std::uint32_t                                                                m_instance_id{0};
    std::uint16_t                                                                m_endpoint_id{0};
    std::uint32_t                                                                m_next_msg_id{1};
    std::size_t                                                                  m_max_fragment_payload{mq_proto::default_max_fragment_payload};
    std::unordered_map<mq_assembly_key, mq_assembly_state, mq_assembly_key_hash> m_inflight;
};

struct mq_transport_entry {
    mc::string                                 fragment;
    std::optional<mc::time_point>              deadline;
    mc::proto::detail::request_core*           request{nullptr};
    std::shared_ptr<mc::runtime::steady_timer> timer;
    bool                                       completed{false};
};

class mq_transport_proto::impl : public std::enable_shared_from_this<mq_transport_proto::impl> {
public:
    void configure(mq_queue& queue, std::uint32_t writer_id, std::uint64_t writer_instance_id,
                   mq_queue_writer_validator validator, std::unique_ptr<mc::io::native_waiter> space_waiter,
                   std::function<void()> drain_space);
    void start_inbound(mc::proto::instance& host);
    void stop(std::size_t layer_index);
    void submit(mc::string_view fragment, mc::proto::detail::request_core* request, std::size_t layer_index);
    std::optional<mq_queue_message> try_receive_message() const;
    bool                            take_inbound_fragment(mc::string& fragment);

private:
    void arm_space_wait();
    void drain_pending(std::size_t layer_index);
    void fail_entry(const std::shared_ptr<mq_transport_entry>& entry, std::size_t layer_index);
    void complete_entry(const std::shared_ptr<mq_transport_entry>& entry, std::size_t layer_index);

    mq_queue*                                          m_queue{nullptr};
    std::uint32_t                                      m_writer_id{0};
    std::uint64_t                                      m_writer_instance_id{0};
    mq_queue_writer_validator                          m_validator;
    std::mutex                                         m_mutex;
    std::deque<std::shared_ptr<mq_transport_entry>>    m_pending;
    std::unique_ptr<mc::io::native_waiter>             m_space_waiter;
    std::function<void()>                              m_drain_space;
    std::size_t                                        m_layer_index{0};
    bool                                               m_wait_armed{false};
    bool                                               m_stopping{false};
    mc::shared_ptr<mc::proto::detail::request_session> m_inbound_session;
    std::shared_ptr<detail::mq_watcher>                m_inbound_watcher;
    std::deque<mc::string>                             m_inbound_fragments;
};

} // namespace mc::shm

#endif // MC_SHM_MESSAGE_QUEUE_MQ_PROTOCOL_PRIVATE_H
