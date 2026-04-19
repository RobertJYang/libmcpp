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

#ifndef MC_SHM_MESSAGE_QUEUE_MQ_TYPES_H
#define MC_SHM_MESSAGE_QUEUE_MQ_TYPES_H

#include <cstdint>

#include <mc/small_function.h>
#include <mc/string.h>
#include <mc/string_view.h>

namespace mc::shm {

struct mq_queue_options {
    mc::string  shared_memory_name;
    std::size_t slot_count       = 256;
    std::size_t max_payload_size = 256;
};

struct mq_queue_message {
    std::uint64_t message_id         = 0;
    std::uint32_t writer_id          = 0;
    std::uint64_t writer_instance_id = 0;
    std::uint8_t  priority           = 0;
    mc::string    payload;
};

using mq_queue_writer_validator =
    mc::small_function<bool(std::uint32_t writer_id, std::uint64_t writer_instance_id), 48>;

} // namespace mc::shm

#endif // MC_SHM_MESSAGE_QUEUE_MQ_TYPES_H
