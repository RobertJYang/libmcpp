/*
 * Copyright (c) 2024 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <mc/dbus/error.h>
#include <mc/dbus/message.h>
#include <mc/dbus/signature.h>
#include <mc/dbus/validator.h>

namespace mc::dbus {
void ensure_container_max_length(const char* type_name, std::size_t size) {
    if (size > validator::maximum_array_size) {
        MC_THROW(
            mc::invalid_arg_exception,
            "类型 ${type} 的大小超过了最大限制, 最大 ${max_size}, 当前 ${current_size}",
            ("type", type_name)("max_size", validator::maximum_array_size)("current_size", size));
    }
}

void ensure_message_depth(std::size_t depth) {
    if (depth >= validator::maximum_message_depth) {
        MC_THROW(mc::invalid_arg_exception,
                 "超过最大消息深度限制，最大 ${max_depth}, 当前 ${current_depth}",
                 ("max_depth", validator::maximum_message_depth)("current_depth", depth));
    }
}

namespace detail {

template <typename T>
static void demarshal_variant_basic(const message_reader& reader, mc::variant& v) {
    reader.ensure_type(get_signature<T>().first_type());

    T t;
    reader >> t;

    if constexpr (std::is_same_v<T, mc::dbus::signature>) {
        v = t.str();
    } else {
        v = t;
    }
}

template <typename T>
static void marshal_variant_basic(const message_writer& writer, const mc::variant& v) {
    if constexpr (std::is_arithmetic_v<T>) {
        writer << v.as<T>();
    } else if constexpr (std::is_same_v<T, std::string_view> || std::is_same_v<T, std::string> ||
                         std::is_same_v<T, mc::dbus::path>) {
        if (v.is_string()) {
            writer << v.get_string();
        } else {
            writer << v.as<std::string>();
        }
    } else if constexpr (std::is_same_v<T, mc::dbus::signature>) {
        if (v.is_string()) {
            writer.write_signature(v.get_string());
        } else {
            writer.write_signature(v.as<std::string>());
        }
    } else {
        static_assert(std::is_same_v<T, void>, "unsupported type");
    }
}

static inline void add_type(signature& sig, type_code type) {
    sig += type;
}

static bool check_array_all_type(const mc::variants& v, mc::type_id type) {
    for (const auto& item : v) {
        if (item.get_type() != type) {
            return false;
        }
    }

    return true;
}

static std::pair<bool, bool> check_dict_all_type(const mc::dict& v, mc::type_id key_type,
                                                 mc::type_id value_type) {
    bool key_equal   = true;
    bool value_equal = true;
    for (const auto& item : v) {
        if (item.key.get_type() != key_type) {
            key_equal = false;
        }

        if (item.value.get_type() != value_type) {
            value_equal = false;
        }
    }

    return {key_equal, value_equal};
}

static void variant_to_dbus_signature(signature& sig, const mc::variant& v);

static void array_to_dbus_signature(signature& sig, const mc::variants& v) {
    // 如果数组是空，无法探测到数组类型，默认使用byte类型
    if (v.empty()) {
        sig += container::array_of_byte;
        return;
    }

    // 如果数组中所有元素类型相同，则直接为该类型的数组
    if (check_array_all_type(v, v[0].get_type())) {
        sig += type_code::array_start;
        variant_to_dbus_signature(sig, v[0]);
        return;
    }

    // 否则是 av 数组
    sig += container::array_of_variant;
}

static void dict_to_dbus_signature(signature& sig, const mc::dict& v) {
    // 如果字典是空，无法探测到字典类型，默认使用a{sv}类型
    if (v.empty()) {
        sig += container::dict_string_var;
        return;
    }

    sig += type_code::array_start;

    auto it = v.begin();
    sig += type_code::dict_entry_start;

    auto result = check_dict_all_type(v, it->key.get_type(), it->value.get_type());
    if (result.first && result.second) {
        // 字典所有元素的 {k, v} 类型相同，直接使用 k, v 类型签名
        variant_to_dbus_signature(sig, it->key);
        variant_to_dbus_signature(sig, it->value);
    } else if (result.first) {
        // k 的类型相同，v 的类型不同，保持 key 类型签名，value 使用 variant 类型签名
        variant_to_dbus_signature(sig, it->key);
        sig += type_to_char(type_code::variant_type);
    } else if (result.second) {
        // k 的类型不同，v 的类型相同，保持 value 类型签名，key 使用 string 类型
        sig += type_to_char(type_code::string_type);
        variant_to_dbus_signature(sig, it->value);
    } else {
        // 字典元素的 {k, v} 类型不同，使用 a{sv} 类型签名
        sig += container::dict_string_var;
    }

    sig += type_code::dict_entry_end;
}

static void variant_to_dbus_signature(signature& sig, const mc::variant& v) {
    switch (v.get_type()) {
    case mc::type_id::null_type: // dbus 不支持空类型，用一个 std::optional 类型来表示
        sig += "ai";
        return;
    case mc::type_id::bool_type:
        return add_type(sig, type_code::boolean_type);
    case mc::type_id::int8_type:
    case mc::type_id::uint8_type:
        return add_type(sig, type_code::byte_type);
    case mc::type_id::int16_type:
        return add_type(sig, type_code::int16_type);
    case mc::type_id::uint16_type:
        return add_type(sig, type_code::uint16_type);
    case mc::type_id::int32_type:
        return add_type(sig, type_code::int32_type);
    case mc::type_id::uint32_type:
        return add_type(sig, type_code::uint32_type);
    case mc::type_id::int64_type:
        return add_type(sig, type_code::int64_type);
    case mc::type_id::uint64_type:
        return add_type(sig, type_code::uint64_type);
    case mc::type_id::double_type:
        return add_type(sig, type_code::double_type);
    case mc::type_id::string_type:
        return add_type(sig, type_code::string_type);
    case mc::type_id::object_type:
        return dict_to_dbus_signature(sig, v.get_object());
    case mc::type_id::array_type:
        return array_to_dbus_signature(sig, v.get_array());
    case mc::type_id::blob_type:
        sig += container::array_of_byte;
        break;
    default:
        throw_unknow_type_error(v.get_type());
    }
}
} // namespace detail

message message::new_method_call(std::string_view destination, std::string_view path,
                                 std::string_view interface, std::string_view member) {
    MC_ASSERT(validator::is_valid_bus_name(destination), "invalid destination: ${destination}",
              ("destination", destination));
    MC_ASSERT(validator::is_valid_path(path), "invalid path: ${path}", ("path", path));
    MC_ASSERT(validator::is_valid_interface_name(interface), "invalid interface: ${interface}",
              ("interface", interface));
    MC_ASSERT(validator::is_valid_member_name(member), "invalid member: ${member}",
              ("member", member));

    return {dbus_message_new_method_call(destination.data(), path.data(), interface.data(),
                                         member.data())};
}

message message::new_method_return(const message& msg) {
    MC_ASSERT(msg.is_valid(), "invalid message");
    MC_ASSERT(msg.get_type() == message_type::method_call, "invalid message type");

    return {dbus_message_new_method_return(msg.m_dbus_message)};
}

message message::new_error(const message& msg, std::string_view error_name,
                           std::string_view error_message) {
    MC_ASSERT(msg.is_valid(), "invalid message");
    MC_ASSERT(msg.get_type() == message_type::method_call, "invalid message type");
    MC_ASSERT(validator::is_valid_error_name(error_name), "invalid error name: ${error_name}",
              ("error_name", error_name));

    return {dbus_message_new_error(msg.m_dbus_message, error_name.data(), error_message.data())};
}

message message::new_signal(std::string_view path, std::string_view interface,
                            std::string_view member) {
    MC_ASSERT(validator::is_valid_path(path), "invalid path: ${path}", ("path", path));
    MC_ASSERT(validator::is_valid_interface_name(interface), "invalid interface: ${interface}",
              ("interface", interface));
    MC_ASSERT(validator::is_valid_member_name(member), "invalid member: ${member}",
              ("member", member));

    return {dbus_message_new_signal(path.data(), interface.data(), member.data())};
}

message message::new_message(message_type msg_type) {
    if (msg_type > message_type::invalid && msg_type <= message_type::num_message_types) {
        return {dbus_message_new(static_cast<int>(msg_type))};
    }

    return {};
}

message message::new_error_message(std::string_view error_name, std::string_view error_message) {
    message msg = new_message(message_type::error);
    msg.set_error_name(error_name);
    msg.writer() << error_message;
    return msg;
}

message::message(DBusMessage* msg, bool add_ref) : m_dbus_message(msg) {
    if (add_ref && m_dbus_message) {
        dbus_message_ref(m_dbus_message);
    }
}

message::~message() {
    release();
}

message::message(const message& other) : m_dbus_message(other.m_dbus_message) {
    if (m_dbus_message) {
        dbus_message_ref(m_dbus_message);
    }
}

message& message::operator=(const message& other) {
    if (this != &other) {
        release();

        if (other.m_dbus_message) {
            dbus_message_ref(other.m_dbus_message);
            m_dbus_message = other.m_dbus_message;
        }
    }

    return *this;
}

DBusMessage* message::get_dbus_message() const {
    return m_dbus_message;
}

bool message::is_valid() const {
    return m_dbus_message != nullptr;
}

bool message::is_error() const {
    return get_type() == message_type::error;
}

bool message::is_method_call() const {
    return get_type() == message_type::method_call;
}

bool message::is_method_return() const {
    return get_type() == message_type::method_return;
}

bool message::is_signal() const {
    return get_type() == message_type::signal;
}

void message::release() {
    if (m_dbus_message) {
        dbus_message_unref(m_dbus_message);
        m_dbus_message = nullptr;
    }
}

message_reader message::reader() const {
    return message_reader(*this);
}

message_writer message::writer() {
    return message_writer(*this);
}

std::pair<dbus_ptr<char>, std::size_t> message::marshal() {
    if (m_dbus_message == nullptr) {
        return {};
    }

    char* data;
    int   size;
    if (!dbus_message_marshal(m_dbus_message, &data, &size)) {
        return {nullptr, 0};
    }

    return {dbus_ptr<char>(data), size};
}

bool message::demarshal(const std::vector<uint8_t>& out, error& err) {
    return demarshal(reinterpret_cast<const char*>(out.data()), out.size(), err);
}

bool message::demarshal(const char* in, std::size_t len, error& err) {
    if (!in || len == 0) {
        return false;
    }

    release();

    auto msg = dbus_message_demarshal(in, len, &err);
    if (err.is_set()) {
        return false;
    }

    m_dbus_message = msg;
    return true;
}

message::message(message&& other) noexcept : m_dbus_message(other.m_dbus_message) {
    other.m_dbus_message = nullptr;
}

message& message::operator=(message&& other) noexcept {
    if (this != &other) {
        release();

        m_dbus_message       = other.m_dbus_message;
        other.m_dbus_message = nullptr;
    }

    return *this;
}

message_type message::get_type() const {
    if (m_dbus_message == nullptr) {
        return message_type::invalid;
    }

    auto type = static_cast<message_type>(dbus_message_get_type(m_dbus_message));
    if (type >= message_type::invalid && type <= message_type::num_message_types) {
        return type;
    }

    return message_type::invalid;
}

std::string_view message::get_path() const {
    if (m_dbus_message == nullptr) {
        return {};
    }

    return dbus_message_get_path(m_dbus_message);
}

std::string_view message::get_interface() const {
    if (m_dbus_message == nullptr) {
        return {};
    }

    return dbus_message_get_interface(m_dbus_message);
}

std::string_view message::get_member() const {
    if (m_dbus_message == nullptr) {
        return {};
    }

    return dbus_message_get_member(m_dbus_message);
}

std::string_view message::get_error_name() const {
    if (m_dbus_message == nullptr) {
        return {};
    }

    return dbus_message_get_error_name(m_dbus_message);
}

std::string_view message::get_destination() const {
    if (m_dbus_message == nullptr) {
        return {};
    }

    return dbus_message_get_destination(m_dbus_message);
}

std::string_view message::get_sender() const {
    if (m_dbus_message == nullptr) {
        return {};
    }

    return dbus_message_get_sender(m_dbus_message);
}

std::string_view message::get_signature() const {
    if (m_dbus_message == nullptr) {
        return {};
    }

    return dbus_message_get_signature(m_dbus_message);
}

uint32_t message::get_serial() const {
    if (m_dbus_message == nullptr) {
        return 0;
    }

    return dbus_message_get_serial(m_dbus_message);
}

uint32_t message::get_reply_serial() const {
    if (m_dbus_message == nullptr) {
        return 0;
    }

    return dbus_message_get_reply_serial(m_dbus_message);
}

void message::set_path(std::string_view path) {
    if (m_dbus_message == nullptr) {
        return;
    }

    MC_ASSERT(path.empty() || validator::is_valid_path(path), "invalid path: ${path}",
              ("path", path));

    dbus_message_set_path(m_dbus_message, path.data());
}

void message::set_interface(std::string_view interface) {
    if (m_dbus_message == nullptr) {
        return;
    }

    MC_ASSERT(interface.empty() || validator::is_valid_interface_name(interface),
              "invalid interface name: ${interface}", ("interface", interface));

    dbus_message_set_interface(m_dbus_message, interface.data());
}

void message::set_member(std::string_view member) {
    if (m_dbus_message == nullptr) {
        return;
    }

    MC_ASSERT(member.empty() || validator::is_valid_member_name(member),
              "invalid member name: ${member}", ("member", member));

    dbus_message_set_member(m_dbus_message, member.data());
}

void message::set_error_name(std::string_view error_name) {
    if (m_dbus_message == nullptr) {
        return;
    }

    MC_ASSERT(error_name.empty() || validator::is_valid_error_name(error_name),
              "invalid error name: ${error_name}", ("error_name", error_name));

    dbus_message_set_error_name(m_dbus_message, error_name.data());
}

void message::set_destination(std::string_view destination) {
    if (m_dbus_message == nullptr) {
        return;
    }

    MC_ASSERT(destination.empty() || validator::is_valid_bus_name(destination),
              "invalid destination: ${destination}", ("destination", destination));

    dbus_message_set_destination(m_dbus_message, destination.data());
}

void message::set_sender(std::string_view sender) {
    if (m_dbus_message == nullptr) {
        return;
    }

    MC_ASSERT(sender.empty() || validator::is_valid_bus_name(sender), "invalid sender: ${sender}",
              ("sender", sender));

    dbus_message_set_sender(m_dbus_message, sender.data());
}

bool message::has_signature(std::string_view signature) {
    if (m_dbus_message == nullptr) {
        return false;
    }

    return dbus_message_has_signature(m_dbus_message, signature.data());
}

void message::set_serial(uint32_t serial) {
    if (m_dbus_message == nullptr) {
        return;
    }

    dbus_message_set_serial(m_dbus_message, serial);
}

void message::lock() {
    if (m_dbus_message == nullptr) {
        return;
    }

    dbus_message_lock(m_dbus_message);
}

mc::variants message::read_args() const {
    mc::variants args;
    auto         sig    = get_signature();
    auto         reader = this->reader();
    for (mc::dbus::signature_iterator it(sig); !it.at_end(); it.next()) {
        mc::variant v;
        reader.read_variant_value(it.current_type_code(), v, 0);
        args.emplace_back(std::move(v));
    }
    return args;
}

/* -------------------- message_reader -------------------- */
message_reader::message_reader() {
}

