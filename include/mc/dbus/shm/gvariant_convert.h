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

#ifndef MC_DBUS_APP_BUS_H
#define MC_DBUS_APP_BUS_H

#include <dbus/dbus.h>
#include <glib-2.0/glib.h>
#include <mc/dict.h>
#include <mc/variant.h>

namespace mc::dbus {
constexpr int32_t MAX_SIG_LEN         = 100;
constexpr int32_t MAX_CONTAINER_DEPTH = 32;
constexpr int32_t MAX_CONTAINER_SIZE  = 1024;
constexpr int32_t MAX_SIGNATURE_LEN   = 255;

struct sig_unit {
public:
    int         type;
    int         len;
    char        buf[MAX_SIG_LEN + 1];
    const char* sub_types;
    const char* next_types;

    bool sub_types_is_valid() const {
        return sub_types != nullptr && *sub_types != '\0';
    }

    static int get_sig_len(const char* types, bool allow_dict_entry, size_t array_depth,
                           size_t struct_depth);

private:
    static int get_dict_len(const char* types, bool allow_dict_entry, size_t array_depth,
                            size_t struct_depth);
    static int get_struct_len(const char* types, bool allow_dict_entry, size_t array_depth,
                              size_t struct_depth);
};

class gvariant_convert {
public:
    static variant     to_mc_variant(GVariant* value);
    static GVariant*   to_gvariant(const variant& value, const char* types);
    static GVariant*   to_gvariant(const variant& value);
    static std::string get_variant_signature(const variant& value, int depth = 0);

private:
    static std::tuple<GVariant*, const char*> to_gvariant_inner(const variant& v,
                                                                const char*    types);
    static dict                               dict_to_mc_variant(GVariant* value, int n);
    static variants                           array_to_mc_variant(GVariant* value, int n);
    static variant                            container_to_mc_variant(GVariant* value);
    static GVariant*                          new_gvariant_dict(const variant& v, sig_unit& sig);
    static GVariant*                          new_gvariant_struct(const variant& v, sig_unit& sig);
    static GVariant*                          new_gvariant_array(const variant& v, sig_unit& sig);
    static std::string                        get_array_signature(const variants& v, int depth);
    static std::string                        get_dict_signature(const dict& v, int depth);
};

} // namespace mc::dbus

#endif // MC_DBUS_APP_BUS_H