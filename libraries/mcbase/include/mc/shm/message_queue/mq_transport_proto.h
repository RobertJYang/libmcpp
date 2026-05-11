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

#ifndef MC_SHM_MESSAGE_QUEUE_MQ_TRANSPORT_PROTO_H
#define MC_SHM_MESSAGE_QUEUE_MQ_TRANSPORT_PROTO_H

#include <mc/common.h>
#include <mc/protocol.h>
#include <mc/shm/message_queue/mq_types.h>
#include <mc/small_function.h>

#include <memory>

namespace mc::shm {

class mq_queue;

class MC_API mq_transport_proto : public mc::proto::protocol {
public:
    mq_transport_proto() = default;
    ~mq_transport_proto() override;

    void configure(mq_queue& queue, std::uint32_t writer_id, std::uint64_t writer_instance_id);
    void set_receive_completion_handler(mc::small_function<void(), 64> handler);
    void shutdown() noexcept;

protected:
    mc::proto::execution_state on_push(mc::proto::proto_request& req) override;
    mc::proto::execution_state on_pop(mc::proto::proto_request& req) override;

private:
    struct send_runtime;
    struct receive_runtime;
    bool                             _is_configured() const noexcept;
    mc::string                       _payload_from_buffer(const mc::proto::buffer& buffer) const;
    bool                             _submit_payload(mc::string_view payload) const;
    bool                             _try_receive_payload(mc::string& payload) const;
    void                             _replace_payload(mc::proto::buffer& buffer, mc::string_view payload) const;
    void                             _queue_pending_request(mc::proto::proto_request& req);
    mq_queue*                        m_queue{nullptr};
    std::uint32_t                    m_writer_id{0};
    std::uint64_t                    m_writer_instance_id{0};
    std::shared_ptr<send_runtime>    m_send_runtime;
    std::shared_ptr<receive_runtime> m_receive_runtime;
};

} // namespace mc::shm

#endif // MC_SHM_MESSAGE_QUEUE_MQ_TRANSPORT_PROTO_H