message_reader::message_reader(const message& msg) {
    dbus_message_iter_init(msg.get_dbus_message(), &m_iter);
}

void message_reader::recurse(const message_reader& parent) const {
    dbus_message_iter_recurse(&parent.m_iter, &m_iter);
}

const message_reader& message_reader::next() const {
    dbus_message_iter_next(&m_iter);
    return *this;
}

bool message_reader::at_end() const {
    return dbus_message_iter_get_arg_type(&m_iter) == DBUS_TYPE_INVALID;
}

void message_reader::ensure_type(int expected) const {
    int actual = dbus_message_iter_get_arg_type(&m_iter);
    ensure_type(expected, actual);
}

void message_reader::ensure_type(int expected, int actual) {
    char actual_type[]   = {static_cast<char>(actual), 0};
    char expected_type[] = {static_cast<char>(expected), 0};
    if (actual != expected) {
        MC_THROW(mc::invalid_arg_exception, "类型不匹配，期望${expected}, 实际为${actual}",
                 ("expected", expected_type)("actual", actual_type));
    }
}

type_code message_reader::current_type() const {
    return static_cast<type_code>(dbus_message_iter_get_arg_type(&m_iter));
}

const message_reader& operator>>(const message_reader& reader, std::string& v) {
    std::string_view sv;
    reader >> sv;
    v = std::string(sv);
    return reader;
}

