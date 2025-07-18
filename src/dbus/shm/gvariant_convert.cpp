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

#include <mc/dbus/message.h>
#include <mc/dbus/shm/gvariant_convert.h>
#include <mc/exception.h>
#include <mc/log.h>
#include <mc/reflect.h>

namespace mc::dbus {

static bool gvariant_is_basic_type(int8_t type) {
    switch (type) {
    case DBUS_TYPE_BOOLEAN:
    case DBUS_TYPE_BYTE:
    case DBUS_TYPE_INT16:
    case DBUS_TYPE_UINT16:
    case DBUS_TYPE_INT32:
    case DBUS_TYPE_UINT32:
    case DBUS_TYPE_UNIX_FD:
    case DBUS_TYPE_INT64:
    case DBUS_TYPE_UINT64:
    case DBUS_TYPE_DOUBLE:
    case DBUS_TYPE_STRING:
    case DBUS_TYPE_OBJECT_PATH:
    case DBUS_TYPE_SIGNATURE:
        return true;
    default:
        return false;
    }
}

static bool gvariant_is_fixed_type(int8_t type) {
    switch (type) {
    case DBUS_TYPE_BOOLEAN:
    case DBUS_TYPE_BYTE:
    case DBUS_TYPE_INT16:
    case DBUS_TYPE_UINT16:
    case DBUS_TYPE_INT32:
    case DBUS_TYPE_UINT32:
    case DBUS_TYPE_INT64:
    case DBUS_TYPE_UINT64:
    case DBUS_TYPE_DOUBLE:
        return true;
    default:
        return false;
    }
}

static GVariant* new_basic_gvariant(const variant& v, const char* types) {
    switch (*types) {
    case DBUS_TYPE_BOOLEAN:
        return g_variant_new_boolean(v.as_bool());
    case DBUS_TYPE_BYTE:
        return g_variant_new_byte(v.as_uint8());
    case DBUS_TYPE_INT16:
        return g_variant_new_int16(v.as_int16());
    case DBUS_TYPE_UINT16:
        return g_variant_new_uint16(v.as_uint16());
    case DBUS_TYPE_INT32:
        return g_variant_new_int32(v.as_int32());
    case DBUS_TYPE_UINT32:
        return g_variant_new_uint32(v.as_uint32());
    case DBUS_TYPE_INT64:
        return g_variant_new_int64(v.as_int64());
    case DBUS_TYPE_UINT64:
        return g_variant_new_uint64(v.as_uint64());
    case DBUS_TYPE_DOUBLE:
        return g_variant_new_double(v.as_double());
    case DBUS_TYPE_STRING:
    case DBUS_TYPE_OBJECT_PATH:
    case DBUS_TYPE_SIGNATURE: {
        auto str = v.as_string();
        if (!g_utf8_validate(str.data(), str.size(), nullptr)) {
            MC_THROW(mc::invalid_arg_exception, "invalid utf-8 string");
        }
        return g_variant_new_string(str.data());
    }
    default:
        MC_THROW(mc::invalid_arg_exception, "unsupported type: ${type}", ("type", types));
    }
}

int sig_unit::get_dict_len(const char* types, bool allow_dict_entry, size_t array_depth,
                           size_t struct_depth) {
    if (!allow_dict_entry) {
        MC_THROW(mc::invalid_arg_exception, "dict entry not allowed");
    }
    if (struct_depth >= MAX_CONTAINER_DEPTH) {
        MC_THROW(mc::invalid_arg_exception, "struct depth too deep");
    }
    int         cnt = 0;
    const char* p   = types + 1;
    while (*p != DBUS_DICT_ENTRY_END_CHAR) {
        if (cnt == 0 && !gvariant_is_basic_type(*p)) {
            MC_THROW(mc::invalid_arg_exception, "invalid dict entry");
        }
        p += get_sig_len(p, false, array_depth, struct_depth + 1);
        cnt++;
    }
    if (cnt != 2) {
        MC_THROW(mc::invalid_arg_exception, "invalid key value pair");
    }
    return p - types + 1;
}

int sig_unit::get_struct_len(const char* types, bool allow_dict_entry, size_t array_depth,
                             size_t struct_depth) {
    if (struct_depth >= MAX_CONTAINER_DEPTH) {
        MC_THROW(mc::invalid_arg_exception, "struct depth too deep");
    }
    const char* p = types + 1;
    while (*p != DBUS_STRUCT_END_CHAR) {
        p += get_sig_len(p, false, array_depth, struct_depth + 1);
    }
    if (p - types < 2) {
        MC_THROW(mc::invalid_arg_exception, "empty struct");
    }
    return p - types + 1;
}

int sig_unit::get_sig_len(const char* types, bool allow_dict_entry, size_t array_depth,
                          size_t struct_depth) {
    char type = *types;
    if (gvariant_is_basic_type(type) || type == DBUS_TYPE_VARIANT) {
        return 1;
    }
    switch (type) {
    case DBUS_TYPE_ARRAY:
        if (array_depth >= MAX_CONTAINER_DEPTH) {
            MC_THROW(mc::invalid_arg_exception, "array depth too deep");
        }
        return get_sig_len(types + 1, true, array_depth + 1, struct_depth) + 1;
    case DBUS_DICT_ENTRY_BEGIN_CHAR:
        return get_dict_len(types, allow_dict_entry, array_depth, struct_depth);
    case DBUS_STRUCT_BEGIN_CHAR:
        return get_struct_len(types, allow_dict_entry, array_depth, struct_depth);
    default:
        MC_THROW(mc::invalid_arg_exception, "unsupported type: ${type}", ("type", type));
    }
}

static void parse_signature(const char* types, sig_unit& sig) {
    int len = sig_unit::get_sig_len(types, true, 0, 0);
    if (len <= 0 || len >= MAX_SIG_LEN) {
        MC_THROW(mc::invalid_arg_exception, "invalid signature length");
    }
    sig.next_types = types + len;
    sig.type       = *types;
    sig.len        = len;
    std::memcpy(sig.buf, types, len);
    sig.buf[len]  = '\0';
    sig.sub_types = sig.buf + 1;
}

GVariant* gvariant_convert::new_gvariant_dict(const variant& v, sig_unit& sig) {
    auto            d         = v.as_dict();
    const char*     key_sig   = sig.sub_types + 1;
    const char*     value_sig = key_sig + 1;
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE(sig.buf));
    for (const auto& entry : d) {
        auto key_gvariant   = to_gvariant(entry.key, key_sig);
        auto value_gvariant = to_gvariant(entry.value, value_sig);
        auto dict_entry =
            g_variant_new_dict_entry(g_variant_ref(key_gvariant), g_variant_ref(value_gvariant));
        g_variant_builder_add_value(&builder, g_variant_ref(dict_entry));
    }
    return g_variant_builder_end(&builder);
}

