/*
 * Copyright (c) 2024 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 */

#include <mc/dbus/marshalled.h>
#include <mc/dbus/stream.h>

namespace mc::dbus {

void demarshal(stream& stream, signature_iterator it, mc::variant& v) {
    stream.read_variant(it, v, 0);
}

void demarshal(stream& stream, signature_iterator it, mc::dict& v) {
    mc::mutable_dict dict;
    stream.read_variant_dict(it, dict, 0);
    v = std::move(dict);
}

void demarshal(stream& stream, signature_iterator it, mc::mutable_dict& v) {
    stream.read_variant_dict(it, v, 0);
}

void demarshal(stream& stream, signature_iterator it, mc::variants& v) {
    stream.read_variant_array(it, v, 0);
}

void marshal(stream& stream, signature_iterator it, mc::variant& v) {
    stream.write_variant(it, v, 0);
}

void marshal(stream& stream, signature_iterator it, mc::dict& v) {
    stream.write_variant_dict(it, v, 0);
}

void marshal(stream& stream, signature_iterator it, mc::mutable_dict& v) {
    stream.write_variant_dict(it, v, 0);
}

void marshal(stream& stream, signature_iterator it, mc::variants& v) {
    stream.write_variant_array(it, v, 0);
}

} // namespace mc::dbus