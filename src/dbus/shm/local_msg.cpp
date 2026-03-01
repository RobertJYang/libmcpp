/*
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <mc/dbus/shm/local_msg.h>
#include <mc/log.h>

namespace mc::dbus {
static constexpr size_t MAX_VARIANT_DEPTH  = 64;
static constexpr size_t MSG_SIZE           = 12;
static constexpr size_t TYPE_INDEX         = 0;
static constexpr size_t SERVICE_NAME_INDEX = 1;
static constexpr size_t PATH_INDEX         = 2;
static constexpr size_t INTERFACE_INDEX    = 3;
static constexpr size_t MEMBER_INDEX       = 4;
static constexpr size_t ERROR_NAME_INDEX   = 5;
static constexpr size_t SIGNATURE_INDEX    = 6;
static constexpr size_t ARGS_INDEX         = 7;
static constexpr size_t SENDER_INDEX       = 8;
static constexpr size_t SERIAL_INDEX       = 9;
static constexpr size_t LOCAL_CALL_INDEX   = 10;
static constexpr size_t REPLY_SERIAL_INDEX = 11;
static constexpr size_t TIMESTAMP_INDEX    = 12;

template <typename T>
static T get_variants_item(const variants& v, size_t index, type_id expected_type,
                           const T& default_value) {
    if (v.size() <= index) {
        return default_value;
    }
    if (v[index].get_type() != expected_type) {
        return default_value;
    }
    return v[index].as<T>(default_value);
}

static uint32_t get_uint32_item(const variants& v, size_t index, uint32_t default_value) {
    if (v.size() <= index) {
        return default_value;
    }
    if (v[index].is_integer()) {
        return v[index].as_uint32();
    }
    return default_value;
}

static size_t get_signature_item_count(std::string_view signature) {
    size_t count = 0;
    for (signature_iterator it(signature); !it.at_end(); it.next()) {
        count++;
    }
    return count;
}

local_msg::local_msg(std::string_view service_name, std::string_view path,
                     std::string_view interface, std::string_view member)
    : m_local_call(false), m_type(DBUS_MESSAGE_TYPE_METHOD_CALL), m_serial(0), m_reply_serial(0),
      m_service_name(service_name), m_path(path), m_interface(interface), m_member(member),
      m_error_name("NA") {
}

local_msg::local_msg(const variants& v) {
    m_type = get_uint32_item(v, TYPE_INDEX, DBUS_MESSAGE_TYPE_METHOD_CALL);
    m_service_name =
        get_variants_item<std::string>(v, SERVICE_NAME_INDEX, type_id::string_type, "");
    m_path        = get_variants_item<std::string>(v, PATH_INDEX, type_id::string_type, "");
    m_interface   = get_variants_item<std::string>(v, INTERFACE_INDEX, type_id::string_type, "");
    m_member      = get_variants_item<std::string>(v, MEMBER_INDEX, type_id::string_type, "");
    m_error_name  = get_variants_item<std::string>(v, ERROR_NAME_INDEX, type_id::string_type, "");
    m_signature   = get_variants_item<std::string>(v, SIGNATURE_INDEX, type_id::string_type, "");
    auto raw_args = get_variants_item<variants>(v, ARGS_INDEX, type_id::array_type, variants());
    if (m_signature.empty()) {
        m_args = raw_args;
    } else {
        try {
            m_args = parse_variant_elements(signature_iterator(m_signature), raw_args, 0);
        } catch (const std::exception& e) {
            m_args = raw_args;
            elog("failed to parse local msg args, error: ${error}", ("error", e.what()));
        }
    }
    m_sender       = get_variants_item<std::string>(v, SENDER_INDEX, type_id::string_type, "");
    m_serial       = get_uint32_item(v, SERIAL_INDEX, 0);
    m_local_call   = get_variants_item<bool>(v, LOCAL_CALL_INDEX, type_id::bool_type, false);
    m_reply_serial = get_uint32_item(v, REPLY_SERIAL_INDEX, 0);
    m_timestamp    = get_variants_item<std::string>(v, TIMESTAMP_INDEX, type_id::string_type, "");
}

std::tuple<std::string, std::string> local_msg::get_error() const {
    return std::make_tuple(std::string(m_error_name), get_error_message());
}

void local_msg::method_return() {
    m_type         = DBUS_MESSAGE_TYPE_METHOD_RETURN;
    m_reply_serial = m_serial;
    m_service_name = m_sender;
}

void local_msg::error(std::string_view error_name, std::string_view message) {
    m_type         = DBUS_MESSAGE_TYPE_ERROR;
    m_reply_serial = m_serial;
    m_service_name = m_sender;
    m_error_name   = error_name;
    m_signature    = "s";
    m_args         = variants();
    m_args.push_back(message);
}

void local_msg::set_member(std::string_view member) {
    m_member = member;
}

uint32_t local_msg::msg_type() const {
    return m_type;
}

std::string_view local_msg::path() const {
    return m_path;
}

std::string_view local_msg::interface() const {
    return m_interface;
}

std::string_view local_msg::member() const {
    return m_member;
}

std::string_view local_msg::signature() const {
    return m_signature;
}

void local_msg::append_args(std::string_view signature, const variants& args) {
    m_signature  = signature;
    m_args       = variants();
    size_t index = 0;
    for (signature_iterator it(signature); !it.at_end(); it.next()) {
        MC_ASSERT(index < args.size(), "invalid args size");
        m_args.push_back(args[index]);
        index++;
    }
}

template <typename T>
static auto parse_variant_basic(const variant& v) {
    if constexpr (std::is_arithmetic_v<T>) {
        return v.as<T>();
    } else if constexpr (std::is_same_v<T, std::string_view> || std::is_same_v<T, std::string> ||
                         std::is_same_v<T, mc::dbus::path> ||
                         std::is_same_v<T, mc::dbus::signature>) {
        if (v.is_string()) {
            return v.get_string();
        } else {
            return v.as<std::string>();
        }
    } else {
        static_assert(std::is_same_v<T, void>, "unsupported type");
    }
}

static dict parse_variant_dict(signature_iterator it, const dict& v, size_t depth) {
    ensure_message_depth(depth);
    ensure_container_max_length(v);
    dict output;
    auto key_it = it.get_dict_key_iterator();
    auto val_it = it.get_dict_value_iterator();
    for (const auto& entry : v) {
        auto key    = local_msg::parse_variant(key_it, entry.key, depth + 1);
        auto val    = local_msg::parse_variant(val_it, entry.value, depth + 1);
        output[key] = val;
    }
    return output;
}

static variants parse_variant_array(signature_iterator it, const variants& v, size_t depth) {
    ensure_message_depth(depth);
    ensure_container_max_length(v);
    variants output;
    for (const auto& item : v) {
        output.push_back(local_msg::parse_variant(it, item, depth + 1));
    }
    return output;
}

static variant parse_variant_array_or_dict(signature_iterator it, const variant& v, size_t depth) {
    ensure_message_depth(depth);
    if (it.current_type_code() == type_code::dict_entry_start) {
        if (v.is_array() && v.as_array().empty()) {
            // 兼容lua框架，空数组也允许作为空字典处理
            return mc::dict();
        }
        return parse_variant_dict(it, v.get_object(), depth + 1);
    }
    if (v.is_null()) {
        return variant();
    }
    if (it.current_type_code() == type_code::byte_type && v.is_string()) {
        return parse_variant_basic<std::string_view>(v);
    }
    return parse_variant_array(it, v.get_array(), depth + 1);
}

variants local_msg::parse_variant_elements(signature_iterator it, const variants& v, size_t depth) {
    ensure_message_depth(depth);
    ensure_container_max_length(v);
    variants output;
    for (const auto& item : v) {
        signature_iterator item_it(it.current_type());
        MC_ASSERT(!item_it.at_end() && !it.at_end(),
                  "invalid number of elements ${size} for signature: ${signature}",
                  ("size", v.size())("signature", it.str()));
        output.push_back(parse_variant(item_it, item, depth + 1));
        it.next();
    }
    return output;
}

variant local_msg::parse_variant(signature_iterator it, const variant& v, size_t depth) {
    ensure_message_depth(depth);
    switch (it.current_type_code()) {
    case type_code::byte_type:
        return parse_variant_basic<uint8_t>(v);
    case type_code::boolean_type:
        return parse_variant_basic<bool>(v);
    case type_code::int16_type:
        return parse_variant_basic<int16_t>(v);
    case type_code::uint16_type:
        return parse_variant_basic<uint16_t>(v);
    case type_code::int32_type:
        return parse_variant_basic<int32_t>(v);
    case type_code::uint32_type:
        return parse_variant_basic<uint32_t>(v);
    case type_code::int64_type:
        return parse_variant_basic<int64_t>(v);
    case type_code::uint64_type:
        return parse_variant_basic<uint64_t>(v);
    case type_code::double_type:
        return parse_variant_basic<double>(v);
    case type_code::string_type:
        return parse_variant_basic<std::string_view>(v);
    case type_code::signature_type:
        return parse_variant_basic<mc::dbus::signature>(v);
    case type_code::object_path_type:
        return parse_variant_basic<mc::dbus::path>(v);
    case type_code::array_start:
        return parse_variant_array_or_dict(it.get_content_iterator(), v, depth + 1);
    case type_code::struct_start:
        return parse_variant_elements(it.get_content_iterator(), v.get_array(), depth + 1);
    case type_code::variant_type: {
        mc::dbus::signature sig;
        detail::variant_to_dbus_signature(sig, v);
        return parse_variant(sig, v, depth + 1);
    }
    default:
        MC_THROW(mc::invalid_arg_exception, "unknown type: ${type}",
                 ("type", it.current_type_char()));
    }
}

void local_msg::append_return_args(std::string_view signature, const variant& arg) {
    // 仅针对 bmc.kepler.ObjectGroup 接口的 GetBinaryObjects 方法，展开 struct 返回值
    if (m_interface == "bmc.kepler.ObjectGroup" && m_member == "GetBinaryObjects") {
        signature_iterator top_it(signature);
        // 若返回签名为struct（），展开元素作为独立返回值，与dbus方法返回保持一致
        if (!top_it.at_end() && top_it.current_type_code() == type_code::struct_start) {
            std::string_view inner = signature.substr(1, signature.size() - 2);
            m_signature            = inner;
            MC_ASSERT(arg.is_array(),
                      "struct return type requires array variant, signature: ${signature}",
                      ("signature", signature));
            m_args = parse_variant_elements(signature_iterator(inner), arg.as_array(), 0);
            return;
        }
    }
    m_signature    = signature;
    size_t arg_cnt = get_signature_item_count(signature);
    if (arg_cnt <= 1) {
        m_args = variants();
        signature_iterator it(signature);
        if (!it.at_end()) {
            m_args.push_back(parse_variant(it, arg, 0));
        }
        return;
    }
    MC_ASSERT(
        arg.is_array(),
        "invalid number of return args, signature: ${signature}, expected: ${arg_cnt}, actual: 1",
        ("signature", signature)("arg_cnt", arg_cnt));
    m_args = parse_variant_elements(signature_iterator(signature), arg.as_array(), 0);
}

const variants& local_msg::read() const {
    return m_args;
}

void local_msg::set_sender(std::string_view sender) {
    m_sender = sender;
}

std::string_view local_msg::sender() const {
    return m_sender;
}

void local_msg::set_serial(uint32_t serial) {
    m_serial = serial;
}

uint32_t local_msg::get_serial() const {
    return m_serial;
}

uint32_t local_msg::get_reply_serial() const {
    return m_reply_serial;
}

std::string_view local_msg::destination() const {
    return m_service_name;
}

void local_msg::set_local_call(bool v) {
    m_local_call = v;
}

bool local_msg::is_local_call() const {
    return m_local_call;
}

message local_msg::new_dbus_msg() const {
    message msg = message::new_method_call(m_service_name, m_path, m_interface, m_member);
    msg.set_sender(m_sender);
    msg.set_serial(m_reply_serial);
    if (m_type == DBUS_MESSAGE_TYPE_METHOD_RETURN) {
        message rsp = message::new_method_return(msg);
        if (!m_signature.empty()) {
            for (auto arg : m_args) {
                rsp.writer().write_variant_value(arg);
            }
        }
        rsp.set_serial(m_serial);
        return rsp;
    }
    if (m_type == DBUS_MESSAGE_TYPE_ERROR) {
        message rsp = message::new_error(msg, m_error_name, get_error_message());
        rsp.set_serial(m_serial);
        return rsp;
    }
    MC_THROW(mc::invalid_arg_exception, "invalid message type");
}

std::string local_msg::pack() const {
    mc::dbus::serialize::write_buffer wb;
    wb.write_arg(m_type);
    wb.write_arg(m_service_name);
    wb.write_arg(m_path);
    wb.write_arg(m_interface);
    wb.write_arg(m_member);
    wb.write_arg(m_error_name);
    wb.write_arg(m_signature);
    signature_iterator it(m_signature);
    wb.write_variant_elements(it, m_args);
    wb.write_arg(m_sender);
    wb.write_arg(m_serial);
    wb.write_arg(m_local_call);
    wb.write_arg(m_reply_serial);
    return wb.to_string();
}

static int64_t get_monotonic_time_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

void local_msg::set_timestamp() {
    std::string time_str = std::string(sizeof(int64_t) + 2, 0);
    int64_t time_ms = get_monotonic_time_ms();
    time_str[0] = '#';
    for (size_t i = 0; i < sizeof(int64_t); i++) {
        time_str[i + 1] = (time_ms >> (i * 8)) & 0xFF;
    }
    time_str[sizeof(int64_t) + 1] = '#';
    m_timestamp = std::move(time_str);
}

int64_t local_msg::duration_ms_from_timestamp() const {
    if (m_timestamp.empty() || m_timestamp.size() != sizeof(int64_t) + 2 || m_timestamp[0] != '#' || m_timestamp[sizeof(int64_t) + 1] != '#') {
        return -1;
    }
    int64_t time_ms = 0;
    for (size_t i = 0; i < sizeof(int64_t); i++) {
        time_ms |= (m_timestamp[i + 1] << (i * 8));
    }
    return get_monotonic_time_ms() - time_ms;
}

std::string local_msg::get_error_message() const {
    if (m_args.empty()) {
        return "";
    }
    return m_args[0].as<std::string>("");
}

bool shm_pending_msgs::send(std::string_view source_unique_name, uint32_t serial,
                            shm_msg_promise promise) {
    std::lock_guard lock(m_mutex);
    auto&           serial_map = m_pending_msgs[std::string(source_unique_name)];
    if (serial_map.find(serial) != serial_map.end()) {
        return false;
    }
    auto result = serial_map.emplace(std::piecewise_construct, std::forward_as_tuple(serial),
                                     std::forward_as_tuple(std::move(promise)));
    return result.second;
}

bool shm_pending_msgs::reply(std::string_view destination_unique_name, uint32_t serial,
                             local_msg& msg) {
    std::lock_guard lock(m_mutex);
    auto&           serial_map = m_pending_msgs[std::string(destination_unique_name)];
    auto            it         = serial_map.find(serial);
    if (it == serial_map.end()) {
        return false;
    }
    auto promise = std::move(it->second);
    serial_map.erase(it);
    promise.set_value(msg);
    return true;
}

void shm_pending_msgs::remove(std::string_view unique_name, uint32_t serial) {
    std::lock_guard lock(m_mutex);
    auto            it = m_pending_msgs.find(std::string(unique_name));
    if (it == m_pending_msgs.end()) {
        return;
    }
    auto serial_it = it->second.find(serial);
    if (serial_it != it->second.end()) {
        it->second.erase(serial_it);
    }
}

void shm_pending_msgs::clear(std::string_view unique_name) {
    std::lock_guard lock(m_mutex);
    auto            it = m_pending_msgs.find(std::string(unique_name));
    if (it != m_pending_msgs.end()) {
        m_pending_msgs.erase(it);
    }
}

} // namespace mc::dbus