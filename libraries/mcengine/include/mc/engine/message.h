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

#ifndef MC_ENGINE_MESSAGE_H
#define MC_ENGINE_MESSAGE_H

#include <mc/engine/payload.h>
#include <mc/exception.h>
#include <mc/reflect/reflectable_macro.h>
#include <mc/string.h>
#include <mc/time.h>

#include <utility>

namespace mc::engine {

struct message_decode_options;

struct message_header {
    MC_REFLECTABLE("mc.engine.message_header");

    message_type     type{message_type::invalid};
    mc::string       destination;
    mc::string       sender;
    mc::string       path;
    mc::string       interface_name;
    mc::string       member_name;
    mc::string       error_name;
    uint64_t         serial{0};
    uint64_t         reply_serial{0};
    mc::milliseconds timeout{0};
    mc::dict         context;

    static void to_variant(const message_header& header, mc::dict& dict);
    static void from_variant(const mc::dict& dict, message_header& header);
};

struct message {
    MC_REFLECTABLE("mc.engine.message");

    message_header                         header;
    mc::shared_ptr<const abstract_payload> body;

    bool empty() const noexcept
    {
        return body == nullptr;
    }

    template <typename Payload>
    const Payload* try_as() const noexcept
    {
        if (!body || body->payload_id() != Payload::static_payload_id()) {
            return nullptr;
        }
        return dynamic_cast<const Payload*>(body.get());
    }

    template <typename Payload>
    const Payload& as() const
    {
        auto* payload = try_as<Payload>();
        MC_ASSERT_THROW(payload != nullptr, mc::invalid_arg_exception, "message payload type mismatch");
        return *payload;
    }

    mc::string     to_bytes() const;
    static message from_bytes(mc::string_view bytes);
    static message from_bytes(mc::string_view bytes, const message_decode_options& options);

    static void to_variant(const message& msg, mc::dict& dict);
    static void from_variant(const mc::dict& dict, message& msg);
};

template <typename Payload, typename... Args>
mc::shared_ptr<const abstract_payload> make_payload(Args&&... args)
{
    return mc::make_shared<Payload>(std::forward<Args>(args)...);
}

} // namespace mc::engine

#endif // MC_ENGINE_MESSAGE_H