GVariant* gvariant_convert::new_gvariant_struct(const variant& v, sig_unit& sig) {
    const char* types = sig.sub_types;
    if (types == nullptr) {
        MC_THROW(mc::invalid_arg_exception, "types is nullptr");
    }
    auto   arr = v.as_array();
    size_t len = arr.size();
    if (len > MAX_CONTAINER_SIZE) {
        MC_THROW(mc::invalid_arg_exception, "struct size too large");
    }
    std::vector<GVariant*> buf;
    buf.resize(len);

    size_t i = 0;
    for (; i < len && *types != '\0'; i++) {
        auto pair = to_gvariant_inner(arr[i], types);
        buf[i]    = g_variant_ref(std::get<0>(pair));
        types     = std::get<1>(pair);
    }
    if (*types != '\0' && i < len) {
        MC_THROW(mc::invalid_arg_exception,
                 "invalid number of elements ${len} for struct: ${types}",
                 ("len", len)("types", std::string(types)));
    }
    return g_variant_new_tuple(reinterpret_cast<GVariant* const*>(&buf[0]), i);
}

static size_t get_element_size(int type) {
    switch (type) {
    case DBUS_TYPE_BOOLEAN:
    case DBUS_TYPE_BYTE:
        return sizeof(DBusBasicValue::byt);
    case DBUS_TYPE_INT16:
        return sizeof(DBusBasicValue::i16);
    case DBUS_TYPE_UINT16:
        return sizeof(DBusBasicValue::u16);
    case DBUS_TYPE_INT32:
        return sizeof(DBusBasicValue::i32);
    case DBUS_TYPE_UINT32:
        return sizeof(DBusBasicValue::u32);
    case DBUS_TYPE_INT64:
        return sizeof(DBusBasicValue::i64);
    case DBUS_TYPE_UINT64:
        return sizeof(DBusBasicValue::u64);
    case DBUS_TYPE_DOUBLE:
        return sizeof(DBusBasicValue::dbl);
    default:
        MC_THROW(mc::exception, "unsupported element type: ${type}", ("type", type));
    }
}

