/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef MC_DICT_DICT_INL_H
#define MC_DICT_DICT_INL_H

#include <mc/dict/entry.h>

namespace mc {

inline dict::iterator dict::find(const std::string& key)
{
    return find(mc::string_view(key.data(), key.size()));
}

inline dict::iterator dict::find(std::string_view key)
{
    return find(mc::string_view(key.data(), key.size()));
}

inline dict::const_iterator dict::find(const std::string& key) const
{
    return find(mc::string_view(key.data(), key.size()));
}

inline dict::const_iterator dict::find(std::string_view key) const
{
    return find(mc::string_view(key.data(), key.size()));
}

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
    variant value;
    if constexpr (std::is_constructible_v<variant, Arg&&>) {
        value = variant(std::forward<Arg>(arg));
    } else {
        to_variant(std::forward<Arg>(arg), value);
    }
    return insert(std::move(key), std::move(value));
}

template <typename K, typename Arg>
std::pair<dict::iterator, bool> dict::try_emplace(const K& key, Arg&& arg)
{
    variant vkey;
    if constexpr (std::is_constructible_v<variant, const K&>) {
        vkey = variant(key);
    } else {
        to_variant(key, vkey);
    }
    if (!contains(vkey)) {
        return emplace(vkey, std::forward<Arg>(arg));
    }
    return {find(vkey), false};
}

} // namespace mc

#endif // MC_DICT_DICT_INL_H
