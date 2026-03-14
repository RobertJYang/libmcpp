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

#ifndef MC_DICT_H
#define MC_DICT_H

#include <mc/dict/dict.h>
#include <mc/dict/mutable_dict.h>

// 包含完整的类型定义以支持模板实现
#include <mc/dict/entry.h>

// 定义在std命名空间中特化hash以支持dict和mutable_dict
namespace std {
template <>
struct hash<mc::dict> {
    size_t operator()(const mc::dict& d) const
    {
        return d.hash();
    }
};
} // namespace std

namespace mc {
template <typename InputIt>
dict::dict(InputIt first, InputIt last)
{
    m_data = mc::make_shared<data_t>();
    for (; first != last; ++first) {
        insert(first->first, first->second);
    }
}

template <typename InputIt>
auto dict::insert(InputIt first, InputIt last)
    -> std::enable_if_t<std::is_convertible_v<typename std::iterator_traits<InputIt>::value_type, entry>>
{
    for (; first != last; ++first) {
        insert(*first);
    }
}

template <typename InputIt>
auto dict::insert(InputIt first, InputIt last)
    -> std::enable_if_t<is_variant_constructible_v<typename std::iterator_traits<InputIt>::value_type::first_type> &&
                        is_variant_constructible_v<typename std::iterator_traits<InputIt>::value_type::second_type>>
{
    for (; first != last; ++first) {
        insert(first->first, first->second);
    }
}

template <typename K, typename V>
void dict::insert(std::initializer_list<std::pair<K, V>> ilist)
{
    for (const auto& pair : ilist) {
        insert(pair.first, pair.second);
    }
}

template <typename Arg>
std::pair<dict::iterator, bool> dict::emplace(variant key, Arg&& arg)
{
    return insert(std::move(key), variant(std::forward<Arg>(arg)));
}

template <typename K, typename Arg>
std::pair<dict::iterator, bool> dict::try_emplace(const K& key, Arg&& arg)
{
    variant vkey(key);
    if (!contains(vkey)) {
        return emplace(vkey, std::forward<Arg>(arg));
    }
    return {find(vkey), false};
}
} // namespace mc

#endif // MC_DICT_H