static void set_basic_value(DBusBasicValue& value, int type, const variant& v) {
    switch (type) {
    case DBUS_TYPE_BOOLEAN:
        value.bool_val = v.as_bool();
        break;
    case DBUS_TYPE_BYTE:
        value.byt = v.as_uint8();
        break;
    case DBUS_TYPE_INT16:
        value.i16 = v.as_int16();
        break;
    case DBUS_TYPE_UINT16:
        value.u16 = v.as_uint16();
        break;
    case DBUS_TYPE_INT32:
        value.i32 = v.as_int32();
        break;
    case DBUS_TYPE_UINT32:
        value.u32 = v.as_uint32();
        break;
    case DBUS_TYPE_INT64:
        value.i64 = v.as_int64();
        break;
    case DBUS_TYPE_UINT64:
        value.u64 = v.as_uint64();
        break;
    case DBUS_TYPE_DOUBLE:
        value.dbl = v.as_double();
        break;
    default:
        MC_THROW(mc::invalid_arg_exception, "unsupported element type: ${type}", ("type", type));
    }
}

static GVariant* new_gvariant_fixed_array(const variant& v, const char* types) {
    auto   arr          = v.as_array();
    size_t len          = arr.size();
    size_t element_size = get_element_size(types[0]);
    if (len == 0) {
        return g_variant_new_fixed_array(reinterpret_cast<const GVariantType*>(types), nullptr, 0,
                                         element_size);
    }
    void* data = g_malloc0(len * element_size);
    if (data == nullptr) {
        MC_THROW(mc::exception, "g_malloc0 failed, len: ${len}, element_size: ${element_size}",
                 ("len", len)("element_size", element_size));
    }
    uint8_t* p = reinterpret_cast<uint8_t*>(data);
    for (auto& item : arr) {
        DBusBasicValue value;
        std::memset(&value, 0, sizeof(value));
        set_basic_value(value, types[0], item);
        std::memcpy(p, &value, element_size);
        p += element_size;
    }
    return g_variant_new_fixed_array(reinterpret_cast<const GVariantType*>(types), data, len,
                                     element_size);
}

GVariant* gvariant_convert::new_gvariant_array(const variant& v, sig_unit& sig) {
    const char* types = sig.sub_types;
    if (types == nullptr) {
        MC_THROW(mc::exception, "types is nullptr");
    }
    if (gvariant_is_fixed_type(*types)) {
        if (*types == DBUS_TYPE_BYTE && v.is_string()) {
            auto str         = v.as_string();
            auto fixed_array = g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE, str.data(),
                                                         str.size(), sizeof(char));
            return fixed_array;
        }
        return new_gvariant_fixed_array(v, types);
    }
    auto            arr = v.as_array();
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE(sig.buf));
    for (auto& item : arr) {
        auto child = to_gvariant(item, types);
        g_variant_builder_add_value(&builder, g_variant_ref(child));
    }
    return g_variant_builder_end(&builder);
}

GVariant* gvariant_convert::to_gvariant(const variant& v, const char* types) {
    return std::get<0>(to_gvariant_inner(v, types));
}