const message_reader& operator>>(const message_reader& reader, std::string_view& v) {
    reader.ensure_type(DBUS_TYPE_STRING);

    const char* str;
    dbus_message_iter_get_basic(&reader.m_iter, &str);
    v = std::string_view(str);

    return reader.next();
}

const message_reader& operator>>(const message_reader& reader, mc::dbus::path& v) {
    reader.ensure_type(DBUS_TYPE_OBJECT_PATH);

    const char* path = nullptr;
    dbus_message_iter_get_basic(&reader.m_iter, &path);
    v = path;
    return reader.next();
}

const message_reader& operator>>(const message_reader& reader, mc::dbus::signature& v) {
    reader.ensure_type(DBUS_TYPE_SIGNATURE);

    const char* sig;
    dbus_message_iter_get_basic(&reader.m_iter, &sig);
    v = sig;
    return reader.next();
}

const message_reader& operator>>(const message_reader& reader, mc::blob& v) {
    reader.ensure_type(DBUS_TYPE_ARRAY);
    reader.ensure_type(DBUS_TYPE_BYTE, dbus_message_iter_get_element_type(&reader.m_iter));

    const uint8_t* data;
    int            size;
    dbus_message_iter_get_fixed_array(&reader.m_iter, &data, &size);
    v.data.assign(data, data + size);
    return reader.next();
}

