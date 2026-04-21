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

#include <mc/engine/message_codec.h>

#include <mc/io/io_buffer.h>
#include <mc/io/io_stream.h>
#include <mc/reflect.h>

#include <limits>
#include <vector>

MC_REFLECTABLE("mc.engine.message_type", mc::engine::message_type)
MC_REFLECT_ENUM(mc::engine::message_type, (invalid)(method_call)(method_return)(error)(signal))
MC_REFLECT(mc::engine::method_call_payload, (signature)(args))
MC_REFLECT(mc::engine::method_return_payload, (value)(signature))
MC_REFLECT(mc::engine::error_payload, (name)(message))
MC_REFLECT(mc::engine::signal_payload, (signature)(args))
MC_REFLECT(mc::engine::opaque_payload, (id)(type)(wire_bytes))
MC_REFLECT(mc::engine::message_header, ())
MC_REFLECT(mc::engine::message, ())

namespace mc::engine {
namespace {

constexpr uint32_t MESSAGE_MAGIC   = 0x4d43454dU;
constexpr uint16_t MESSAGE_VERSION = 2;
constexpr uint32_t PAYLOAD_MAGIC   = 0x4d43504cU;
constexpr uint16_t PAYLOAD_VERSION = 1;

enum class binary_value_type : uint8_t {
    null_type = 0,
    int8_type,
    uint8_type,
    int16_type,
    uint16_type,
    int32_type,
    uint32_type,
    int64_type,
    uint64_type,
    double_type,
    bool_type,
    string_type,
    array_type,
    object_type,
    blob_type,
};

enum class payload_body_encoding : uint8_t {
    dict_body = 0,
    field_tlv = 1,
};

enum class header_encoding : uint8_t {
    dict_body = 0,
    field_tlv = 1,
};

struct decoded_payload_field {
    payload_field_id_type id{0};
    mc::variant           value;
};

struct decoded_header_field {
    header_field_id_type id{0};
    mc::variant          value;
};

uint32_t checked_size(std::size_t size, mc::string_view field_name)
{
    MC_ASSERT_THROW(size <= static_cast<std::size_t>(std::numeric_limits<uint32_t>::max()), mc::out_of_range_exception,
                    "${field} size exceeds uint32 range", ("field", field_name));
    return static_cast<uint32_t>(size);
}

mc::string to_string(const mc::io::io_stream& stream)
{
    auto data = stream.get_data();
    return mc::string(data.data(), data.size());
}

void write_sized_bytes(mc::io::io_stream& stream, mc::string_view bytes)
{
    stream.write_value<uint32_t>(checked_size(bytes.size(), "bytes"));
    if (!bytes.empty()) {
        stream.write(bytes);
    }
}

mc::string_view read_sized_bytes(mc::io::io_stream& stream)
{
    auto length = stream.read_value<uint32_t>();
    if (length == 0) {
        return {};
    }
    return stream.read(length);
}

void        write_variant(mc::io::io_stream& stream, const mc::variant& value);
mc::variant read_variant(mc::io::io_stream& stream);

void write_array(mc::io::io_stream& stream, const mc::variants& values)
{
    stream.write_value<uint32_t>(checked_size(values.size(), "array"));
    for (const auto& item : values) {
        write_variant(stream, item);
    }
}

void write_dict(mc::io::io_stream& stream, const mc::dict& values)
{
    stream.write_value<uint32_t>(checked_size(values.size(), "dict"));
    for (const auto& entry : values) {
        write_variant(stream, entry.key);
        write_variant(stream, entry.value);
    }
}

void write_blob(mc::io::io_stream& stream, const mc::blob& value)
{
    stream.write_value<uint32_t>(checked_size(value.data.size(), "blob"));
    if (!value.data.empty()) {
        stream.write(value.data.data(), value.data.size());
    }
}

void write_variant(mc::io::io_stream& stream, const mc::variant& value)
{
    switch (value.get_type()) {
        case mc::type_id::null_type:
            stream.write_value<uint8_t>(static_cast<uint8_t>(binary_value_type::null_type));
            return;
        case mc::type_id::int8_type:
            stream.write_value<uint8_t>(static_cast<uint8_t>(binary_value_type::int8_type));
            stream.write_value<int8_t>(value.as<int8_t>());
            return;
        case mc::type_id::uint8_type:
            stream.write_value<uint8_t>(static_cast<uint8_t>(binary_value_type::uint8_type));
            stream.write_value<uint8_t>(value.as<uint8_t>());
            return;
        case mc::type_id::int16_type:
            stream.write_value<uint8_t>(static_cast<uint8_t>(binary_value_type::int16_type));
            stream.write_value<int16_t>(value.as<int16_t>());
            return;
        case mc::type_id::uint16_type:
            stream.write_value<uint8_t>(static_cast<uint8_t>(binary_value_type::uint16_type));
            stream.write_value<uint16_t>(value.as<uint16_t>());
            return;
        case mc::type_id::int32_type:
            stream.write_value<uint8_t>(static_cast<uint8_t>(binary_value_type::int32_type));
            stream.write_value<int32_t>(value.as<int32_t>());
            return;
        case mc::type_id::uint32_type:
            stream.write_value<uint8_t>(static_cast<uint8_t>(binary_value_type::uint32_type));
            stream.write_value<uint32_t>(value.as<uint32_t>());
            return;
        case mc::type_id::int64_type:
            stream.write_value<uint8_t>(static_cast<uint8_t>(binary_value_type::int64_type));
            stream.write_value<int64_t>(value.as<int64_t>());
            return;
        case mc::type_id::uint64_type:
            stream.write_value<uint8_t>(static_cast<uint8_t>(binary_value_type::uint64_type));
            stream.write_value<uint64_t>(value.as<uint64_t>());
            return;
        case mc::type_id::double_type:
            stream.write_value<uint8_t>(static_cast<uint8_t>(binary_value_type::double_type));
            stream.write_value<double>(value.as<double>());
            return;
        case mc::type_id::bool_type:
            stream.write_value<uint8_t>(static_cast<uint8_t>(binary_value_type::bool_type));
            stream.write_value<uint8_t>(value.as<bool>() ? 1 : 0);
            return;
        case mc::type_id::string_type:
            stream.write_value<uint8_t>(static_cast<uint8_t>(binary_value_type::string_type));
            write_sized_bytes(stream, value.as<mc::string>());
            return;
        case mc::type_id::array_type:
            stream.write_value<uint8_t>(static_cast<uint8_t>(binary_value_type::array_type));
            write_array(stream, value.as<mc::variants>());
            return;
        case mc::type_id::object_type:
            stream.write_value<uint8_t>(static_cast<uint8_t>(binary_value_type::object_type));
            write_dict(stream, value.as<mc::dict>());
            return;
        case mc::type_id::blob_type:
            stream.write_value<uint8_t>(static_cast<uint8_t>(binary_value_type::blob_type));
            write_blob(stream, value.as<mc::blob>());
            return;
        default:
            MC_THROW(mc::invalid_arg_exception, "unsupported binary variant type: ${type}",
                     ("type", static_cast<int32_t>(value.get_type())));
    }
}

mc::variants read_array(mc::io::io_stream& stream)
{
    auto         count = stream.read_value<uint32_t>();
    mc::variants values;
    values.reserve(count);
    for (uint32_t index = 0; index < count; ++index) {
        values.push_back(read_variant(stream));
    }
    return values;
}

mc::dict read_dict(mc::io::io_stream& stream)
{
    auto     count = stream.read_value<uint32_t>();
    mc::dict values;
    for (uint32_t index = 0; index < count; ++index) {
        auto key    = read_variant(stream);
        auto value  = read_variant(stream);
        values[key] = value;
    }
    return values;
}

mc::blob read_blob(mc::io::io_stream& stream)
{
    auto            length = stream.read_value<uint32_t>();
    mc::string_view bytes  = length == 0 ? mc::string_view{} : stream.read(length);
    return mc::blob(bytes.data(), bytes.size());
}

mc::variant read_variant(mc::io::io_stream& stream)
{
    auto type = static_cast<binary_value_type>(stream.read_value<uint8_t>());
    switch (type) {
        case binary_value_type::null_type:
            return {};
        case binary_value_type::int8_type:
            return mc::variant(stream.read_value<int8_t>());
        case binary_value_type::uint8_type:
            return mc::variant(stream.read_value<uint8_t>());
        case binary_value_type::int16_type:
            return mc::variant(stream.read_value<int16_t>());
        case binary_value_type::uint16_type:
            return mc::variant(stream.read_value<uint16_t>());
        case binary_value_type::int32_type:
            return mc::variant(stream.read_value<int32_t>());
        case binary_value_type::uint32_type:
            return mc::variant(stream.read_value<uint32_t>());
        case binary_value_type::int64_type:
            return mc::variant(stream.read_value<int64_t>());
        case binary_value_type::uint64_type:
            return mc::variant(stream.read_value<uint64_t>());
        case binary_value_type::double_type:
            return mc::variant(stream.read_value<double>());
        case binary_value_type::bool_type:
            return mc::variant(stream.read_value<uint8_t>() != 0);
        case binary_value_type::string_type:
            return mc::variant(mc::string(read_sized_bytes(stream)));
        case binary_value_type::array_type:
            return mc::variant(read_array(stream));
        case binary_value_type::object_type:
            return mc::variant(read_dict(stream));
        case binary_value_type::blob_type:
            return mc::variant(read_blob(stream));
        default:
            MC_THROW(mc::invalid_arg_exception, "unsupported binary wire type: ${type}",
                     ("type", static_cast<uint32_t>(type)));
    }
}

mc::string encode_variant_bytes(const mc::variant& value)
{
    mc::io::io_stream stream(128);
    write_variant(stream, value);
    return to_string(stream);
}

mc::variant decode_variant_bytes(mc::string_view bytes)
{
    mc::io::io_stream stream(mc::io::io_buffer::wrap(bytes.data(), bytes.size()), false);
    auto              value = read_variant(stream);
    MC_ASSERT_THROW(stream.readable_bytes() == 0, mc::invalid_arg_exception, "unexpected trailing bytes");
    return value;
}

mc::string encode_dict_bytes(const mc::dict& dict)
{
    return encode_variant_bytes(mc::variant(dict));
}

mc::dict decode_dict_bytes(mc::string_view bytes)
{
    auto value = decode_variant_bytes(bytes);
    MC_ASSERT_THROW(value.is_dict(), mc::invalid_arg_exception, "decoded bytes must be dict");
    return value.as<mc::dict>();
}

void write_payload_field(mc::io::io_stream& stream, payload_field_id_type field_id, const mc::variant& value)
{
    auto field_bytes = encode_variant_bytes(value);
    stream.write_value<payload_field_id_type>(field_id);
    write_sized_bytes(stream, field_bytes);
}

mc::string standard_field_path(payload_id_type payload_id, payload_field_id_type field_id)
{
    switch (payload_id) {
        case payload_ids::method_call:
            if (field_id == payload_field_ids::method_call::signature) {
                return "body.signature";
            }
            if (field_id == payload_field_ids::method_call::args) {
                return "body.args";
            }
            break;
        case payload_ids::method_return:
            if (field_id == payload_field_ids::method_return::value) {
                return "body.value";
            }
            if (field_id == payload_field_ids::method_return::signature) {
                return "body.signature";
            }
            break;
        case payload_ids::error:
            if (field_id == payload_field_ids::error::name) {
                return "body.name";
            }
            if (field_id == payload_field_ids::error::message) {
                return "body.message";
            }
            break;
        case payload_ids::signal:
            if (field_id == payload_field_ids::signal::signature) {
                return "body.signature";
            }
            if (field_id == payload_field_ids::signal::args) {
                return "body.args";
            }
            break;
        default:
            break;
    }

    return "body.unknown";
}

void apply_field_hook(mc::string_view field_path, payload_field_id_type field_id, payload_id_type payload_id,
                      message_type message_kind, mc::variant& value, const message_decode_options& options)
{
    if (!options.on_field_decoded) {
        return;
    }

    decode_field_context context{field_path, field_id, payload_id, message_kind, value.get_type()};
    options.on_field_decoded(context, value);
}

mc::string standard_header_field_path(header_field_id_type field_id)
{
    switch (field_id) {
        case header_field_ids::type:
            return "header.type";
        case header_field_ids::destination:
            return "header.destination";
        case header_field_ids::sender:
            return "header.sender";
        case header_field_ids::path:
            return "header.path";
        case header_field_ids::interface_name:
            return "header.interface_name";
        case header_field_ids::member_name:
            return "header.member_name";
        case header_field_ids::error_name:
            return "header.error_name";
        case header_field_ids::serial:
            return "header.serial";
        case header_field_ids::reply_serial:
            return "header.reply_serial";
        case header_field_ids::timeout_ms:
            return "header.timeout_ms";
        case header_field_ids::context:
            return "header.context";
        default:
            return "header.unknown";
    }
}

void write_header_field(mc::io::io_stream& stream, header_field_id_type field_id, const mc::variant& value)
{
    auto field_bytes = encode_variant_bytes(value);
    stream.write_value<header_field_id_type>(field_id);
    write_sized_bytes(stream, field_bytes);
}

mc::string encode_header_tlv_body(const message_header& header)
{
    mc::io::io_stream stream(192);
    stream.write_value<uint32_t>(11);

    mc::variant type_value;
    mc::to_variant(header.type, type_value);
    write_header_field(stream, header_field_ids::type, type_value);
    write_header_field(stream, header_field_ids::destination, mc::variant(header.destination));
    write_header_field(stream, header_field_ids::sender, mc::variant(header.sender));
    write_header_field(stream, header_field_ids::path, mc::variant(header.path));
    write_header_field(stream, header_field_ids::interface_name, mc::variant(header.interface_name));
    write_header_field(stream, header_field_ids::member_name, mc::variant(header.member_name));
    write_header_field(stream, header_field_ids::error_name, mc::variant(header.error_name));
    write_header_field(stream, header_field_ids::serial, mc::variant(static_cast<uint64_t>(header.serial)));
    write_header_field(stream, header_field_ids::reply_serial, mc::variant(static_cast<uint64_t>(header.reply_serial)));
    write_header_field(stream, header_field_ids::timeout_ms, mc::variant(static_cast<int64_t>(header.timeout.count())));
    write_header_field(stream, header_field_ids::context, mc::variant(header.context));

    return to_string(stream);
}

std::vector<decoded_header_field> decode_header_tlv_fields(mc::string_view               body_bytes,
                                                           const message_decode_options& options)
{
    mc::io::io_stream stream(mc::io::io_buffer::wrap(body_bytes.data(), body_bytes.size()), false);
    auto              field_count = stream.read_value<uint32_t>();

    std::vector<decoded_header_field> fields;
    fields.reserve(field_count);
    for (uint32_t index = 0; index < field_count; ++index) {
        auto field_id    = stream.read_value<header_field_id_type>();
        auto field_bytes = read_sized_bytes(stream);
        auto value       = decode_variant_bytes(field_bytes);
        auto field_path  = standard_header_field_path(field_id);
        apply_field_hook(field_path, field_id, payload_ids::invalid, message_type::invalid, value, options);
        fields.push_back({field_id, std::move(value)});
    }

    MC_ASSERT_THROW(stream.readable_bytes() == 0, mc::invalid_arg_exception, "unexpected trailing header field bytes");
    return fields;
}

const mc::variant* find_header_field(const std::vector<decoded_header_field>& fields, header_field_id_type field_id)
{
    for (const auto& field : fields) {
        if (field.id == field_id) {
            return &field.value;
        }
    }
    return nullptr;
}

template <typename T>
T header_field_or_default(const std::vector<decoded_header_field>& fields, header_field_id_type field_id, T fallback)
{
    auto* value = find_header_field(fields, field_id);
    return value != nullptr ? value->as<T>() : std::move(fallback);
}

message_header build_header_from_fields(const std::vector<decoded_header_field>& fields)
{
    message_header header;

    if (auto* type_value = find_header_field(fields, header_field_ids::type)) {
        mc::from_variant(*type_value, header.type);
    }
    header.destination    = header_field_or_default<mc::string>(fields, header_field_ids::destination, {});
    header.sender         = header_field_or_default<mc::string>(fields, header_field_ids::sender, {});
    header.path           = header_field_or_default<mc::string>(fields, header_field_ids::path, {});
    header.interface_name = header_field_or_default<mc::string>(fields, header_field_ids::interface_name, {});
    header.member_name    = header_field_or_default<mc::string>(fields, header_field_ids::member_name, {});
    header.error_name     = header_field_or_default<mc::string>(fields, header_field_ids::error_name, {});
    header.serial         = header_field_or_default<uint64_t>(fields, header_field_ids::serial, 0U);
    header.reply_serial   = header_field_or_default<uint64_t>(fields, header_field_ids::reply_serial, 0U);
    header.timeout =
        mc::milliseconds(header_field_or_default<int64_t>(fields, header_field_ids::timeout_ms, int64_t{0}));
    header.context = header_field_or_default<mc::dict>(fields, header_field_ids::context, {});
    return header;
}

mc::string encode_standard_payload_body(const abstract_payload& payload)
{
    mc::io::io_stream stream(128);

    switch (payload.payload_id()) {
        case payload_ids::method_call: {
            const auto& typed = static_cast<const method_call_payload&>(payload);
            stream.write_value<uint32_t>(2);
            write_payload_field(stream, payload_field_ids::method_call::signature, mc::variant(typed.signature));
            write_payload_field(stream, payload_field_ids::method_call::args, mc::variant(typed.args));
            break;
        }
        case payload_ids::method_return: {
            const auto& typed = static_cast<const method_return_payload&>(payload);
            stream.write_value<uint32_t>(2);
            write_payload_field(stream, payload_field_ids::method_return::value, typed.value);
            write_payload_field(stream, payload_field_ids::method_return::signature, mc::variant(typed.signature));
            break;
        }
        case payload_ids::error: {
            const auto& typed = static_cast<const error_payload&>(payload);
            stream.write_value<uint32_t>(2);
            write_payload_field(stream, payload_field_ids::error::name, mc::variant(typed.name));
            write_payload_field(stream, payload_field_ids::error::message, mc::variant(typed.message));
            break;
        }
        case payload_ids::signal: {
            const auto& typed = static_cast<const signal_payload&>(payload);
            stream.write_value<uint32_t>(2);
            write_payload_field(stream, payload_field_ids::signal::signature, mc::variant(typed.signature));
            write_payload_field(stream, payload_field_ids::signal::args, mc::variant(typed.args));
            break;
        }
        default:
            MC_THROW(mc::invalid_arg_exception, "unsupported standard payload id ${id}", ("id", payload.payload_id()));
    }

    return to_string(stream);
}

std::vector<decoded_payload_field> decode_standard_payload_fields(payload_id_type payload_id, message_type message_kind,
                                                                  mc::string_view               body_bytes,
                                                                  const message_decode_options& options)
{
    mc::io::io_stream stream(mc::io::io_buffer::wrap(body_bytes.data(), body_bytes.size()), false);
    auto              field_count = stream.read_value<uint32_t>();

    std::vector<decoded_payload_field> fields;
    fields.reserve(field_count);
    for (uint32_t index = 0; index < field_count; ++index) {
        auto field_id    = stream.read_value<payload_field_id_type>();
        auto field_bytes = read_sized_bytes(stream);
        auto value       = decode_variant_bytes(field_bytes);
        auto field_path  = standard_field_path(payload_id, field_id);
        apply_field_hook(field_path, field_id, payload_id, message_kind, value, options);
        fields.push_back({field_id, std::move(value)});
    }

    MC_ASSERT_THROW(stream.readable_bytes() == 0, mc::invalid_arg_exception, "unexpected trailing payload field bytes");
    return fields;
}

const mc::variant* find_payload_field(const std::vector<decoded_payload_field>& fields, payload_field_id_type field_id)
{
    for (const auto& field : fields) {
        if (field.id == field_id) {
            return &field.value;
        }
    }
    return nullptr;
}

mc::variant get_dict_field(const mc::dict& dict, mc::string_view key)
{
    MC_ASSERT_THROW(dict.find(key) != dict.end(), mc::invalid_arg_exception, "missing field ${field}", ("field", key));
    return dict.at(key);
}

template <typename T>
T decode_field_as(const mc::dict& dict, mc::string_view key, mc::string_view field_path, payload_id_type payload_id,
                  message_type message_kind, const message_decode_options& options)
{
    auto value = get_dict_field(dict, key);
    apply_field_hook(field_path, 0, payload_id, message_kind, value, options);
    return value.as<T>();
}

message_type decode_message_type_field(const mc::dict& dict, mc::string_view key, mc::string_view field_path,
                                       payload_id_type payload_id, message_type message_kind,
                                       const message_decode_options& options)
{
    auto value = get_dict_field(dict, key);
    apply_field_hook(field_path, 0, payload_id, message_kind, value, options);

    message_type result{message_type::invalid};
    mc::from_variant(value, result);
    return result;
}

mc::variant decode_variant_field(const mc::dict& dict, mc::string_view key, mc::string_view field_path,
                                 payload_id_type payload_id, message_type message_kind,
                                 const message_decode_options& options)
{
    auto value = get_dict_field(dict, key);
    apply_field_hook(field_path, 0, payload_id, message_kind, value, options);
    return value;
}

mc::variants decode_variants_field(const mc::dict& dict, mc::string_view key, mc::string_view field_path,
                                   payload_id_type payload_id, message_type message_kind,
                                   const message_decode_options& options)
{
    return decode_variant_field(dict, key, field_path, payload_id, message_kind, options).as<mc::variants>();
}

mc::dict decode_dict_field(const mc::dict& dict, mc::string_view key, mc::string_view field_path,
                           payload_id_type payload_id, message_type message_kind, const message_decode_options& options)
{
    return decode_variant_field(dict, key, field_path, payload_id, message_kind, options).as<mc::dict>();
}

message_header decode_header_dict(const mc::dict& dict, const message_decode_options& options)
{
    message_header header;
    header.type =
        decode_message_type_field(dict, "type", "header.type", payload_ids::invalid, message_type::invalid, options);
    header.destination = decode_field_as<mc::string>(dict, "destination", "header.destination", payload_ids::invalid,
                                                     message_type::invalid, options);
    header.sender      = decode_field_as<mc::string>(dict, "sender", "header.sender", payload_ids::invalid,
                                                     message_type::invalid, options);
    header.path =
        decode_field_as<mc::string>(dict, "path", "header.path", payload_ids::invalid, message_type::invalid, options);
    header.interface_name = decode_field_as<mc::string>(dict, "interface_name", "header.interface_name",
                                                        payload_ids::invalid, message_type::invalid, options);
    header.member_name    = decode_field_as<mc::string>(dict, "member_name", "header.member_name", payload_ids::invalid,
                                                        message_type::invalid, options);
    header.error_name     = decode_field_as<mc::string>(dict, "error_name", "header.error_name", payload_ids::invalid,
                                                        message_type::invalid, options);
    header.serial         = decode_field_as<uint64_t>(dict, "serial", "header.serial", payload_ids::invalid,
                                                      message_type::invalid, options);
    header.reply_serial   = decode_field_as<uint64_t>(dict, "reply_serial", "header.reply_serial", payload_ids::invalid,
                                                      message_type::invalid, options);
    header.timeout        = mc::milliseconds(decode_field_as<int64_t>(dict, "timeout_ms", "header.timeout_ms",
                                                                      payload_ids::invalid, message_type::invalid, options));
    header.context =
        decode_dict_field(dict, "context", "header.context", payload_ids::invalid, message_type::invalid, options);
    return header;
}

mc::dict payload_body_to_dict(const abstract_payload& payload)
{
    mc::dict body;
    payload.to_variant(body);
    return body;
}

mc::string encode_payload_record_bytes(payload_id_type payload_id, message_type message_kind,
                                       payload_body_encoding body_encoding, mc::string_view body_bytes)
{
    mc::io::io_stream stream(128 + body_bytes.size());
    stream.write_value<uint32_t>(PAYLOAD_MAGIC);
    stream.write_value<uint16_t>(PAYLOAD_VERSION);
    stream.write_value<payload_id_type>(payload_id);
    stream.write_value<uint32_t>(static_cast<uint32_t>(message_kind));
    stream.write_value<uint8_t>(static_cast<uint8_t>(body_encoding));
    write_sized_bytes(stream, body_bytes);
    return to_string(stream);
}

mc::shared_ptr<const abstract_payload> make_opaque_payload(payload_id_type payload_id, message_type message_kind,
                                                           mc::string_view wire_bytes)
{
    return mc::make_shared<opaque_payload>(payload_id, message_kind, mc::string(wire_bytes.data(), wire_bytes.size()));
}

mc::shared_ptr<const abstract_payload> decode_standard_payload(payload_id_type payload_id, message_type message_kind,
                                                               const mc::dict&               body,
                                                               const message_decode_options& options)
{
    switch (payload_id) {
        case payload_ids::method_call:
            if (message_kind != message_type::method_call) {
                return nullptr;
            }
            return mc::make_shared<method_call_payload>(
                decode_field_as<mc::string>(body, "signature", "body.signature", payload_id, message_kind, options),
                decode_variants_field(body, "args", "body.args", payload_id, message_kind, options));
        case payload_ids::method_return:
            if (message_kind != message_type::method_return) {
                return nullptr;
            }
            return mc::make_shared<method_return_payload>(
                decode_variant_field(body, "value", "body.value", payload_id, message_kind, options),
                decode_field_as<mc::string>(body, "signature", "body.signature", payload_id, message_kind, options));
        case payload_ids::error:
            if (message_kind != message_type::error) {
                return nullptr;
            }
            return mc::make_shared<error_payload>(
                decode_field_as<mc::string>(body, "name", "body.name", payload_id, message_kind, options),
                decode_field_as<mc::string>(body, "message", "body.message", payload_id, message_kind, options));
        case payload_ids::signal:
            if (message_kind != message_type::signal) {
                return nullptr;
            }
            return mc::make_shared<signal_payload>(
                decode_field_as<mc::string>(body, "signature", "body.signature", payload_id, message_kind, options),
                decode_variants_field(body, "args", "body.args", payload_id, message_kind, options));
        default:
            return nullptr;
    }
}

mc::shared_ptr<const abstract_payload> decode_standard_payload(payload_id_type payload_id, message_type message_kind,
                                                               const std::vector<decoded_payload_field>& fields)
{
    switch (payload_id) {
        case payload_ids::method_call: {
            if (message_kind != message_type::method_call) {
                return nullptr;
            }
            auto* signature = find_payload_field(fields, payload_field_ids::method_call::signature);
            auto* args      = find_payload_field(fields, payload_field_ids::method_call::args);
            MC_ASSERT_THROW(signature != nullptr, mc::invalid_arg_exception, "missing method_call signature field");
            MC_ASSERT_THROW(args != nullptr, mc::invalid_arg_exception, "missing method_call args field");
            return mc::make_shared<method_call_payload>(signature->as<mc::string>(), args->as<mc::variants>());
        }
        case payload_ids::method_return: {
            if (message_kind != message_type::method_return) {
                return nullptr;
            }
            auto* value     = find_payload_field(fields, payload_field_ids::method_return::value);
            auto* signature = find_payload_field(fields, payload_field_ids::method_return::signature);
            MC_ASSERT_THROW(value != nullptr, mc::invalid_arg_exception, "missing method_return value field");
            MC_ASSERT_THROW(signature != nullptr, mc::invalid_arg_exception, "missing method_return signature field");
            return mc::make_shared<method_return_payload>(*value, signature->as<mc::string>());
        }
        case payload_ids::error: {
            if (message_kind != message_type::error) {
                return nullptr;
            }
            auto* name    = find_payload_field(fields, payload_field_ids::error::name);
            auto* message = find_payload_field(fields, payload_field_ids::error::message);
            MC_ASSERT_THROW(name != nullptr, mc::invalid_arg_exception, "missing error name field");
            MC_ASSERT_THROW(message != nullptr, mc::invalid_arg_exception, "missing error message field");
            return mc::make_shared<error_payload>(name->as<mc::string>(), message->as<mc::string>());
        }
        case payload_ids::signal: {
            if (message_kind != message_type::signal) {
                return nullptr;
            }
            auto* signature = find_payload_field(fields, payload_field_ids::signal::signature);
            auto* args      = find_payload_field(fields, payload_field_ids::signal::args);
            MC_ASSERT_THROW(signature != nullptr, mc::invalid_arg_exception, "missing signal signature field");
            MC_ASSERT_THROW(args != nullptr, mc::invalid_arg_exception, "missing signal args field");
            return mc::make_shared<signal_payload>(signature->as<mc::string>(), args->as<mc::variants>());
        }
        default:
            return nullptr;
    }
}

mc::shared_ptr<const abstract_payload> decode_unknown_payload(payload_id_type payload_id, message_type message_kind,
                                                              const mc::dict&               body,
                                                              const message_decode_options& options)
{
    if (options.decode_payload) {
        auto custom = options.decode_payload(payload_id, message_kind, body, options);
        if (custom) {
            return custom;
        }
    }

    if (body.find("wire_bytes") != body.end()) {
        auto wire_blob = body.at("wire_bytes").as<mc::blob>();
        return make_opaque_payload(payload_id, message_kind, mc::string(wire_blob.data.data(), wire_blob.data.size()));
    }

    auto body_bytes = encode_dict_bytes(body);
    return make_opaque_payload(
        payload_id, message_kind,
        encode_payload_record_bytes(payload_id, message_kind, payload_body_encoding::dict_body, body_bytes));
}

} // namespace

bool abstract_payload::is_opaque() const noexcept
{
    return false;
}

mc::string abstract_payload::to_bytes() const
{
    return encode_payload_bytes(*this);
}

void method_call_payload::to_variant(mc::dict& dict) const
{
    mc::reflect::to_variant(*this, dict);
}

void method_return_payload::to_variant(mc::dict& dict) const
{
    mc::reflect::to_variant(*this, dict);
}

void error_payload::to_variant(mc::dict& dict) const
{
    mc::reflect::to_variant(*this, dict);
}

void signal_payload::to_variant(mc::dict& dict) const
{
    mc::reflect::to_variant(*this, dict);
}

void opaque_payload::to_variant(mc::dict& dict) const
{
    dict["wire_bytes"] = mc::blob(wire_bytes.data(), wire_bytes.size());
}

void message_header::to_variant(const message_header& header, mc::dict& dict)
{
    dict["type"]           = mc::variant();
    dict["destination"]    = header.destination;
    dict["sender"]         = header.sender;
    dict["path"]           = header.path;
    dict["interface_name"] = header.interface_name;
    dict["member_name"]    = header.member_name;
    dict["error_name"]     = header.error_name;
    dict["serial"]         = static_cast<uint64_t>(header.serial);
    dict["reply_serial"]   = static_cast<uint64_t>(header.reply_serial);
    dict["timeout_ms"]     = static_cast<int64_t>(header.timeout.count());
    dict["context"]        = header.context;
    mc::to_variant(header.type, dict["type"]);
}

void message_header::from_variant(const mc::dict& dict, message_header& header)
{
    header = decode_header_dict(dict, {});
}

mc::dict encode_payload(const abstract_payload& payload)
{
    mc::dict record;
    record["payload_id"]   = static_cast<uint64_t>(payload.payload_id());
    record["message_type"] = mc::variant();
    mc::to_variant(payload.message_type_id(), record["message_type"]);
    record["body"] = payload_body_to_dict(payload);
    return record;
}

mc::shared_ptr<const abstract_payload> decode_payload(const mc::dict& dict)
{
    return decode_payload(dict, {});
}

mc::shared_ptr<const abstract_payload> decode_payload(const mc::dict& dict, const message_decode_options& options)
{
    auto payload_id   = decode_field_as<uint64_t>(dict, "payload_id", "payload.payload_id", payload_ids::invalid,
                                                  message_type::invalid, options);
    auto message_kind = decode_message_type_field(dict, "message_type", "payload.message_type", payload_ids::invalid,
                                                  message_type::invalid, options);
    auto body = decode_dict_field(dict, "body", "payload.body", payload_ids::invalid, message_type::invalid, options);

    auto standard = decode_standard_payload(payload_id, message_kind, body, options);
    return standard ? standard : decode_unknown_payload(payload_id, message_kind, body, options);
}

mc::string encode_payload_bytes(const abstract_payload& payload)
{
    if (payload.is_opaque()) {
        return static_cast<const opaque_payload&>(payload).wire_bytes;
    }

    if (decode_standard_payload(payload.payload_id(), payload.message_type_id(), payload_body_to_dict(payload), {}) !=
        nullptr) {
        auto body_bytes = encode_standard_payload_body(payload);
        return encode_payload_record_bytes(payload.payload_id(), payload.message_type_id(),
                                           payload_body_encoding::field_tlv, body_bytes);
    }

    auto body_bytes = encode_dict_bytes(payload_body_to_dict(payload));
    return encode_payload_record_bytes(payload.payload_id(), payload.message_type_id(),
                                       payload_body_encoding::dict_body, body_bytes);
}

mc::shared_ptr<const abstract_payload> decode_payload_bytes(mc::string_view bytes)
{
    return decode_payload_bytes(bytes, {});
}

mc::shared_ptr<const abstract_payload> decode_payload_bytes(mc::string_view               bytes,
                                                            const message_decode_options& options)
{
    mc::io::io_stream stream(mc::io::io_buffer::wrap(bytes.data(), bytes.size()), false);
    auto              magic   = stream.read_value<uint32_t>();
    auto              version = stream.read_value<uint16_t>();
    MC_ASSERT_THROW(magic == PAYLOAD_MAGIC, mc::invalid_arg_exception, "invalid payload magic");
    MC_ASSERT_THROW(version == PAYLOAD_VERSION, mc::invalid_arg_exception, "unsupported payload version ${version}",
                    ("version", version));

    auto payload_id    = stream.read_value<payload_id_type>();
    auto message_kind  = static_cast<message_type>(stream.read_value<uint32_t>());
    auto body_encoding = static_cast<payload_body_encoding>(stream.read_value<uint8_t>());
    auto body_bytes    = read_sized_bytes(stream);

    switch (body_encoding) {
        case payload_body_encoding::field_tlv: {
            auto fields   = decode_standard_payload_fields(payload_id, message_kind, body_bytes, options);
            auto standard = decode_standard_payload(payload_id, message_kind, fields);
            return standard ? standard : make_opaque_payload(payload_id, message_kind, bytes);
        }
        case payload_body_encoding::dict_body: {
            auto body     = decode_dict_bytes(body_bytes);
            auto standard = decode_standard_payload(payload_id, message_kind, body, options);
            if (standard) {
                return standard;
            }

            if (options.decode_payload) {
                auto custom = options.decode_payload(payload_id, message_kind, body, options);
                if (custom) {
                    return custom;
                }
            }

            return make_opaque_payload(payload_id, message_kind, bytes);
        }
        default:
            MC_THROW(mc::invalid_arg_exception, "unsupported payload body encoding ${encoding}",
                     ("encoding", static_cast<uint32_t>(body_encoding)));
    }
}

void message::to_variant(const message& msg, mc::dict& dict)
{
    dict = encode_message(msg);
}

void message::from_variant(const mc::dict& dict, message& msg)
{
    msg = decode_message(dict);
}

mc::string message::to_bytes() const
{
    return encode_message_bytes(*this);
}

message message::from_bytes(mc::string_view bytes)
{
    return decode_message_bytes(bytes);
}

message message::from_bytes(mc::string_view bytes, const message_decode_options& options)
{
    return decode_message_bytes(bytes, options);
}

mc::dict encode_message(const message& msg)
{
    mc::dict header;
    message_header::to_variant(msg.header, header);

    mc::dict dict{
        {"header", header},
        {"has_body", msg.body != nullptr},
    };
    if (msg.body != nullptr) {
        dict["body"] = encode_payload(*msg.body);
    }
    return dict;
}

message decode_message(const mc::dict& dict)
{
    return decode_message(dict, {});
}

message decode_message(const mc::dict& dict, const message_decode_options& options)
{
    message msg;
    msg.header = decode_header_dict(
        decode_dict_field(dict, "header", "message.header", payload_ids::invalid, message_type::invalid, options),
        options);

    auto has_body = decode_field_as<bool>(dict, "has_body", "message.has_body", payload_ids::invalid,
                                          message_type::invalid, options);
    if (has_body) {
        msg.body = decode_payload(
            decode_dict_field(dict, "body", "message.body", payload_ids::invalid, message_type::invalid, options),
            options);
    }
    return msg;
}

mc::string encode_message_bytes(const message& msg)
{
    mc::io::io_stream stream(256);
    stream.write_value<uint32_t>(MESSAGE_MAGIC);
    stream.write_value<uint16_t>(MESSAGE_VERSION);
    stream.write_value<uint8_t>(static_cast<uint8_t>(header_encoding::field_tlv));
    write_sized_bytes(stream, encode_header_tlv_body(msg.header));

    if (msg.body != nullptr) {
        write_sized_bytes(stream, msg.body->to_bytes());
    } else {
        write_sized_bytes(stream, {});
    }

    return to_string(stream);
}

message decode_message_bytes(mc::string_view bytes)
{
    return decode_message_bytes(bytes, {});
}

message decode_message_bytes(mc::string_view bytes, const message_decode_options& options)
{
    mc::io::io_stream stream(mc::io::io_buffer::wrap(bytes.data(), bytes.size()), false);
    auto              magic   = stream.read_value<uint32_t>();
    auto              version = stream.read_value<uint16_t>();
    MC_ASSERT_THROW(magic == MESSAGE_MAGIC, mc::invalid_arg_exception, "invalid message magic");
    MC_ASSERT_THROW(version == MESSAGE_VERSION, mc::invalid_arg_exception, "unsupported message version ${version}",
                    ("version", version));

    message msg;
    auto    header_kind  = static_cast<header_encoding>(stream.read_value<uint8_t>());
    auto    header_bytes = read_sized_bytes(stream);
    switch (header_kind) {
        case header_encoding::field_tlv:
            msg.header = build_header_from_fields(decode_header_tlv_fields(header_bytes, options));
            break;
        case header_encoding::dict_body:
            msg.header = decode_header_dict(decode_dict_bytes(header_bytes), options);
            break;
        default:
            MC_THROW(mc::invalid_arg_exception, "unsupported header encoding ${encoding}",
                     ("encoding", static_cast<uint32_t>(header_kind)));
    }

    auto payload_bytes = read_sized_bytes(stream);
    if (!payload_bytes.empty()) {
        msg.body = decode_payload_bytes(payload_bytes, options);
    }

    MC_ASSERT_THROW(stream.readable_bytes() == 0, mc::invalid_arg_exception, "unexpected trailing message bytes");
    return msg;
}

} // namespace mc::engine
