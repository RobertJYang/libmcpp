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

#ifndef MC_DBUS_MARSHALLED_H
#define MC_DBUS_MARSHALLED_H
#include <mc/dbus/signature.h>
#include <mc/reflect.h>

namespace mc::dbus {
class stream;

void demarshal(stream& stream, signature_iterator it, mc::variant& v);
void demarshal(stream& stream, signature_iterator it, mc::dict& v);
void demarshal(stream& stream, signature_iterator it, mc::mutable_dict& v);
void demarshal(stream& stream, signature_iterator it, mc::variants& v);
template <typename T, std::enable_if_t<mc::reflect::is_reflectable<T>(), int> = 0>
void demarshal(stream& stream, signature_iterator it, T& v) {
    // TODO:: 反射类型先通过variant中转，后续可以优化成直接从stream中读取到T中
    mc::variant tmp;
    demarshal(stream, it, tmp);
    from_variant(tmp, v);
}

void marshal(stream& stream, signature_iterator it, mc::variant& v);
void marshal(stream& stream, signature_iterator it, mc::dict& v);
void marshal(stream& stream, signature_iterator it, mc::mutable_dict& v);
void marshal(stream& stream, signature_iterator it, mc::variants& v);
template <typename T, std::enable_if_t<mc::reflect::is_reflectable<T>(), int> = 0>
void marshal(stream& stream, signature_iterator it, const T& v) {
    // TODO:: 反射类型先通过variant中转，后续可以优化成直接从T写入stream中
    mc::variant tmp;
    to_variant(v, tmp);
    marshal(stream, it, tmp);
}
} // namespace mc::dbus
#endif // MC_DBUS_MARSHALLED_H