const message_reader& operator>>(const message_reader& reader, mc::variant& v) {
    reader.read_variant(v, 0);

    return reader.next();
}

const message_reader& operator>>(const message_reader& reader, mc::variants& v) {
    // 在这里已经知道是 av 类型了，所以直接按 v 类型去读取内容
    reader.ensure_type(DBUS_TYPE_ARRAY);

    message_reader sub_reader;
    sub_reader.recurse(reader);
    sub_reader.ensure_type(DBUS_TYPE_VARIANT);

    sub_reader.read_variant_array(v, 0);
    return reader.next();
}

const message_reader& operator>>(const message_reader& reader, mc::dict& v) {
    reader.ensure_type(DBUS_TYPE_ARRAY);

    message_reader sub_reader;
    sub_reader.recurse(reader);
    sub_reader.ensure_type(DBUS_TYPE_DICT_ENTRY);

    mc::mutable_dict tmp;
    sub_reader.read_variant_dict(tmp, 0);

    v = std::move(tmp);
    return reader.next();
}

const message_reader& operator>>(const message_reader& reader, mc::mutable_dict& v) {
    reader.ensure_type(DBUS_TYPE_ARRAY);

    message_reader sub_reader;
    sub_reader.recurse(reader);
    sub_reader.ensure_type(DBUS_TYPE_DICT_ENTRY);

    sub_reader.read_variant_dict(v, 0);

    return reader.next();
}

