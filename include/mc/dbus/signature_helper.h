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

#ifndef MC_DBUS_SIGNATURE_HELPER_H
#define MC_DBUS_SIGNATURE_HELPER_H

#include <mc/dbus/path.h>
#include <mc/dbus/signature.h>
#include <mc/dict.h>
#include <mc/reflect.h>
#include <mc/traits.h>
#include <mc/variant.h>

#include <deque>
#include <list>
#include <map>
#include <optional>
#include <queue>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace mc::dbus {
namespace detail {

template <typename T, typename = void>
struct signature_helper {
    static void apply(signature& sig) {
        static_assert(std::is_same_v<T, void>, "Unsupported type for get_signature");
    }
};

// 对基本算术类型的特化
template <typename T>
struct signature_helper<T, std::enable_if_t<std::is_arithmetic_v<T>>> {
    static void apply(signature& sig) {
        // 处理基本数值类型
        if constexpr (std::is_same_v<T, bool>) {
            sig += type_code::boolean_type;
        } else if constexpr (std::is_same_v<T, int8_t> || std::is_same_v<T, uint8_t>) {
            sig += type_code::byte_type;
        } else if constexpr (std::is_same_v<T, int16_t>) {
            sig += type_code::int16_type;
        } else if constexpr (std::is_same_v<T, uint16_t>) {
            sig += type_code::uint16_type;
        } else if constexpr (std::is_same_v<T, int32_t>) {
            sig += type_code::int32_type;
        } else if constexpr (std::is_same_v<T, uint32_t>) {
            sig += type_code::uint32_type;
        } else if constexpr (std::is_same_v<T, int64_t>) {
            sig += type_code::int64_type;
        } else if constexpr (std::is_same_v<T, uint64_t>) {
            sig += type_code::uint64_type;
        } else if constexpr (std::is_same_v<T, double>) {
            sig += type_code::double_type;
        } else if constexpr (std::is_same_v<T, float>) {
            sig += type_code::double_type;
        } else {
            static_assert(std::is_same_v<T, void>, "Unsupported type");
        }
    }
};

// 对字符串类型的特化
template <typename T>
struct signature_helper<
    T, std::enable_if_t<std::is_same_v<T, std::string> || std::is_same_v<T, std::string_view>>> {
    static void apply(signature& sig) {
        sig += type_code::string_type;
    }
};

// 对 std::pair 的特化
template <typename K, typename V>
struct signature_helper<std::pair<K, V>> {
    static void apply(signature& sig) {
        sig += type_code::struct_start;
        signature_helper<K>::apply(sig);
        signature_helper<V>::apply(sig);
        sig += type_code::struct_end;
    }
};

// 对 std::tuple 的特化
template <typename... Args>
struct signature_helper<std::tuple<Args...>> {
    static void apply(signature& sig) {
        sig += type_code::struct_start;
        ((signature_helper<Args>::apply(sig)), ...);
        sig += type_code::struct_end;
    }
};

// 对 std::optional 的特化
template <typename T>
struct signature_helper<std::optional<T>> {
    static void apply(signature& sig) {
        // 用 aT 来表示 optional，长度为0的数组表示 std::nullopt
        sig += type_code::array_start;
        signature_helper<T>::apply(sig);
    }
};

// 对 std::vector 的特化
template <typename T, typename Alloc>
struct signature_helper<std::vector<T, Alloc>> {
    static void apply(signature& sig) {
        sig += type_code::array_start;
        signature_helper<T>::apply(sig);
    }
};

// 对 std::list 的特化
template <typename T, typename Alloc>
struct signature_helper<std::list<T, Alloc>> {
    static void apply(signature& sig) {
        sig += type_code::array_start;
        signature_helper<T>::apply(sig);
    }
};

// 对 std::queue 的特化
template <typename T, typename Alloc>
struct signature_helper<std::queue<T, Alloc>> {
    static void apply(signature& sig) {
        sig += type_code::array_start;
        signature_helper<T>::apply(sig);
    }
};

// 对 std::deque 的特化
template <typename T, typename Alloc>
struct signature_helper<std::deque<T, Alloc>> {
    static void apply(signature& sig) {
        sig += type_code::array_start;
        signature_helper<T>::apply(sig);
    }
};

// 对 std::map 的特化
template <typename K, typename V, typename Comp, typename Alloc>
struct signature_helper<std::map<K, V, Comp, Alloc>> {
    static void apply(signature& sig) {
        sig += "a{";
        signature_helper<K>::apply(sig);
        signature_helper<V>::apply(sig);
        sig += "}";
    }
};

// 对 std::unordered_map 的特化
template <typename K, typename V, typename Hash, typename KeyEqual, typename Alloc>
struct signature_helper<std::unordered_map<K, V, Hash, KeyEqual, Alloc>> {
    static void apply(signature& sig) {
        sig += "a{";
        signature_helper<K>::apply(sig);
        signature_helper<V>::apply(sig);
        sig += "}";
    }
};

// 对反射类型的特化
template <typename T>
struct signature_helper<T, std::enable_if_t<mc::reflect::is_reflectable<T>()>> {
    static void apply(signature& sig) {
        sig += type_code::struct_start;
        mc::traits::tuple_for_each(mc::reflect::reflector<T>::get_properties(), [&](auto& p) {
            using member_type = typename std::decay_t<decltype(p)>::member_type;
            signature_helper<member_type>::apply(sig);
        });
        sig += type_code::struct_end;
    }
};

// 对 variant 的特化
template <typename Config>
struct signature_helper<mc::variant_base<Config>> {
    static void apply(signature& sig) {
        sig += type_code::variant_type;
    }
};

// 对 dict 的特化
template <>
struct signature_helper<mc::dict> {
    static void apply(signature& sig) {
        sig += container::dict_string_var;
    }
};

// 对 variants 的特化
template <>
struct signature_helper<mc::variants> {
    static void apply(signature& sig) {
        sig += container::array_of_variant;
    }
};

// 对 blob 的特化
template <>
struct signature_helper<mc::blob> {
    static void apply(signature& sig) {
        sig += container::array_of_byte;
    }
};

// 对 path 的特化
template <>
struct signature_helper<mc::dbus::path> {
    static void apply(signature& sig) {
        sig += type_code::object_path_type;
    }
};

// 对 signature 的特化
template <>
struct signature_helper<mc::dbus::signature> {
    static void apply(signature& sig) {
        sig += type_code::signature_type;
    }
};

} // namespace detail

// 公共接口函数
template <typename T>
void get_signature(signature& sig) {
    detail::signature_helper<T>::apply(sig);
}

// 获取签名字符串的版本
template <typename T>
signature get_signature() {
    signature sig;
    get_signature<T>(sig);
    return sig;
}

} // namespace mc::dbus

#endif // MC_DBUS_SIGNATURE_HELPER_H
