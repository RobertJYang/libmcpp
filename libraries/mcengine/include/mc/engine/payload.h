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

#ifndef MC_ENGINE_PAYLOAD_H
#define MC_ENGINE_PAYLOAD_H

#include <mc/common.h>
#include <mc/dict.h>
#include <mc/memory.h>
#include <mc/reflect/reflectable_macro.h>
#include <mc/result.h>
#include <mc/string.h>
#include <mc/string_view.h>
#include <mc/variant.h>

#include <utility>

namespace mc::engine {

enum class message_type : int32_t {
    invalid       = 0,
    method_call   = 1,
    method_return = 2,
    error         = 3,
    signal        = 4,
};

using payload_id_type       = uint64_t;
using payload_field_id_type = uint32_t;
using header_field_id_type  = uint32_t;

namespace payload_ids {

inline constexpr payload_id_type invalid       = 0;
inline constexpr payload_id_type method_call   = 1;
inline constexpr payload_id_type method_return = 2;
inline constexpr payload_id_type error         = 3;
inline constexpr payload_id_type signal        = 4;

} // namespace payload_ids

namespace header_field_ids {

inline constexpr header_field_id_type type           = 1;
inline constexpr header_field_id_type destination    = 2;
inline constexpr header_field_id_type sender         = 3;
inline constexpr header_field_id_type path           = 4;
inline constexpr header_field_id_type interface_name = 5;
inline constexpr header_field_id_type member_name    = 6;
inline constexpr header_field_id_type error_name     = 7;
inline constexpr header_field_id_type serial         = 8;
inline constexpr header_field_id_type reply_serial   = 9;
inline constexpr header_field_id_type timeout_ms     = 10;
inline constexpr header_field_id_type context        = 11;

} // namespace header_field_ids

namespace payload_field_ids {

namespace method_call {
inline constexpr payload_field_id_type signature = 1;
inline constexpr payload_field_id_type args      = 2;
} // namespace method_call

namespace method_return {
inline constexpr payload_field_id_type value     = 1;
inline constexpr payload_field_id_type signature = 2;
} // namespace method_return

namespace error {
inline constexpr payload_field_id_type name    = 1;
inline constexpr payload_field_id_type message = 2;
} // namespace error

namespace signal {
inline constexpr payload_field_id_type signature = 1;
inline constexpr payload_field_id_type args      = 2;
} // namespace signal

} // namespace payload_field_ids

class MC_API abstract_payload : public mc::shared_base {
public:
    virtual ~abstract_payload() = default;

    virtual payload_id_type  payload_id() const noexcept      = 0;
    virtual message_type     message_type_id() const noexcept = 0;
    virtual void             to_variant(mc::dict& dict) const = 0;
    virtual bool             is_opaque() const noexcept;
    mc::string               to_bytes() const;
    virtual mc::result<void> validate() const
    {
        return {};
    }
};

template <typename Derived, payload_id_type PayloadId, message_type MessageId>
class payload : public abstract_payload {
public:
    static constexpr payload_id_type static_payload_id() noexcept
    {
        return PayloadId;
    }

    static constexpr message_type static_message_type() noexcept
    {
        return MessageId;
    }

    payload_id_type payload_id() const noexcept override
    {
        return static_payload_id();
    }

    message_type message_type_id() const noexcept override
    {
        return static_message_type();
    }
};

struct method_call_payload : public payload<method_call_payload, payload_ids::method_call, message_type::method_call> {
    MC_REFLECTABLE("mc.engine.method_call_payload");

    method_call_payload() = default;

    method_call_payload(mc::string_view signature_value, mc::variants args_value = {})
        : signature(signature_value), args(std::move(args_value))
    {}

    void to_variant(mc::dict& dict) const override;

    mc::string   signature;
    mc::variants args;
};

struct method_return_payload
    : public payload<method_return_payload, payload_ids::method_return, message_type::method_return> {
    MC_REFLECTABLE("mc.engine.method_return_payload");

    method_return_payload() = default;

    method_return_payload(mc::variant value_value, mc::string_view signature_value = {})
        : value(std::move(value_value)), signature(signature_value)
    {}

    void to_variant(mc::dict& dict) const override;

    mc::variant value;
    mc::string  signature;
};

struct error_payload : public payload<error_payload, payload_ids::error, message_type::error> {
    MC_REFLECTABLE("mc.engine.error_payload");

    error_payload() = default;

    error_payload(mc::string_view name_value, mc::string_view message_value) : name(name_value), message(message_value)
    {}

    void to_variant(mc::dict& dict) const override;

    mc::string name;
    mc::string message;
};

struct signal_payload : public payload<signal_payload, payload_ids::signal, message_type::signal> {
    MC_REFLECTABLE("mc.engine.signal_payload");

    signal_payload() = default;

    signal_payload(mc::string_view signature_value, mc::variants args_value = {})
        : signature(signature_value), args(std::move(args_value))
    {}

    void to_variant(mc::dict& dict) const override;

    mc::string   signature;
    mc::variants args;
};

struct opaque_payload : public abstract_payload {
    MC_REFLECTABLE("mc.engine.opaque_payload");

    opaque_payload() = default;

    opaque_payload(payload_id_type payload_id_value, message_type message_type_value, mc::string wire_bytes_value)
        : id(payload_id_value), type(message_type_value), wire_bytes(std::move(wire_bytes_value))
    {}

    payload_id_type payload_id() const noexcept override
    {
        return id;
    }

    message_type message_type_id() const noexcept override
    {
        return type;
    }

    void to_variant(mc::dict& dict) const override;

    bool is_opaque() const noexcept override
    {
        return true;
    }

    payload_id_type id{payload_ids::invalid};
    message_type    type{message_type::invalid};
    mc::string      wire_bytes;
};

} // namespace mc::engine

#endif // MC_ENGINE_PAYLOAD_H