void message_reader::read_variant_array_or_dict(mc::variant& v, std::size_t depth) const {
    ensure_message_depth(depth);
    ensure_type(DBUS_TYPE_ARRAY);

    message_reader sub_reader;
    sub_reader.recurse(*this);

    if (sub_reader.current_type() == type_code::dict_entry_start) {
        mc::mutable_dict dict;
        sub_reader.read_variant_dict(dict, depth);
        v = std::move(dict);
        return;
    }

    mc::variants arr;
    sub_reader.read_variant_array(arr, depth);
    v = std::move(arr);
}

void message_reader::read_variant_array(mc::variants& arr, std::size_t depth) const {
    ensure_message_depth(depth);

    while (!at_end()) {
        mc::variant item;
        read_variant_value(current_type(), item, depth + 1);
        arr.emplace_back(std::move(item));
        next();
    }
}

void message_reader::read_variant_struct(mc::variant& v, std::size_t depth) const {
    ensure_message_depth(depth);

    mc::variants arr;
    while (!at_end()) {
        mc::variant item;
        read_variant_value(current_type(), item, depth + 1);
        arr.emplace_back(std::move(item));
        next();
    }

    v = std::move(arr);
}

void message_reader::read_variant_dict(mc::mutable_dict& dict, std::size_t depth) const {
    ensure_message_depth(depth);

    while (!at_end()) {
        message_reader entry_reader;
        entry_reader.recurse(*this);

        mc::variant key;
        entry_reader.read_variant_value(entry_reader.current_type(), key, depth + 1);

        mc::variant value;
        entry_reader.read_variant_value(entry_reader.current_type(), value, depth + 1);

        dict[key] = value;
        next();
    }
}

