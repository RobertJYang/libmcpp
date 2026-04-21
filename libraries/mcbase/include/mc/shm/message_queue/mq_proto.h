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

#ifndef MC_SHM_MESSAGE_QUEUE_MQ_PROTO_H
#define MC_SHM_MESSAGE_QUEUE_MQ_PROTO_H

#include <mc/common.h>
#include <mc/protocol.h>
#include <mc/small_function.h>

#include <cstddef>
#include <cstdint>
#include <memory>

namespace mc::shm {

struct mq_proto_inbound_runtime;

class MC_API mq_proto : public mc::proto::protocol {
public:
    using source_liveness_probe = mc::small_function<bool(std::uint16_t, std::uint64_t), 64>;

    static constexpr std::size_t fragment_header_size         = 13;
    static constexpr std::size_t default_max_fragment_payload = 256 - fragment_header_size;

    mq_proto() = default;

    void configure(std::uint32_t instance_id, std::uint16_t endpoint_id,
                   std::size_t max_fragment_payload = default_max_fragment_payload);
    void set_source_liveness_probe(source_liveness_probe probe);

protected:
    mc::proto::execution_state on_push(mc::proto::proto_request& req) override;
    mc::proto::execution_state on_pop(mc::proto::proto_request& req) override;

private:
    bool                                      _is_configured() const noexcept;
    std::uint32_t                             m_instance_id{0};
    std::uint16_t                             m_endpoint_id{0};
    std::uint32_t                             m_next_msg_id{1};
    std::size_t                               m_max_fragment_payload{default_max_fragment_payload};
    std::shared_ptr<mq_proto_inbound_runtime> m_inbound_runtime;
    source_liveness_probe                     m_source_liveness_probe;
};

} // namespace mc::shm

#endif // MC_SHM_MESSAGE_QUEUE_MQ_PROTO_H
