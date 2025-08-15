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

#ifndef MC_REFLECT_SIGNATURE_HELPER_H
#define MC_REFLECT_SIGNATURE_HELPER_H

#include <mc/dict.h>
#include <mc/reflect/type_code.h>
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
#include <unordered_set>
#include <vector>

namespace mc::reflect {
namespace detail {

template <typename T, typename = void>
struct signature_helper {
    static void apply(std::string& sig) {
        static_assert(std::is_same_v<T, void>, "Unsupported type for get_signature");
    }
};

// 对基本算术类型的特化
template <typename T>
struct signature_helper<T, std::enable_if_t<std::is_arithmetic_v<T>>> {
    static void apply(std::string& sig) {
        // 处理基本数值类型
        if constexpr (std::is_same_v<T, bool>) {
            sig += type_to_char(type_code::boolean_type);
        } else if constexpr (sizeof(T) == 1) {
            sig += type_to_char(type_code::byte_type);
        } else if constexpr (sizeof(T) == 2 && std::is_integral_v<T> && std::is_signed_v<T>) {
            sig += type_to_char(type_code::int16_type);
        } else if constexpr (sizeof(T) == 2 && std::is_integral_v<T> && std::is_unsigned_v<T>) {
            sig += type_to_char(type_code::uint16_type);
        } else if constexpr (sizeof(T) == 4 && std::is_integral_v<T> && std::is_signed_v<T>) {
            sig += type_to_char(type_code::int32_type);
        } else if constexpr (sizeof(T) == 4 && std::is_integral_v<T> && std::is_unsigned_v<T>) {
            sig += type_to_char(type_code::uint32_type);
        } else if constexpr (sizeof(T) == 8 && std::is_integral_v<T> && std::is_signed_v<T>) {
            sig += type_to_char(type_code::int64_type);
        } else if constexpr (sizeof(T) == 8 && std::is_integral_v<T> && std::is_unsigned_v<T>) {
            sig += type_to_char(type_code::uint64_type);
        } else if constexpr (std::is_same_v<T, double>) {
            sig += type_to_char(type_code::double_type);
        } else if constexpr (std::is_same_v<T, float>) {
            sig += type_to_char(type_code::double_type);
        } else {
            static_assert(std::is_same_v<T, void>, "Unsupported type");
        }
    }
};

// 对字符串类型的特化
template <typename T>
struct signature_helper<
    T, std::enable_if_t<
           std::is_same_v<T, std::string> ||
           std::is_same_v<T, std::string_view> ||
           std::is_same_v<T, const char*> ||
           std::is_same_v<T, char*>>> {
    static void apply(std::string& sig) {
        sig += type_to_char(type_code::string_type);
    }
};

// 对 std::pair 的特化
template <typename K, typename V>
struct signature_helper<std::pair<K, V>> {
    static void apply(std::string& sig) {
        sig += type_to_char(type_code::struct_start);
        signature_helper<std::decay_t<K>>::apply(sig);
        signature_helper<std::decay_t<V>>::apply(sig);
        sig += type_to_char(type_code::struct_end);
    }
};

// 对 std::tuple 的特化
template <typename... Args>
struct signature_helper<std::tuple<Args...>> {
    static void apply(std::string& sig) {
        sig += type_to_char(type_code::struct_start);
        ((signature_helper<std::decay_t<Args>>::apply(sig)), ...);
        sig += type_to_char(type_code::struct_end);
    }
};

// 对 std::optional 的特化
template <typename T>
struct signature_helper<std::optional<T>> {
    static void apply(std::string& sig) {
        // 用 aT 来表示 optional，长度为0的数组表示 std::nullopt
        sig += type_to_char(type_code::array_start);
        signature_helper<std::decay_t<T>>::apply(sig);
    }
};

// 对 std::vector 的特化
template <typename T, typename Alloc>
struct signature_helper<std::vector<T, Alloc>> {
    static void apply(std::string& sig) {
        sig += type_to_char(type_code::array_start);
        signature_helper<std::decay_t<T>>::apply(sig);
    }
};

// 对 std::list 的特化
template <typename T, typename Alloc>
struct signature_helper<std::list<T, Alloc>> {
    static void apply(std::string& sig) {
        sig += type_to_char(type_code::array_start);
        signature_helper<std::decay_t<T>>::apply(sig);
    }
};

// 对 std::deque 的特化
template <typename T, typename Alloc>
struct signature_helper<std::deque<T, Alloc>> {
    static void apply(std::string& sig) {
        sig += type_to_char(type_code::array_start);
        signature_helper<std::decay_t<T>>::apply(sig);
    }
};

// 对 std::array 的特化
template <typename T, std::size_t N>
struct signature_helper<std::array<T, N>> {
    static void apply(std::string& sig) {
        sig += type_to_char(type_code::array_start);
        signature_helper<std::decay_t<T>>::apply(sig);
    }
};

// 对 std::map 的特化
template <typename K, typename V, typename Comp, typename Alloc>
struct signature_helper<std::map<K, V, Comp, Alloc>> {
    static void apply(std::string& sig) {
        sig += "a{";
        signature_helper<std::decay_t<K>>::apply(sig);
        signature_helper<std::decay_t<V>>::apply(sig);
        sig += "}";
    }
};

// 对 std::multimap 的特化
template <typename K, typename V, typename Comp, typename Alloc>
struct signature_helper<std::multimap<K, V, Comp, Alloc>> {
    static void apply(std::string& sig) {
        sig += "a{";
        signature_helper<std::decay_t<K>>::apply(sig);
        signature_helper<std::decay_t<V>>::apply(sig);
        sig += "}";
    }
};

// 对 std::unordered_map 的特化
template <typename K, typename V, typename Hash, typename KeyEqual, typename Alloc>
struct signature_helper<std::unordered_map<K, V, Hash, KeyEqual, Alloc>> {
    static void apply(std::string& sig) {
        sig += "a{";
        signature_helper<std::decay_t<K>>::apply(sig);
        signature_helper<std::decay_t<V>>::apply(sig);
        sig += "}";
    }
};

// 对 std::set 的特化
template <typename T, typename Comp, typename Alloc>
struct signature_helper<std::set<T, Comp, Alloc>> {
    static void apply(std::string& sig) {
        sig += type_to_char(type_code::array_start);
        signature_helper<std::decay_t<T>>::apply(sig);
    }
};

// 对 std::multiset 的特化
template <typename T, typename Comp, typename Alloc>
struct signature_helper<std::multiset<T, Comp, Alloc>> {
    static void apply(std::string& sig) {
        sig += type_to_char(type_code::array_start);
        signature_helper<std::decay_t<T>>::apply(sig);
    }
};

// 对 std::unordered_set 的特化
template <typename T, typename Hash, typename KeyEqual, typename Alloc>
struct signature_helper<std::unordered_set<T, Hash, KeyEqual, Alloc>> {
    static void apply(std::string& sig) {
        sig += type_to_char(type_code::array_start);
        signature_helper<std::decay_t<T>>::apply(sig);
    }
};

// 对 variant 的特化
template <typename Config>
struct signature_helper<mc::variant_base<Config>> {
    static void apply(std::string& sig) {
        sig += type_to_char(type_code::variant_type);
    }
};

// 对 dict 的特化
template <>
struct signature_helper<mc::dict> {
    static void apply(std::string& sig) {
        sig += container::dict_string_var;
    }
};

// 对 mutable_dict 的特化
template <>
struct signature_helper<mc::mutable_dict> {
    static void apply(std::string& sig) {
        sig += container::dict_string_var;
    }
};

// 对 variants 的特化
template <>
struct signature_helper<mc::variants> {
    static void apply(std::string& sig) {
        sig += container::array_of_variant;
    }
};

// 对 blob 的特化
template <>
struct signature_helper<mc::blob> {
    static void apply(std::string& sig) {
        sig += container::array_of_byte;
    }
};

// 获取签名字符串的版本
template <typename T>
std::string get_signature() {
    std::string sig;
    signature_helper<std::decay_t<T>>::apply(sig);
    sig.shrink_to_fit();
    return sig;
}

} // namespace detail

// 公共接口函数
template <typename T>
const std::string& get_signature() {
    static std::string sig_str = detail::get_signature<T>();
    return sig_str;
}

} // namespace mc::reflect

#endif // MC_REFLECT_SIGNATURE_H
