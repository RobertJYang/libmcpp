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

    static constexpr std::size_t get_alignment() {
        if constexpr (std::is_same_v<T, bool>) {
            return 4;
        } else if constexpr (std::is_same_v<T, int8_t> || std::is_same_v<T, uint8_t>) {
            return 1;
        } else if constexpr (std::is_same_v<T, int16_t> || std::is_same_v<T, uint16_t>) {
            return 2;
        } else if constexpr (std::is_same_v<T, int32_t> || std::is_same_v<T, uint32_t>) {
            return 4;
        } else if constexpr (std::is_same_v<T, int64_t> || std::is_same_v<T, uint64_t>) {
            return 8;
        } else if constexpr (std::is_same_v<T, double> || std::is_same_v<T, float>) {
            return 8;
        }
        static_assert(!std::is_same_v<T, void>, "Unsupported type");
    }
};

// 对字符串类型的特化
template <typename T>
struct signature_helper<
    T, std::enable_if_t<std::is_same_v<T, std::string> || std::is_same_v<T, std::string_view>>> {
    static void apply(signature& sig) {
        sig += type_code::string_type;
    }

    static constexpr std::size_t get_alignment() {
        return 4;
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

    static constexpr std::size_t get_alignment() {
        return std::max(signature_helper<K>::get_alignment(), signature_helper<V>::get_alignment());
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

    static constexpr std::size_t get_alignment() {
        return (std::max({signature_helper<Args>::get_alignment()...}));
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

    static constexpr std::size_t get_alignment() {
        // 类型 T 与数组长度（uint32_t）对齐对最大值
        return std::max(signature_helper<T>::get_alignment(), std::size_t(4));
    }
};

// 对 std::vector 的特化
template <typename T, typename Alloc>
struct signature_helper<std::vector<T, Alloc>> {
    static void apply(signature& sig) {
        sig += type_code::array_start;
        signature_helper<T>::apply(sig);
    }

    static constexpr std::size_t get_alignment() {
        // 类型 T 与数组长度（uint32_t）对齐对最大值
        return std::max(signature_helper<T>::get_alignment(), std::size_t(4));
    }
};

// 对 std::list 的特化
template <typename T, typename Alloc>
struct signature_helper<std::list<T, Alloc>> {
    static void apply(signature& sig) {
        sig += type_code::array_start;
        signature_helper<T>::apply(sig);
    }

    static constexpr std::size_t get_alignment() {
        // 类型 T 与数组长度（uint32_t）对齐对最大值
        return std::max(signature_helper<T>::get_alignment(), std::size_t(4));
    }
};

// 对 std::deque 的特化
template <typename T, typename Alloc>
struct signature_helper<std::deque<T, Alloc>> {
    static void apply(signature& sig) {
        sig += type_code::array_start;
        signature_helper<T>::apply(sig);
    }

    static constexpr std::size_t get_alignment() {
        // 类型 T 与数组长度（uint32_t）对齐对最大值
        return std::max(signature_helper<T>::get_alignment(), std::size_t(4));
    }
};

// 对 std::array 的特化
template <typename T, std::size_t N>
struct signature_helper<std::array<T, N>> {
    static void apply(signature& sig) {
        sig += type_code::array_start;
        signature_helper<T>::apply(sig);
    }

    static constexpr std::size_t get_alignment() {
        // 类型 T 与数组长度（uint32_t）对齐对最大值
        return std::max(signature_helper<T>::get_alignment(), std::size_t(4));
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

    static constexpr std::size_t get_alignment() {
        // 类型 K 与类型 V 对齐对最大值，与数组长度（uint32_t）对齐对最大值
        return std::max({signature_helper<K>::get_alignment(), signature_helper<V>::get_alignment(),
                         std::size_t(4)});
    }
};

// 对 std::multimap 的特化
template <typename K, typename V, typename Comp, typename Alloc>
struct signature_helper<std::multimap<K, V, Comp, Alloc>> {
    static void apply(signature& sig) {
        sig += "a{";
        signature_helper<K>::apply(sig);
        signature_helper<V>::apply(sig);
        sig += "}";
    }

    static constexpr std::size_t get_alignment() {
        // 类型 K 与类型 V 对齐对最大值，与数组长度（uint32_t）对齐对最大值
        return std::max({signature_helper<K>::get_alignment(), signature_helper<V>::get_alignment(),
                         std::size_t(4)});
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

    static constexpr std::size_t get_alignment() {
        // 类型 K 与类型 V 对齐对最大值，与数组长度（uint32_t）对齐对最大值
        return std::max({signature_helper<K>::get_alignment(), signature_helper<V>::get_alignment(),
                         std::size_t(4)});
    }
};

// 对 std::set 的特化
template <typename T, typename Comp, typename Alloc>
struct signature_helper<std::set<T, Comp, Alloc>> {
    static void apply(signature& sig) {
        sig += type_code::array_start;
        signature_helper<T>::apply(sig);
    }

    static constexpr std::size_t get_alignment() {
        // 类型 T 与数组长度（uint32_t）对齐对最大值
        return std::max(signature_helper<T>::get_alignment(), std::size_t(4));
    }
};

// 对 std::multiset 的特化
template <typename T, typename Comp, typename Alloc>
struct signature_helper<std::multiset<T, Comp, Alloc>> {
    static void apply(signature& sig) {
        sig += type_code::array_start;
        signature_helper<T>::apply(sig);
    }

    static constexpr std::size_t get_alignment() {
        // 类型 T 与数组长度（uint32_t）对齐对最大值
        return std::max(signature_helper<T>::get_alignment(), std::size_t(4));
    }
};

// 对 std::unordered_set 的特化
template <typename T, typename Hash, typename KeyEqual, typename Alloc>
struct signature_helper<std::unordered_set<T, Hash, KeyEqual, Alloc>> {
    static void apply(signature& sig) {
        sig += type_code::array_start;
        signature_helper<T>::apply(sig);
    }

    static constexpr std::size_t get_alignment() {
        // 类型 T 与数组长度（uint32_t）对齐对最大值
        return std::max(signature_helper<T>::get_alignment(), std::size_t(4));
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

    static constexpr std::size_t get_alignment() {
        return std::apply(
            [](auto&&... args) {
                return (std::max({signature_helper<
                    typename std::decay_t<decltype(args)>::member_type>::get_alignment()...}));
            },
            mc::reflect::reflector<T>::get_properties());
    }
};

// 对 variant 的特化
template <typename Config>
struct signature_helper<mc::variant_base<Config>> {
    static void apply(signature& sig) {
        sig += type_code::variant_type;
    }

    static constexpr std::size_t get_alignment() {
        return 1;
    }
};

// 对 dict 的特化
template <>
struct signature_helper<mc::dict> {
    static void apply(signature& sig) {
        sig += container::dict_string_var;
    }

    static constexpr std::size_t get_alignment() {
        return 8;
    }
};

// 对 variants 的特化
template <>
struct signature_helper<mc::variants> {
    static void apply(signature& sig) {
        sig += container::array_of_variant;
    }

    // av 类型
    static constexpr std::size_t get_alignment() {
        return 4;
    }
};

// 对 blob 的特化
template <>
struct signature_helper<mc::blob> {
    static void apply(signature& sig) {
        sig += container::array_of_byte;
    }

    static constexpr std::size_t get_alignment() {
        return 4;
    }
};

// 对 path 的特化
template <>
struct signature_helper<mc::dbus::path> {
    static void apply(signature& sig) {
        sig += type_code::object_path_type;
    }

    static constexpr std::size_t get_alignment() {
        return 4;
    }
};

// 对 signature 的特化
template <>
struct signature_helper<mc::dbus::signature> {
    static void apply(signature& sig) {
        sig += type_code::signature_type;
    }

    static constexpr std::size_t get_alignment() {
        return 1;
    }
};

// 获取签名字符串的版本
template <typename T>
signature get_signature() {
    signature sig;
    signature_helper<T>::apply(sig);
    return sig;
}

} // namespace detail

// 公共接口函数
template <typename T>
const signature& get_signature() {
    using type = std::decay_t<T>;

    static signature sig_str = detail::get_signature<type>();
    return sig_str;
}

template <typename T>
constexpr std::size_t get_alignment() {
    using type = std::decay_t<T>;

    return detail::signature_helper<type>::get_alignment();
}

} // namespace mc::dbus

#endif // MC_DBUS_SIGNATURE_HELPER_H