void message_reader::read_variant(mc::variant& v, std::size_t depth) const {
    ensure_type(DBUS_TYPE_VARIANT);

    message_reader v_reader;
    v_reader.recurse(*this);
    v_reader.read_variant_value(v_reader.current_type(), v, depth + 1);
}

void message_reader::read_variant_value(type_code type, mc::variant& v, std::size_t depth) const {
    ensure_message_depth(depth);

    switch (type) {
    case type_code::byte_type:
        return detail::demarshal_variant_basic<uint8_t>(*this, v);
    case type_code::boolean_type:
        return detail::demarshal_variant_basic<bool>(*this, v);
    case type_code::int16_type:
        return detail::demarshal_variant_basic<int16_t>(*this, v);
    case type_code::uint16_type:
        return detail::demarshal_variant_basic<uint16_t>(*this, v);
    case type_code::int32_type:
        return detail::demarshal_variant_basic<int32_t>(*this, v);
    case type_code::uint32_type:
        return detail::demarshal_variant_basic<uint32_t>(*this, v);
    case type_code::int64_type:
        return detail::demarshal_variant_basic<int64_t>(*this, v);
    case type_code::uint64_type:
        return detail::demarshal_variant_basic<uint64_t>(*this, v);
    case type_code::double_type:
        return detail::demarshal_variant_basic<double>(*this, v);
    case type_code::string_type:
        return detail::demarshal_variant_basic<std::string_view>(*this, v);
    case type_code::signature_type:
        return detail::demarshal_variant_basic<mc::dbus::signature>(*this, v);
    case type_code::object_path_type:
        return detail::demarshal_variant_basic<mc::dbus::path>(*this, v);
    case type_code::array_start:
        return read_variant_array_or_dict(v, depth + 1);
    case type_code::struct_start:
        return read_variant_struct(v, depth + 1);
    case type_code::variant_type: {
        return read_variant(v, depth + 1);
    }
    default:
        MC_THROW(mc::invalid_arg_exception, "unknown type: ${type}",
                 ("type", static_cast<char>(type)));
    }
}

