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

#ifndef MC_VARIANT_CONTAINER_CONVERT_H
#define MC_VARIANT_CONTAINER_CONVERT_H

/**
 * @file container_convert.h
 * @brief 容器和 variant 之间的通用转换模板
 */

#include <array>
#include <deque>
#include <forward_list>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include <mc/dict.h>

// 前向声明
namespace mc {
// 检查容器是否具有 reserve 方法的辅助类
template <typename T, typename = void>
struct has_reserve : std::false_type {};

template <typename T>
struct has_reserve<T, std::void_t<decltype(std::declval<T>().reserve(std::declval<size_t>()))>>
    : std::true_type {};

// 如果容器支持 reserve，则调用它
template <typename Container>
void reserve_if_possible(Container& container, size_t size, std::true_type) {
    container.reserve(size);
}

// 如果容器不支持 reserve，则不做任何事情
template <typename Container>
void reserve_if_possible(Container&, size_t, std::false_type) {
    // 不支持 reserve 的容器，什么都不做
}
} // namespace mc

namespace mc {

/**
 * @brief 序列容器到 variant 的通用转换函数
 * @tparam Container 序列容器类型，如 std::vector, std::list 等
 * @param container 要转换的容器
 * @param vo 输出的 variant
 */
template <typename Container>
void sequence_to_variant(const Container& container, variant& vo) {
    if (container.size() > MAX_NUM_ARRAY_ELEMENTS) {
        throw std::range_error("容器元素过多");
    }

    variants vars;
    vars.reserve(container.size());
    for (const auto& item : container) {
        variant v;
        to_variant(item, v);
        vars.push_back(std::move(v));
    }
    vo = std::move(vars);
}

/**
 * @brief variant 到序列容器的通用转换函数
 * @tparam Container 序列容器类型，如 std::vector, std::list 等
 * @param var 源 variant
 * @param vo 输出的容器
 */
template <typename Container>
void sequence_from_variant(const variant& var, Container& vo) {
    const variants& vars = var.get_array();
    if (vars.size() > MAX_NUM_ARRAY_ELEMENTS) {
        throw std::range_error("容器元素过多");
    }
    vo.clear();

    // 预分配空间（如果容器支持）
    // 使用 SFINAE 代替 C++20 的 requires 表达式
    reserve_if_possible(vo, vars.size(),
                        std::integral_constant<bool, has_reserve<Container>::value>());

    for (const auto& item : vars) {
        typename Container::value_type v;
        from_variant(item, v);
        vo.push_back(std::move(v));
    }
}

/**
 * @brief 关联容器到 variant 的通用转换函数
 * @tparam Container 关联容器类型，如 std::set, std::unordered_set 等
 * @param container 要转换的容器
 * @param vo 输出的 variant
 */
template <typename Container>
void associative_to_variant(const Container& container, variant& vo) {
    if (container.size() > MAX_NUM_ARRAY_ELEMENTS) {
        throw std::range_error("容器元素过多");
    }

    variants vars;
    vars.reserve(container.size());
    for (const auto& item : container) {
        variant v;
        to_variant(item, v);
        vars.push_back(std::move(v));
    }
    vo = std::move(vars);
}

/**
 * @brief variant 到关联容器的通用转换函数
 * @tparam Container 关联容器类型，如 std::set, std::unordered_set 等
 * @param var 源 variant
 * @param vo 输出的容器
 */
template <typename Container>
void associative_from_variant(const variant& var, Container& vo) {
    const variants& vars = var.get_array();
    if (vars.size() > MAX_NUM_ARRAY_ELEMENTS) {
        throw std::range_error("容器元素过多");
    }
    vo.clear();

    // 预分配空间（如果容器支持）
    reserve_if_possible(vo, vars.size(),
                        std::integral_constant<bool, has_reserve<Container>::value>());

    for (const auto& item : vars) {
        typename Container::value_type v;
        from_variant(item, v);
        vo.insert(std::move(v));
    }
}

/**
 * @brief 数组到 variant 的通用转换函数
 * @tparam T 元素类型
 * @tparam S 数组大小
 * @param arr 要转换的数组
 * @param vo 输出的 variant
 */
template <typename T, std::size_t S>
void array_to_variant(const std::array<T, S>& arr, variant& vo) {
    variants vars;
    vars.reserve(S);
    for (const auto& item : arr) {
        variant v;
        to_variant(item, v);
        vars.push_back(std::move(v));
    }
    vo = std::move(vars);
}

/**
 * @brief variant 到数组的通用转换函数
 * @tparam T 元素类型
 * @tparam S 数组大小
 * @param var 源 variant
 * @param vo 输出的数组
 */
template <typename T, std::size_t S>
void array_from_variant(const variant& var, std::array<T, S>& vo) {
    const variants& vars = var.get_array();
    if (vars.size() != S) {
        throw std::runtime_error("数组大小不匹配");
    }
    for (std::size_t i = 0; i < S; ++i) {
        from_variant(vars[i], vo[i]);
    }
}

/**
 * @brief 将映射类容器转换为 variant
 * @tparam MapType 映射类型，例如 std::map 或 std::unordered_map
 * @tparam K 键类型
 * @tparam T 值类型
 * @param map 要转换的映射
 * @param vo 输出的 variant
 */
template <template <typename...> class MapType, typename K, typename T, typename... Args>
void map_to_variant(const MapType<K, T, Args...>& map, variant& vo) {
    if (map.size() > MAX_NUM_ARRAY_ELEMENTS) {
        throw std::range_error("容器元素过多");
    }

    // 创建dict::entry的vector
    std::vector<dict::entry> entries;
    entries.reserve(map.size());

    for (const auto& pair : map) {
        dict::entry entry;
        // 将键转换为字符串
        if constexpr (std::is_same_v<K, std::string>) {
            entry.key = pair.first;
        } else {
            variant key_variant;
            to_variant(pair.first, key_variant);
            entry.key = key_variant.as_string();
        }

        // 将值转换为variant
        to_variant(pair.second, entry.value);
        entries.push_back(std::move(entry));
    }

    // 直接使用dict构造函数创建variant
    vo = dict(std::move(entries));
}

/**
 * @brief 从 variant 转换为映射类容器
 * @tparam MapType 映射类型，例如 std::map 或 std::unordered_map
 * @tparam K 键类型
 * @tparam T 值类型
 * @param var 源 variant
 * @param vo 输出的映射
 */
template <template <typename...> class MapType, typename K, typename T, typename... Args>
void map_from_variant(const variant& var, MapType<K, T, Args...>& vo) {
    const dict& d = var.get_object();
    if (d.size() > MAX_NUM_ARRAY_ELEMENTS) {
        throw std::range_error("容器元素过多");
    }
    vo.clear();

    // 使用迭代器遍历 dict，而不是使用索引访问
    // 这可以显著提高对链表实现的大型字典的遍历效率
    for (const auto& entry : d) {
        K key;

        // 将键从字符串转换为K类型
        if constexpr (std::is_same_v<K, std::string>) {
            key = entry.key;
        } else {
            variant key_variant(entry.key);
            from_variant(key_variant, key);
        }

        // 将值转换为T类型
        T value = entry.value.template as<T>();

        vo.insert(std::make_pair(key, value));
    }
}

// std::vector 特化
template <typename T, typename Allocator = std::allocator<T>>
void to_variant(const std::vector<T, Allocator>& var, variant& vo) {
    sequence_to_variant(var, vo);
}

template <typename T, typename Allocator = std::allocator<T>>
void from_variant(const variant& var, std::vector<T, Allocator>& vo) {
    sequence_from_variant(var, vo);
}

// std::list 特化
template <typename T, typename Allocator = std::allocator<T>>
void to_variant(const std::list<T, Allocator>& var, variant& vo) {
    sequence_to_variant(var, vo);
}

template <typename T, typename Allocator = std::allocator<T>>
void from_variant(const variant& var, std::list<T, Allocator>& vo) {
    sequence_from_variant(var, vo);
}

// std::deque 特化
template <typename T, typename Allocator = std::allocator<T>>
void to_variant(const std::deque<T, Allocator>& var, variant& vo) {
    sequence_to_variant(var, vo);
}

template <typename T, typename Allocator = std::allocator<T>>
void from_variant(const variant& var, std::deque<T, Allocator>& vo) {
    sequence_from_variant(var, vo);
}

// std::set 特化
template <typename T, typename Compare = std::less<T>, typename Allocator = std::allocator<T>>
void to_variant(const std::set<T, Compare, Allocator>& var, variant& vo) {
    associative_to_variant(var, vo);
}

template <typename T, typename Compare = std::less<T>, typename Allocator = std::allocator<T>>
void from_variant(const variant& var, std::set<T, Compare, Allocator>& vo) {
    associative_from_variant(var, vo);
}

// std::unordered_set 特化
template <typename T, typename Hash = std::hash<T>, typename KeyEqual = std::equal_to<T>,
          typename Allocator = std::allocator<T>>
void to_variant(const std::unordered_set<T, Hash, KeyEqual, Allocator>& var, variant& vo) {
    associative_to_variant(var, vo);
}

template <typename T, typename Hash = std::hash<T>, typename KeyEqual = std::equal_to<T>,
          typename Allocator = std::allocator<T>>
void from_variant(const variant& var, std::unordered_set<T, Hash, KeyEqual, Allocator>& vo) {
    associative_from_variant(var, vo);
}

// std::map 特化
template <typename K, typename T, typename Compare = std::less<K>,
          typename Allocator = std::allocator<std::pair<const K, T>>>
void to_variant(const std::map<K, T, Compare, Allocator>& var, variant& vo) {
    map_to_variant(var, vo);
}

template <typename K, typename T, typename Compare = std::less<K>,
          typename Allocator = std::allocator<std::pair<const K, T>>>
void from_variant(const variant& var, std::map<K, T, Compare, Allocator>& vo) {
    map_from_variant(var, vo);
}

// std::unordered_map 特化
template <typename K, typename T, typename Hash = std::hash<K>,
          typename KeyEqual  = std::equal_to<K>,
          typename Allocator = std::allocator<std::pair<const K, T>>>
void to_variant(const std::unordered_map<K, T, Hash, KeyEqual, Allocator>& var, variant& vo) {
    map_to_variant(var, vo);
}

template <typename K, typename T, typename Hash = std::hash<K>,
          typename KeyEqual  = std::equal_to<K>,
          typename Allocator = std::allocator<std::pair<const K, T>>>
void from_variant(const variant& var, std::unordered_map<K, T, Hash, KeyEqual, Allocator>& vo) {
    map_from_variant(var, vo);
}

// std::array 特化
template <typename T, std::size_t S>
void to_variant(const std::array<T, S>& var, variant& vo) {
    array_to_variant(var, vo);
}

template <typename T, std::size_t S>
void from_variant(const variant& var, std::array<T, S>& vo) {
    array_from_variant(var, vo);
}

// std::pair 特化
template <typename K, typename T>
void to_variant(const std::pair<K, T>& var, variant& vo) {
    variants vars;
    vars.reserve(2);
    variant a, b;
    to_variant(var.first, a);
    to_variant(var.second, b);
    vars.push_back(std::move(a));
    vars.push_back(std::move(b));
    vo = std::move(vars);
}

template <typename K, typename T>
void from_variant(const variant& var, std::pair<K, T>& vo) {
    const variants& vars = var.get_array();
    if (vars.size() != 2) {
        throw std::runtime_error("pair 大小不匹配");
    }
    from_variant(vars[0], vo.first);
    from_variant(vars[1], vo.second);
}

// 智能指针特化
template <typename T>
void to_variant(const std::shared_ptr<T>& var, variant& vo) {
    if (var) {
        to_variant(*var, vo);
    } else {
        vo = variant();
    }
}

template <typename T>
void from_variant(const variant& var, std::shared_ptr<T>& vo) {
    if (var.is_null()) {
        vo.reset();
    } else {
        if (!vo) {
            vo = std::make_shared<T>();
        }
        from_variant(var, *vo);
    }
}

template <typename T, typename Deleter = std::default_delete<T>>
void to_variant(const std::unique_ptr<T, Deleter>& var, variant& vo) {
    if (var) {
        to_variant(*var, vo);
    } else {
        vo = variant();
    }
}

template <typename T, typename Deleter = std::default_delete<T>>
void from_variant(const variant& var, std::unique_ptr<T, Deleter>& vo) {
    if (var.is_null()) {
        vo.reset();
    } else {
        if (!vo) {
            vo = std::make_unique<T>();
        }
        from_variant(var, *vo);
    }
}

// std::optional 的转换函数
template <typename T>
void to_variant(const std::optional<T>& var, mc::variant& vo) {
    if (var.has_value()) {
        to_variant(var.value(), vo);
    } else {
        vo = variant();
    }
}

template <typename T>
void from_variant(const mc::variant& var, std::optional<T>& vo) {
    if (var.is_null()) {
        vo = std::nullopt;
    } else {
        T value;
        from_variant(var, value);
        vo = std::move(value);
    }
}

// std::variant 的转换函数
template <typename... T>
void to_variant(const std::variant<T...>& var, mc::variant& vo) {
    std::visit(
        [&vo](const auto& value) {
            to_variant(value, vo);
        },
        var);
}

// 辅助函数，尝试将 variant 转换为特定类型
template <typename T, typename... Rest>
bool try_convert_variant(const mc::variant& var, std::variant<Rest...>& vo, bool& converted) {
    if (converted) {
        return true; // 已经转换成功，不需要再尝试
    }

    try {
        T value;
        from_variant(var, value);
        vo        = std::move(value);
        converted = true;
        return true;
    } catch (const std::exception&) {
        // 转换失败，尝试下一种类型
        return false;
    }
}

template <typename... T>
void from_variant(const mc::variant& var, std::variant<T...>& vo) {
    // 尝试将 variant 转换为每种可能的类型
    bool converted = false;

    // 使用折叠表达式尝试每种类型
    (try_convert_variant<T>(var, vo, converted) || ...);

    if (!converted) {
        throw std::bad_cast();
    }
}

} // namespace mc

#endif // MC_VARIANT_CONTAINER_CONVERT_H