std::tuple<GVariant*, const char*> gvariant_convert::to_gvariant_inner(const variant& v,
                                                                       const char*    types) {
    if (types == nullptr) {
        MC_THROW(mc::exception, "types is nullptr");
    }
    if (gvariant_is_basic_type(*types)) {
        return std::make_tuple(new_basic_gvariant(v, types), types + 1);
    }
    sig_unit sig;
    parse_signature(types, sig);
    if (*types != DBUS_TYPE_VARIANT && !sig.sub_types_is_valid()) {
        MC_THROW(mc::invalid_arg_exception, "invalid signature: ${types}", ("types", types));
    }
    GVariant* res = nullptr;
    switch (*types) {
    case DBUS_TYPE_ARRAY:
        if (sig.sub_types[0] == DBUS_DICT_ENTRY_BEGIN_CHAR) {
            res = new_gvariant_dict(v, sig);
        } else {
            res = new_gvariant_array(v, sig);
        }
        break;
    case DBUS_STRUCT_BEGIN_CHAR:
        res = new_gvariant_struct(v, sig);
        break;
    case DBUS_TYPE_VARIANT:
        res = g_variant_new_variant(to_gvariant(v));
        break;
    default:
        MC_THROW(mc::invalid_arg_exception, "unsupported type: ${types}", ("types", types));
    }
    return std::make_tuple(res, sig.next_types);
}

dict gvariant_convert::dict_to_mc_variant(GVariant* value, int n) {
    mutable_dict d;
    for (int i = 0; i < n; i++) {
        auto entry = g_variant_get_child_value(value, i);
        auto key   = to_mc_variant(g_variant_get_child_value(entry, 0));
        auto value = to_mc_variant(g_variant_get_child_value(entry, 1));
        d[key]     = value;
    }
    return d;
}

variants gvariant_convert::array_to_mc_variant(GVariant* value, int n) {
    variants res;
    for (int i = 0; i < n; i++) {
        auto child = g_variant_get_child_value(value, i);
        auto v     = to_mc_variant(child);
        res.push_back(v);
    }
    return res;
}

variant gvariant_convert::container_to_mc_variant(GVariant* value) {
    if (g_variant_is_of_type(value, G_VARIANT_TYPE_BYTESTRING)) {
        gsize       len = 0;
        const char* data =
            reinterpret_cast<const char*>(g_variant_get_fixed_array(value, &len, sizeof(char)));
        return variant(std::string(data, len));
    }
    variants res;
    auto     n = g_variant_n_children(value);
    if (n == 0) {
        return res;
    }
    auto        child = g_variant_get_child_value(value, 0);
    const char* type  = g_variant_get_type_string(child);
    if (type[0] == DBUS_DICT_ENTRY_BEGIN_CHAR) {
        return dict_to_mc_variant(value, n);
    }
    return array_to_mc_variant(value, n);
}

variant gvariant_convert::to_mc_variant(GVariant* value) {
    const char* type = g_variant_get_type_string(value);
    switch (type[0]) {
    case DBUS_TYPE_BOOLEAN:
        return variant(static_cast<bool>(g_variant_get_boolean(value)));
    case DBUS_TYPE_BYTE:
        return variant(static_cast<uint8_t>(g_variant_get_byte(value)));
    case DBUS_TYPE_INT16:
        return variant(static_cast<int16_t>(g_variant_get_int16(value)));
    case DBUS_TYPE_UINT16:
        return variant(static_cast<uint16_t>(g_variant_get_uint16(value)));
    case DBUS_TYPE_INT32:
        return variant(static_cast<int32_t>(g_variant_get_int32(value)));
    case DBUS_TYPE_UINT32:
        return variant(static_cast<uint32_t>(g_variant_get_uint32(value)));
    case DBUS_TYPE_INT64:
        return variant(static_cast<int64_t>(g_variant_get_int64(value)));
    case DBUS_TYPE_UINT64:
        return variant(static_cast<uint64_t>(g_variant_get_uint64(value)));
    case DBUS_TYPE_DOUBLE:
        return variant(static_cast<double>(g_variant_get_double(value)));
    case DBUS_TYPE_STRING:
        return variant(std::string(g_variant_get_string(value, nullptr)));
    case DBUS_TYPE_VARIANT:
        return to_mc_variant(g_variant_get_child_value(value, 0));
    default:
        if (g_variant_is_container(value)) {
            return container_to_mc_variant(value);
        }
        MC_THROW(mc::invalid_arg_exception, "unsupported type: ${type}", ("type", type));
    }
}

GVariant* gvariant_convert::to_gvariant(const variant& v) {
    signature sig;
    detail::variant_to_dbus_signature(sig, v);
    std::string sig_str = sig.str();
    return to_gvariant(v, sig_str.c_str());
}

} // namespace mc::dbus