/* -------------------- message_writer -------------------- */

message_writer::message_writer(message& msg) {
    dbus_message_iter_init_append(msg.get_dbus_message(), &m_iter);
}

message_writer::message_writer(DBusMessageIter& parent_iter, int type, std::string_view signature)
    : m_parent_iter(&parent_iter) {
    const char* sig = signature.empty() ? nullptr : signature.data();
    dbus_message_iter_open_container(m_parent_iter, type, sig, &m_iter);
}

void message_writer::close_container() {
    if (m_parent_iter) {
        dbus_message_iter_close_container(m_parent_iter, &m_iter);
        m_parent_iter = nullptr;
    }
}

const message_writer& operator<<(const message_writer& writer, const std::string& v) {
    const char* str = v.c_str();
    dbus_message_iter_append_basic(&writer.m_iter, DBUS_TYPE_STRING, &str);
    return writer;
}

const message_writer& operator<<(const message_writer& writer, const char* str) {
    dbus_message_iter_append_basic(&writer.m_iter, DBUS_TYPE_STRING, str);
    return writer;
}

const message_writer& operator<<(const message_writer& writer, const std::string_view& v) {
    const char* str = v.data();
    dbus_message_iter_append_basic(&writer.m_iter, DBUS_TYPE_STRING, &str);
    return writer;
}

const message_writer& operator<<(const message_writer& writer, const mc::dbus::path& v) {
    const char* str = v.str().c_str();
    dbus_message_iter_append_basic(&writer.m_iter, DBUS_TYPE_OBJECT_PATH, &str);
    return writer;
}

const message_writer& operator<<(const message_writer& writer, const mc::dbus::signature& v) {
    const char* str = v.str().c_str();
    dbus_message_iter_append_basic(&writer.m_iter, DBUS_TYPE_SIGNATURE, &str);
    return writer;
}

const message_writer& operator<<(const message_writer& writer, const mc::blob& v) {
    dbus_message_iter_append_fixed_array(&writer.m_iter, DBUS_TYPE_BYTE, v.data.data(),
                                         v.data.size());
    return writer;
}

const message_writer& operator<<(const message_writer& writer, const mc::variant& v) {
    writer.write_variant(v, 0);
    return writer;
}

const message_writer& operator<<(const message_writer& writer, const mc::variants& v) {
    writer.write_variant_array(container::array_of_variant.substr(1), v, 0);
    return writer;
}

const message_writer& operator<<(const message_writer& writer, const mc::dict& v) {
    writer.write_variant_dict(container::dict_string_var.substr(1), v, 0);
    return writer;
}

const message_writer& operator<<(const message_writer& writer, const mc::mutable_dict& v) {
    writer.write_variant_dict(container::dict_string_var.substr(1), v, 0);
    return writer;
}

void message_writer::write_variant(const mc::variant& v, std::size_t depth) const {
    signature sig;
    detail::variant_to_dbus_signature(sig, v);

    message_writer vit(m_iter, DBUS_TYPE_VARIANT, sig.str());
    vit.write_variant(sig, v, depth + 1);
    vit.close_container();
}

void message_writer::write_variant_value(const mc::variant& v) const {
    signature sig;
    detail::variant_to_dbus_signature(sig, v);

    write_variant(sig, v, 0);
}

