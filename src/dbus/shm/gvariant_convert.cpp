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

static bool gvariant_is_basic_type(int8_t type)
{
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

static GVariant* new_basic_gvariant(const variant& v, const char* types)
{
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

int sig_unit::get_dict_len(const char* types, bool allow_dict_entry, size_t array_depth, size_t struct_depth)
{
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

int sig_unit::get_struct_len(const char* types, bool allow_dict_entry, size_t array_depth, size_t struct_depth)
{
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

int sig_unit::get_sig_len(const char* types, bool allow_dict_entry, size_t array_depth, size_t struct_depth)
{
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

static void parse_signature(const char* types, sig_unit& sig)
{
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

gvariant_auto_free::gvariant_auto_free(GVariant* v, bool add_ref) : ptr(v)
{
    if (add_ref && ptr) {
        g_variant_ref(ptr);
    }
}

void gvariant_auto_free::release()
{
    if (ptr) {
        g_variant_unref(ptr);
        ptr = nullptr;
    }
}

gvariant_auto_free::~gvariant_auto_free()
{
    release();
}

gvariant_auto_free::gvariant_auto_free(const gvariant_auto_free& other) : ptr(other.ptr)
{
    if (ptr) {
        g_variant_ref(ptr);
    }
}

gvariant_auto_free& gvariant_auto_free::operator=(const gvariant_auto_free& other)
{
    if (this != &other) {
        release();
        if (other.ptr) {
            g_variant_ref(other.ptr);
            ptr = other.ptr;
        }
    }
    return *this;
}

gvariant_auto_free::gvariant_auto_free(gvariant_auto_free&& other) noexcept : ptr(other.ptr)
{
    other.ptr = nullptr;
}

gvariant_auto_free& gvariant_auto_free::operator=(gvariant_auto_free&& other) noexcept
{
    if (this != &other) {
        release();
        ptr       = other.ptr;
        other.ptr = nullptr;
    }
    return *this;
}

gvariant_builder::gvariant_builder(const GVariantType* type)
{
    g_variant_builder_init(this, type);
}

gvariant_builder::~gvariant_builder()
{
    g_variant_builder_clear(this);
}

void gvariant_builder::add(GVariant* value)
{
    g_variant_builder_add_value(this, value);
}

GVariant* gvariant_builder::end()
{
    return g_variant_builder_end(this);
}

GVariant* gvariant_convert::new_gvariant_dict(const variant& v, sig_unit& sig)
{
    auto             d         = v.as_dict();
    const char*      key_sig   = sig.sub_types + 1;
    const char*      value_sig = key_sig + 1;
    gvariant_builder builder(G_VARIANT_TYPE(sig.buf));
    for (const auto& entry : d) {
        gvariant_auto_free key(to_gvariant(entry.key, key_sig), true);
        gvariant_auto_free value(to_gvariant(entry.value, value_sig), true);
        auto               dict_entry = g_variant_new_dict_entry(key.ptr, value.ptr);
        builder.add(dict_entry);
    }
    return builder.end();
}

GVariant* gvariant_convert::new_gvariant_struct(const variant& v, sig_unit& sig)
{
    const char* types = sig.sub_types;
    if (types == nullptr) {
        MC_THROW(mc::invalid_arg_exception, "types is nullptr");
    }
    auto   arr = v.as_array();
    size_t len = arr.size();
    if (len > MAX_CONTAINER_SIZE) {
        MC_THROW(mc::invalid_arg_exception, "struct size too large");
    }
    gvariant_builder builder(G_VARIANT_TYPE_TUPLE);
    size_t           i = 0;
    for (; i < len && *types != '\0'; i++) {
        auto pair = to_gvariant_inner(arr[i], types);
        builder.add(std::get<0>(pair));
        types = std::get<1>(pair);
    }
    if (*types != '\0' && i < len) {
        MC_THROW(mc::invalid_arg_exception, "invalid number of elements ${len} for struct: ${types}",
                 ("len", len)("types", std::string(types)));
    }
    return builder.end();
}

GVariant* gvariant_convert::new_gvariant_array(const variant& v, sig_unit& sig)
{
    const char* types = sig.sub_types;
    if (types == nullptr) {
        MC_THROW(mc::exception, "types is nullptr");
    }
    if (*types == DBUS_TYPE_BYTE && v.is_string()) {
        auto str = v.as_string();
        return g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE, str.data(), str.size(), sizeof(char));
    }
    auto             arr = v.as_array();
    gvariant_builder builder(G_VARIANT_TYPE(sig.buf));
    for (auto item : arr) {
        auto child = to_gvariant(item, types);
        builder.add(child);
    }
    return builder.end();
}

GVariant* gvariant_convert::to_gvariant(const variant& v, const char* types)
{
    return std::get<0>(to_gvariant_inner(v, types));
}

std::tuple<GVariant*, const char*> gvariant_convert::to_gvariant_inner(const variant& v, const char* types)
{
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

mc::dict gvariant_convert::dict_to_mc_variant(GVariant* value, int n)
{
    mc::dict d;
    for (int i = 0; i < n; i++) {
        gvariant_auto_free entry(g_variant_get_child_value(value, i));
        gvariant_auto_free entry_key(g_variant_get_child_value(entry.ptr, 0));
        gvariant_auto_free entry_value(g_variant_get_child_value(entry.ptr, 1));
        auto               key   = to_mc_variant(entry_key.ptr);
        auto               value = to_mc_variant(entry_value.ptr);
        d[key]                   = value;
    }
    return d;
}

variants gvariant_convert::array_to_mc_variant(GVariant* value, int n)
{
    variants res;
    for (int i = 0; i < n; i++) {
        gvariant_auto_free child(g_variant_get_child_value(value, i));
        auto               v = to_mc_variant(child.ptr);
        res.push_back(v);
    }
    return res;
}

variant gvariant_convert::container_to_mc_variant(GVariant* value)
{
    if (g_variant_is_of_type(value, G_VARIANT_TYPE_BYTESTRING)) {
        gsize       len  = 0;
        const char* data = reinterpret_cast<const char*>(g_variant_get_fixed_array(value, &len, sizeof(char)));
        return variant(std::string(data, len));
    }
    variants res;
    auto     n = g_variant_n_children(value);
    if (n == 0) {
        return res;
    }
    gvariant_auto_free child(g_variant_get_child_value(value, 0));
    const char*        type = g_variant_get_type_string(child.ptr);
    if (type[0] == DBUS_DICT_ENTRY_BEGIN_CHAR) {
        return dict_to_mc_variant(value, n);
    }
    return array_to_mc_variant(value, n);
}

variant gvariant_convert::to_mc_variant(GVariant* value)
{
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
        case DBUS_TYPE_VARIANT: {
            gvariant_auto_free child(g_variant_get_child_value(value, 0));
            return to_mc_variant(child.ptr);
        }
        default:
            if (g_variant_is_container(value)) {
                return container_to_mc_variant(value);
            }
            MC_THROW(mc::invalid_arg_exception, "unsupported type: ${type}", ("type", type));
    }
}

GVariant* gvariant_convert::to_gvariant(const variant& v)
{
    signature sig;
    detail::variant_to_dbus_signature(sig, v);
    std::string sig_str = sig.str();
    return to_gvariant(v, sig_str.c_str());
}

} // namespace mc::dbus
