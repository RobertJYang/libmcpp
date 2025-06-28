/**
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

#ifndef MC_ENGINE_UTILS_H
#define MC_ENGINE_UTILS_H

#include <mc/reflect/metadata_info.h>
#include <mc/traits.h>
#include <string_view>

namespace mc::engine {
namespace detail {

// 用于检测类型是否是接口类型
template <typename T, typename = void>
struct is_interface : std::false_type {};

template <typename T>
struct is_interface<T, std::void_t<typename T::is_interface>> : std::true_type {};

template <typename T>
constexpr bool is_interface_v = is_interface<T>::value;

struct filter_interface {
    template <typename ElementType>
    static constexpr bool check = is_interface_v<typename ElementType::member_type>;
};

constexpr std::size_t max_name_length = 255;

constexpr bool is_allowable_char(char c) {
    return is_identifier_char(c);
}

constexpr bool is_allowable_first_char(char c) {
    return is_first_identifier_char(c);
}

constexpr bool is_valid_interface_name(std::string_view name) {
    // 必须至少有一个点分隔符
    bool has_dot = false;

    // 不能以点开头或结尾
    if (name.empty() || name[0] == '.' || name[name.size() - 1] == '.' ||
        name.size() > max_name_length) {
        return false;
    }

    // 检查每个分段
    bool segment_start = true;
    for (size_t i = 0; i < name.size(); ++i) {
        char c = name[i];

        if (c == '.') {
            // 找到点分隔符
            has_dot = true;

            // 不允许连续的点
            if (i > 0 && name[i - 1] == '.') {
                return false;
            }

            segment_start = true;
        } else if (segment_start) {
            // 检查分段的第一个字符必须是字母
            if (!is_allowable_first_char(c)) {
                return false;
            }
            segment_start = false;
        } else if (!is_allowable_char(c)) {
            return false;
        }
    }

    return has_dot;
}

constexpr bool is_valid_member_name(std::string_view name) {
    if (name.empty() || name.size() > max_name_length) {
        return false;
    }

    if (!is_allowable_first_char(name[0])) {
        return false;
    }

    for (size_t i = 1; i < name.size(); ++i) {
        if (!is_allowable_char(name[i])) {
            return false;
        }
    }

    return true;
}

// InterfaceTypes： std::tuple<Interface1*, Interface2, ...>
template <typename MemberType, typename InterfaceTypes>
constexpr bool is_interface_declared(const InterfaceTypes& interfaces) {
    bool is_declared = false;
    mc::traits::tuple_for_each(interfaces, [&](auto interface) {
        using declared_type = std::remove_pointer_t<decltype(interface)>;
        if constexpr (std::is_same_v<MemberType, declared_type>) {
            is_declared = true;
        }
    });

    return is_declared;
}

// 检查对象实现的 interface 成员是否都声明了
template <typename InterfaceTypes, typename Members>
constexpr bool check_members_is_declared(const InterfaceTypes& declared_interfaces,
                                         const Members&        members) {
    bool all_valid = true;
    mc::traits::tuple_for_each(members, [&](auto& element) {
        using element_type = mc::traits::remove_cvref_t<decltype(element)>;
        if constexpr (mc::reflect::has_tag_v<mc::reflect::property_tag, element_type> ||
                      mc::reflect::has_tag_v<mc::reflect::base_class_tag, element_type>) {
            using member_type = typename element_type::member_type;
            if constexpr (is_interface_v<member_type>) {
                if (!is_interface_declared<member_type>(declared_interfaces)) {
                    all_valid = false;
                }
            }
        }
    });

    return all_valid;
}

// 检查声明的所有 interfaces 都是满足要求的接口类型
template <typename InterfaceTypes>
constexpr bool check_declared_interfaces(const InterfaceTypes& declared_interfaces) {
    bool all_valid = true;
    mc::traits::tuple_for_each(declared_interfaces, [&](auto& interface) {
        using interface_type =
            std::remove_pointer_t<mc::traits::remove_cvref_t<decltype(interface)>>;
        if constexpr (!is_interface_v<interface_type>) {
            all_valid = false;
        }
    });

    return all_valid;
}

template <typename InterfaceType, typename Members>
constexpr int get_members_count(const Members& members) {
    int count = 0;
    mc::traits::tuple_for_each(members, [&](auto& element) {
        using element_type = mc::traits::remove_cvref_t<decltype(element)>;
        if constexpr (mc::reflect::has_tag_v<mc::reflect::property_tag, element_type> ||
                      mc::reflect::has_tag_v<mc::reflect::base_class_tag, element_type>) {
            using member_type = typename element_type::member_type;
            if constexpr (is_interface_v<member_type> &&
                          std::is_same_v<member_type, InterfaceType>) {
                count++;
            }
        }
    });

    return count;
}

// 检查对象声明的 interface 成员是否都实现了，并且仅且实现了一次
template <typename InterfaceTypes, typename Members>
constexpr bool check_interface_implement(const InterfaceTypes& declared_interfaces,
                                         const Members&        members) {
    bool all_valid = true;
    mc::traits::tuple_for_each(declared_interfaces, [&](auto& interface) {
        using interface_type =
            std::remove_pointer_t<mc::traits::remove_cvref_t<decltype(interface)>>;
        if (get_members_count<interface_type>(members) != 1) {
            // 没有实现或者实现多个
            all_valid = false;
        }
    });

    return all_valid;
}

template <typename InterfaceTypes, typename Members>
constexpr bool check_members(const InterfaceTypes& declared_interfaces, const Members& members) {
    if (!check_declared_interfaces(declared_interfaces)) {
        return false;
    }

    if (!check_members_is_declared(declared_interfaces, members)) {
        return false;
    }

    if (!check_interface_implement(declared_interfaces, members)) {
        return false;
    }

    return true;
}

bool path_starts_with(std::string_view path, std::string_view prefix);

} // namespace detail
} // namespace mc::engine
#endif // MC_ENGINE_UTILS_H