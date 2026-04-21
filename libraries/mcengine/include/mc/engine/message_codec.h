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

#ifndef MC_ENGINE_MESSAGE_CODEC_H
#define MC_ENGINE_MESSAGE_CODEC_H

#include <mc/common.h>
#include <mc/dict.h>
#include <mc/engine/message.h>

#include <functional>

namespace mc::engine {

struct decode_field_context {
    mc::string_view       field_path;
    payload_field_id_type field_id{0};
    payload_id_type       payload_id{payload_ids::invalid};
    message_type          message_kind{message_type::invalid};
    mc::type_id           value_type{mc::type_id::null_type};
};

struct message_decode_options {
    std::function<void(const decode_field_context&, mc::variant&)> on_field_decoded;
    std::function<mc::shared_ptr<const abstract_payload>(payload_id_type, message_type, const mc::dict&,
                                                         const message_decode_options&)>
        decode_payload;
};

MC_API mc::dict encode_payload(const abstract_payload& payload);
MC_API mc::shared_ptr<const abstract_payload> decode_payload(const mc::dict& dict);
MC_API mc::shared_ptr<const abstract_payload> decode_payload(const mc::dict&               dict,
                                                             const message_decode_options& options);
MC_API mc::string encode_payload_bytes(const abstract_payload& payload);
MC_API mc::shared_ptr<const abstract_payload> decode_payload_bytes(mc::string_view bytes);
MC_API mc::shared_ptr<const abstract_payload> decode_payload_bytes(mc::string_view               bytes,
                                                                   const message_decode_options& options);

MC_API mc::dict encode_message(const message& msg);
MC_API message  decode_message(const mc::dict& dict);
MC_API message  decode_message(const mc::dict& dict, const message_decode_options& options);
MC_API mc::string encode_message_bytes(const message& msg);
MC_API message    decode_message_bytes(mc::string_view bytes);
MC_API message    decode_message_bytes(mc::string_view bytes, const message_decode_options& options);

} // namespace mc::engine

#endif // MC_ENGINE_MESSAGE_CODEC_H
