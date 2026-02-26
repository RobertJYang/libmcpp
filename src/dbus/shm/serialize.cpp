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
#include <mc/dbus/shm/gvariant_convert.h>
#include <mc/dbus/shm/serialize.h>
#include <mc/dict.h>
#include <mc/exception.h>
#include <mc/variant.h>

namespace mc::dbus::serialize {

inline static uint8_t pack_value(data_type t, uint8_t value) {
    return static_cast<uint8_t>(t) | (value << 3);
}

write_buffer::write_buffer()
    : m_head(new data_block()), m_current(m_head), m_len(0), m_offset(0) {
}

write_buffer::~write_buffer() {
    data_block* node = m_head;
    while (node) {
        data_block* next = node->next;
        delete node;
        node = next;
    }
}

void write_buffer::write_arg(const variant& arg, int depth) {
    MC_ASSERT(depth <= MAX_DEPTH, "depth exceeds limit");
    if (arg.is_null()) {
        write_nil();
        return;
    }
    if (arg.is_bool()) {
        write_boolean(arg.as_bool());
        return;
    }
    if (arg.is_numeric()) {
        write_number(arg);
        return;
    }
    if (arg.is_string() || arg.is_blob()) {
        write_string(arg.as_string());
        return;
    }
    if (arg.is_array()) {
        write_array(arg.as_array(), depth + 1);
        return;
    }
    if (arg.is_dict()) {
        write_dict(arg.as_dict(), depth + 1);
        return;
    }
    MC_THROW(mc::invalid_arg_exception, "unsupported variant type: ${type_id}",
             ("type_id", arg.get_type()));
}

void write_buffer::write_arg_with_signature(signature_iterator it, const variant& arg, int depth) {
    ensure_message_depth(depth);
    if (arg.is_null()) {
        write_nil();
        return;
    }
    switch (it.current_type_code()) {
    case type_code::boolean_type:
        write_boolean(arg.as_bool());
        break;
    case type_code::byte_type:
    case type_code::int16_type:
    case type_code::uint16_type:
    case type_code::int32_type:
    case type_code::uint32_type:
    case type_code::int64_type:
    case type_code::uint64_type:
    case type_code::double_type:
        write_number(arg);
        break;
    case type_code::string_type:
    case type_code::signature_type:
    case type_code::object_path_type:
        write_string(arg.as_string());
        break;
    case type_code::array_start:
        write_array_or_dict(it.get_content_iterator(), arg, depth + 1);
        break;
    case type_code::struct_start:
        write_variant_elements(it.get_content_iterator(), arg.as_array(), depth + 1);
        break;
    case type_code::variant_type:
        write_gvariant(arg);
        break;
    default:
        MC_THROW(mc::invalid_arg_exception, "unsupported type for write_arg_with_signature: ${type}",
                 ("type", it.current_type_char()));
    }
}

void write_buffer::write_array_or_dict(signature_iterator it, const variant& arg, int depth) {
    ensure_message_depth(depth);
    if (it.current_type_code() == type_code::dict_entry_start) {
        if (arg.is_array() && arg.get_array().empty()) {
            // 兼容lua数据类型，允许空数组作为空字典处理
            write_dict(it, mc::dict(), depth + 1);
        } else {
            write_dict(it, arg.get_object(), depth + 1);
        }
        return;
    }
    if (arg.is_null()) {
        write_nil();
        return;
    }
    if (it.current_type_code() == type_code::byte_type) {
        // ay类型当作字符串处理，与lua框架保持兼容
        auto data = arg.as<std::vector<uint8_t>>();
        std::string str(data.begin(), data.end());
        write_string(str);
        return;
    }
    write_array(it, arg.as_array(), depth + 1);
}

void write_buffer::write_inner(const uint8_t* input, size_t size) {
    if (m_offset == BLOCK_SIZE) {
        m_current->next = new data_block();
        m_current       = m_current->next;
        m_offset        = 0;
    }
    size_t remaining_size = size;
    while (remaining_size > 0) {
        if (remaining_size <= BLOCK_SIZE - m_offset) {
            memcpy(m_current->buf.data() + m_offset, input, remaining_size);
            m_offset += remaining_size;
            m_len += remaining_size;
            break;
        }
        size_t copy_size = BLOCK_SIZE - m_offset;
        memcpy(m_current->buf.data() + m_offset, input, copy_size);
        input += copy_size;
        remaining_size -= copy_size;
        m_len += copy_size;
        m_current->next = new data_block();
        m_current       = m_current->next;
        m_offset        = 0;
    }
}

void write_buffer::write_nil() {
    uint8_t n = static_cast<uint8_t>(data_type::nil);
    write_inner(&n, 1);
}

void write_buffer::write_boolean(bool value) {
    uint8_t n = pack_value(data_type::boolean, value ? 1 : 0);
    write_inner(&n, 1);
}

void write_buffer::write_double(double value) {
    uint8_t n = pack_value(data_type::number, COOKIE_NUMBER_REAL);
    write_inner(&n, 1);
    write_inner(reinterpret_cast<const uint8_t*>(&value), sizeof(value));
}

void write_buffer::write_integer(int64_t value) {
    auto type = data_type::number;
    if (value == 0) {
        uint8_t n = pack_value(type, COOKIE_NUMBER_ZERO);
        write_inner(&n, 1);
    } else if (value != static_cast<int32_t>(value)) {
        uint8_t n = pack_value(type, COOKIE_NUMBER_QWORD);
        write_inner(&n, 1);
        write_inner(reinterpret_cast<const uint8_t*>(&value), sizeof(value));
    } else if (value < 0) {
        uint8_t n = pack_value(type, COOKIE_NUMBER_DWORD);
        write_inner(&n, 1);
        int32_t v32 = static_cast<int32_t>(value);
        write_inner(reinterpret_cast<const uint8_t*>(&v32), sizeof(v32));
    } else if (value < 0x100) {
        uint8_t n = pack_value(type, COOKIE_NUMBER_BYTE);
        write_inner(&n, 1);
        uint8_t byte = static_cast<uint8_t>(value);
        write_inner(&byte, sizeof(byte));
    } else if (value < 0x10000) {
        uint8_t n = pack_value(type, COOKIE_NUMBER_WORD);
        write_inner(&n, 1);
        uint16_t word = static_cast<uint16_t>(value);
        write_inner(reinterpret_cast<const uint8_t*>(&word), sizeof(word));
    } else {
        uint8_t n = pack_value(type, COOKIE_NUMBER_DWORD);
        write_inner(&n, 1);
        uint32_t v32 = static_cast<uint32_t>(value);
        write_inner(reinterpret_cast<const uint8_t*>(&v32), sizeof(v32));
    }
}

void write_buffer::write_number(const variant& arg) {
    if (arg.is_double()) {
        write_double(arg.as_double());
        return;
    }
    if (arg.is_uint64() &&
        arg.as_uint64() > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
        // lua中整数最大为INT64_MAX，超过这个值的整数需要转换为double类型
        write_double(arg.as_double());
    } else {
        write_integer(arg.as_int64());
    }
}

void write_buffer::write_string(std::string_view value) {
    const char* str = value.data();
    size_t      len = value.size();
    if (len < MAX_COOKIE) {
        uint8_t n = pack_value(data_type::short_string, len);
        write_inner(&n, 1);
        if (len > 0) {
            write_inner(reinterpret_cast<const uint8_t*>(str), len);
        }
    } else {
        uint8_t n;
        if (len < 0x10000) {
            n = pack_value(data_type::long_string, 2);
            write_inner(&n, 1);
            uint16_t x = static_cast<uint16_t>(len);
            write_inner(reinterpret_cast<const uint8_t*>(&x), sizeof(x));
        } else {
            n = pack_value(data_type::long_string, 4);
            write_inner(&n, 1);
            uint32_t x = static_cast<uint32_t>(len);
            write_inner(reinterpret_cast<const uint8_t*>(&x), sizeof(x));
        }
        write_inner(reinterpret_cast<const uint8_t*>(str), len);
    }
}

void write_buffer::write_array(const variants& arg, int depth) {
    size_t array_size = arg.size();
    if (array_size >= MAX_COOKIE - 1) {
        uint8_t n = pack_value(data_type::table, MAX_COOKIE - 1);
        write_inner(&n, 1);
        write_integer(array_size);
    } else {
        uint8_t n = pack_value(data_type::table, array_size);
        write_inner(&n, 1);
    }
    for (const auto& item : arg) {
        write_arg(item, depth);
    }
    write_nil();
}

void write_buffer::write_array(signature_iterator it, const variants& arg, int depth) {
    ensure_message_depth(depth);
    ensure_container_max_length(arg);
    size_t array_size = arg.size();
    if (array_size >= MAX_COOKIE - 1) {
        uint8_t n = pack_value(data_type::table, MAX_COOKIE - 1);
        write_inner(&n, 1);
        write_integer(array_size);
    } else {
        uint8_t n = pack_value(data_type::table, array_size);
        write_inner(&n, 1);
    }
    for (const auto& item : arg) {
        write_arg_with_signature(it, item, depth + 1);
    }
    write_nil();
}

void write_buffer::write_variant_elements(signature_iterator it, const variants& args, int depth) {
    ensure_message_depth(depth);
    ensure_container_max_length(args);
    size_t array_size = args.size();
    if (array_size >= MAX_COOKIE - 1) {
        uint8_t n = pack_value(data_type::table, MAX_COOKIE - 1);
        write_inner(&n, 1);
        write_integer(array_size);
    } else {
        uint8_t n = pack_value(data_type::table, array_size);
        write_inner(&n, 1);
    }
    for (const auto& item : args) {
        signature_iterator item_it(it.current_type());
        MC_ASSERT(!item_it.at_end() && !it.at_end(),
                  "invalid number of elements ${size} for signature: ${signature}",
                  ("size", args.size())("signature", it.str()));
        write_arg_with_signature(item_it, item, depth + 1);
        it.next();
    }
    write_nil();
}

void write_buffer::write_dict(const dict& arg, int depth) {
    uint8_t n = pack_value(data_type::table, 0);
    write_inner(&n, 1);
    for (const auto& item : arg) {
        write_arg(item.key, depth);
        write_arg(item.value, depth);
    }
    write_nil();
}

void write_buffer::write_dict(signature_iterator it, const dict& arg, int depth) {
    ensure_message_depth(depth);
    ensure_container_max_length(arg);
    uint8_t n = pack_value(data_type::table, 0);
    write_inner(&n, 1);
    auto key_it = it.get_dict_key_iterator();
    auto val_it = it.get_dict_value_iterator();
    for (const auto& entry : arg) {
        write_arg_with_signature(key_it, entry.key, depth + 1);
        write_arg_with_signature(val_it, entry.value, depth + 1);
    }
    write_nil();
}

void write_buffer::write_gvariant(const variant& arg) {
    if (arg.is_null()) {
        write_nil();
        return;
    }
    gvariant_auto_free gvar(gvariant_convert::to_gvariant(arg));
    const char*        t   = g_variant_get_type_string(gvar.ptr);
    size_t             len = strlen(t);
    MC_ASSERT(len < MAX_COOKIE, "invalid gvariant type: ${type}", ("type", t));
    uint8_t n = pack_value(data_type::gvariant, len);
    write_inner(&n, 1);
    write_inner(reinterpret_cast<const uint8_t*>(t), len + 1);
    int         size = g_variant_get_size(gvar.ptr);
    const void* data = g_variant_get_data(gvar.ptr);
    write_inner(reinterpret_cast<const uint8_t*>(&size), sizeof(int));
    write_inner(reinterpret_cast<const uint8_t*>(data), size);
}

std::string write_buffer::to_string() const {
    uint8_t*    buffer         = static_cast<uint8_t*>(malloc(m_len));
    uint8_t*    ptr            = buffer;
    size_t      remaining_size = m_len;
    data_block* node           = m_head;
    while (remaining_size > 0) {
        if (remaining_size < BLOCK_SIZE) {
            memcpy(ptr, node->buf.data(), remaining_size);
            break;
        }
        memcpy(ptr, node->buf.data(), BLOCK_SIZE);
        ptr += BLOCK_SIZE;
        remaining_size -= BLOCK_SIZE;
        node = node->next;
    }
    std::string result(reinterpret_cast<char*>(buffer), m_len);
    free(buffer);
    return result;
}

read_buffer::read_buffer(std::string_view msg)
    : m_buf(msg), m_offset(0) {
}

const char* read_buffer::read(size_t size) {
    if (size > m_buf.size() - m_offset) {
        return nullptr;
    }
    const char* ptr = m_buf.data() + m_offset;
    m_offset += size;
    return ptr;
}

int64_t read_buffer::read_integer(uint8_t cookie) {
    switch (cookie) {
    case COOKIE_NUMBER_ZERO:
        return 0;
    case COOKIE_NUMBER_BYTE: {
        uint8_t n;
        auto    pn = reinterpret_cast<const uint8_t*>(read(sizeof(n)));
        MC_ASSERT(pn != nullptr, ERROR_INVALID_FORMAT);
        n = *pn;
        return n;
    }
    case COOKIE_NUMBER_WORD: {
        uint16_t n;
        auto     pn = read(sizeof(n));
        MC_ASSERT(pn != nullptr, ERROR_INVALID_FORMAT);
        memcpy(&n, pn, sizeof(n));
        return n;
    }
    case COOKIE_NUMBER_DWORD: {
        int32_t n;
        auto    pn = read(sizeof(n));
        MC_ASSERT(pn != nullptr, ERROR_INVALID_FORMAT);
        memcpy(&n, pn, sizeof(n));
        return n;
    }
    case COOKIE_NUMBER_QWORD: {
        int64_t n;
        auto    pn = read(sizeof(n));
        MC_ASSERT(pn != nullptr, ERROR_INVALID_FORMAT);
        memcpy(&n, pn, sizeof(n));
        return n;
    }
    default:
        break;
    }
    return 0;
}

double read_buffer::read_double() {
    double n;
    auto   pn = read(sizeof(n));
    MC_ASSERT(pn != nullptr, ERROR_INVALID_FORMAT);
    memcpy(&n, pn, sizeof(n));
    return n;
}

void* read_buffer::read_pointer() {
    void* userdata = 0;
    auto  v        = read(sizeof(userdata));
    MC_ASSERT(v != nullptr, ERROR_INVALID_FORMAT);
    memcpy(&userdata, v, sizeof(userdata));
    return userdata;
}

std::string read_buffer::read_string(size_t len) {
    const char* str = read(len);
    MC_ASSERT(str != nullptr, ERROR_INVALID_FORMAT);
    return std::string(str, len);
}

variant read_buffer::read_inner() {
    uint8_t type;
    auto    t = reinterpret_cast<const uint8_t*>(read(sizeof(type)));
    MC_ASSERT(t != nullptr, ERROR_INVALID_FORMAT);
    type = *t;
    return read_value(type & TYPE_MASK, type >> VALUE_SHIFT);
}

variant read_buffer::read_value(uint8_t type, uint8_t cookie) {
    if (type >= static_cast<uint8_t>(data_type::tail)) {
        MC_THROW(mc::invalid_arg_exception, "unsupported type: ${type}", ("type", type));
    }
    auto value_type = static_cast<data_type>(type);
    switch (value_type) {
    case data_type::nil:
        return variant();
    case data_type::boolean:
        return variant(cookie != 0);
    case data_type::number:
        if (cookie == COOKIE_NUMBER_REAL) {
            return variant(read_double());
        }
        return variant(read_integer(cookie));
    case data_type::userdata:
        return variant(reinterpret_cast<uint64_t>(read_pointer()));
    case data_type::short_string:
        return variant(read_string(cookie));
    case data_type::long_string:
        if (cookie == 2) {
            const void* plen = read(2);
            MC_ASSERT(plen != nullptr, ERROR_INVALID_FORMAT);
            uint16_t n;
            memcpy(&n, plen, sizeof(n));
            return variant(read_string(n));
        } else {
            if (cookie != 4) {
                MC_THROW(mc::invalid_arg_exception, "invalid cookie: ${cookie}",
                         ("cookie", cookie));
            }
            const void* plen = read(4);
            MC_ASSERT(plen != nullptr, ERROR_INVALID_FORMAT);
            uint32_t n;
            memcpy(&n, plen, sizeof(n));
            return variant(read_string(n));
        }
    case data_type::table:
        return read_table(cookie);
    case data_type::gvariant:
        return read_gvariant(cookie);
    default:
        break;
    }
    MC_THROW(mc::invalid_arg_exception, "unsupported type: ${type}", ("type", type));
}

variant read_buffer::read_gvariant(size_t len) {
    const char* t = read(len + 1);
    MC_ASSERT(t != nullptr, ERROR_INVALID_FORMAT);
    int         size   = *reinterpret_cast<const int*>(read(sizeof(int)));
    const char* data   = read(size);
    void*       p_data = nullptr;
    if (size > 0) {
        p_data = g_malloc0(size);
        if (!p_data) {
            MC_THROW(mc::exception, "failed to allocate memory for p_data");
        }
        memcpy(p_data, data, size);
    }
    gvariant_auto_free gvar(g_variant_new_from_data(G_VARIANT_TYPE(t), p_data, size,
                                                    true, g_free, p_data));
    if (!gvar.ptr) {
        if (p_data) {
            g_free(p_data);
        }
        MC_THROW(mc::exception, "failed to create gvariant");
    }
    return gvariant_convert::to_mc_variant(gvar.ptr);
}

variant read_buffer::read_table(int64_t array_size) {
    if (array_size == MAX_COOKIE - 1) {
        uint8_t type;
        auto    t = reinterpret_cast<const uint8_t*>(read(sizeof(type)));
        MC_ASSERT(t != nullptr, ERROR_INVALID_FORMAT);
        type           = *t;
        uint8_t cookie = type >> VALUE_SHIFT;
        MC_ASSERT((type & TYPE_MASK) == static_cast<uint8_t>(data_type::number) &&
                      cookie != COOKIE_NUMBER_REAL,
                  ERROR_INVALID_FORMAT);
        array_size = read_integer(cookie);
    }
    variants arr;
    if (array_size > 0) {
        for (int64_t i = 1; i <= array_size; i++) {
            arr.push_back(read_inner());
        }
    }
    auto key = read_inner();
    if (key.is_null()) {
        return arr;
    }
    mc::dict d;
    for (int64_t i = 1; i <= array_size; i++) {
        variant index(i);
        d[index] = arr[i - 1];
    }
    while (true) {
        auto value = read_inner();
        d[key]     = value;
        key        = read_inner();
        if (key.is_null()) {
            break;
        }
    }
    return d;
}

std::string pack(const variants& args) {
    write_buffer wb;
    for (const auto& arg : args) {
        wb.write_arg(arg, 0);
    }
    return wb.to_string();
}

variants unpack(std::string_view msg) {
    variants    v;
    read_buffer rb(msg);
    while (true) {
        uint8_t type = 0;
        auto    t    = reinterpret_cast<const uint8_t*>(rb.read(sizeof(type)));
        if (t == nullptr) {
            break;
        }
        type = *t;
        v.push_back(rb.read_value(type & TYPE_MASK, type >> VALUE_SHIFT));
    }
    return v;
}

std::string serialize(const variants& args) {
    std::string data = pack(args);
    // 长度头存储含头部自身的总字节数，小端序写入
    uint32_t    sz = static_cast<uint32_t>(4 + data.size());
    std::string result(sz, '\0');
    result[0] = static_cast<char>(sz & 0xff);
    result[1] = static_cast<char>((sz >> 8) & 0xff);
    result[2] = static_cast<char>((sz >> 16) & 0xff);
    result[3] = static_cast<char>((sz >> 24) & 0xff);
    memcpy(result.data() + 4, data.data(), data.size());
    return result;
}

variants deserialize(std::string_view msg) {
    // 跳过前4字节的长度头，直接处理序列化数据
    if (msg.size() < 4) {
        MC_THROW(mc::invalid_arg_exception, "invalid message format: size too small");
    }
    
    // 跳过长度头，处理实际的序列化数据
    std::string_view data = msg.substr(4);
    return unpack(data);
}

} // namespace mc::dbus::serialize