void message_writer::write_variant(signature_iterator it, const mc::variant& v,
                                   std::size_t depth) const {
    ensure_message_depth(depth);

    switch (it.current_type_code()) {
    case type_code::byte_type:
        return detail::marshal_variant_basic<uint8_t>(*this, v);
    case type_code::boolean_type:
        return detail::marshal_variant_basic<bool>(*this, v);
    case type_code::int16_type:
        return detail::marshal_variant_basic<int16_t>(*this, v);
    case type_code::uint16_type:
        return detail::marshal_variant_basic<uint16_t>(*this, v);
    case type_code::int32_type:
        return detail::marshal_variant_basic<int32_t>(*this, v);
    case type_code::uint32_type:
        return detail::marshal_variant_basic<uint32_t>(*this, v);
    case type_code::int64_type:
        return detail::marshal_variant_basic<int64_t>(*this, v);
    case type_code::uint64_type:
        return detail::marshal_variant_basic<uint64_t>(*this, v);
    case type_code::double_type:
        return detail::marshal_variant_basic<double>(*this, v);
    case type_code::string_type:
        return detail::marshal_variant_basic<std::string_view>(*this, v);
    case type_code::signature_type:
        return detail::marshal_variant_basic<mc::dbus::signature>(*this, v);
    case type_code::object_path_type:
        return detail::marshal_variant_basic<mc::dbus::path>(*this, v);
    case type_code::array_start:
        return write_variant_array_or_dict(it.get_content_iterator(), v, depth + 1);
    case type_code::struct_start:
        return write_variant_struct(it.get_content_iterator(), v, depth + 1);
    case type_code::variant_type: {
        return write_variant(v, depth + 1);
    }
    default:
        MC_THROW(mc::invalid_arg_exception, "unknown type: ${type}",
                 ("type", it.current_type_char()));
    }
}

void message_writer::write_variant_array_or_dict(signature_iterator it, const mc::variant& v,
                                                 std::size_t depth) const {
    ensure_message_depth(depth);
    if (it.current_type_code() == type_code::dict_entry_start) {
        write_variant_dict(it, v.get_object(), depth + 1);
        return;
    }

    // null 类型用 std::optional<int32_t> 类型来表示
    if (v.get_type() == mc::type_id::null_type) {
        *this << std::optional<int32_t>();
        return;
    }

    write_variant_array(it, v.get_array(), depth + 1);
}

void message_writer::write_variant_array(signature_iterator it, const mc::variants& arr,
                                         std::size_t depth) const {
    ensure_message_depth(depth);
    ensure_container_max_length(arr);

    message_writer writer(m_iter, DBUS_TYPE_ARRAY, it.str());
    for (const auto& item : arr) {
        writer.write_variant(it, item, depth + 1);
    }
    writer.close_container();
}

void message_writer::write_variant_struct(signature_iterator it, const mc::variant& v,
                                          std::size_t depth) const {
    ensure_message_depth(depth);

    message_writer writer(m_iter, DBUS_TYPE_STRUCT);
    auto&          arr = v.get_array();
    for (const auto& item : arr) {
        signature_iterator item_it(it.current_type());
        MC_ASSERT(!item_it.at_end() && !it.at_end(), "结构体元素类型错误 ${type}, ${pos}",
                  ("type", it.str())("pos", it.str().substr(item_it.pos())));

        writer.write_variant(item_it, item, depth + 1);
        it.next();
    }

    writer.close_container();
    MC_ASSERT(it.at_end(), "结构体元素数量错误, ${type}, ${pos}",
              ("type", it.str())("pos", it.str().substr(it.pos())));
}

void message_writer::write_variant_dict(signature_iterator it, const mc::dict& dict,
                                        std::size_t depth) const {
    ensure_message_depth(depth);
    ensure_container_max_length(dict);

    message_writer writer(m_iter, DBUS_TYPE_ARRAY, it.str());

    auto key_it = it.get_dict_key_iterator();
    auto val_it = it.get_dict_value_iterator();
    for (const auto& entry : dict) {
        message_writer entry_writer(writer.m_iter, DBUS_TYPE_DICT_ENTRY);
        entry_writer.write_variant(key_it, entry.key, depth + 1);
        entry_writer.write_variant(val_it, entry.value, depth + 1);
        entry_writer.close_container();
    }

    writer.close_container();
}

void message_writer::write_signature(const signature& sig) const {
    write_signature(sig.str());
}

void message_writer::write_signature(std::string_view sig) const {
    dbus_message_iter_append_basic(&m_iter, DBUS_TYPE_SIGNATURE, sig.data());
}

} // namespace mc::dbus