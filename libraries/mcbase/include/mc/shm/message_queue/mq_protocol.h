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

#ifndef MC_SHM_MESSAGE_QUEUE_MQ_PROTOCOL_H
#define MC_SHM_MESSAGE_QUEUE_MQ_PROTOCOL_H

#include <cstdint>
#include <memory>

#include <mc/protocol.h>
#include <mc/shm/message_queue/mq_types.h>
#include <mc/string.h>

namespace mc::shm {

class mq_queue;
class mq_channel;

class mq_proto : public mc::proto::protocol {
public:
    static constexpr std::size_t fragment_header_size        = 13;
    static constexpr std::size_t default_max_fragment_payload = 256 - fragment_header_size;

    struct packet {
        mc::string    assembled;
        mc::string    payload;
        std::size_t   next_fragment_index{0};
        std::size_t   payload_offset{0};
        std::uint32_t msg_id{0};
        std::uint32_t src{0};
        std::uint16_t total_parts{0};
    };

    struct context {};
    using request_type = mc::proto::request_view<packet, context>;

    mq_proto();
    ~mq_proto();

    void               configure(std::uint32_t instance_id, std::uint16_t endpoint_id,
                                 std::size_t max_fragment_payload = default_max_fragment_payload);
    mc::proto::command on_push(request_type& req);
    mc::proto::command on_pop(request_type& req);

private:
    bool build_fragments(const mc::proto::buffer& buffer, packet& packet, mc::string& error);
    bool build_next_fragment(packet& packet, mc::string& fragment) const;
    bool accept_fragment(mc::string_view fragment, mc::string& assembled);
    bool on_continue(mc::proto::detail::request_core& request) noexcept override;
    bool on_failed(mc::proto::detail::request_core& request) noexcept override;

    class impl;
    std::shared_ptr<impl> m_impl;

    friend class mq_channel;
    friend class impl;
};

class mq_transport_proto : public mc::proto::protocol {
public:
    struct packet {};
    struct context {};
    using request_type = mc::proto::request_view<packet, context>;

    mq_transport_proto();
    ~mq_transport_proto();

    void               configure(mq_queue& queue, std::uint32_t writer_id, std::uint64_t writer_instance_id,
                                 mq_queue_writer_validator validator = {});
    void               stop();
    mc::proto::command on_push(request_type& req);
    mc::proto::command on_pop(request_type& req);
    void               on_start(mc::proto::instance& host) override;
    void               on_stop(mc::proto::instance& host) override;

private:
    void submit_fragment(mc::string_view fragment, mc::proto::detail::request_core* request);
    bool receive_fragment(mc::proto::buffer& buffer) const;

    class impl;
    std::shared_ptr<impl> m_impl;

    friend class mq_channel;
    friend class impl;
};

} // namespace mc::shm

#endif // MC_SHM_MESSAGE_QUEUE_MQ_PROTOCOL_H
