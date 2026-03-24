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

#ifndef MC_IM_PREFIX_H
#define MC_IM_PREFIX_H

#include <string>

#include <mc/string_view.h>

namespace mc::im {

// 别名，可以作为key_type使用
using key_view = mc::string_view;

template <typename Alloc = std::allocator<char>>
class key_buffer : public std::basic_string<char, std::char_traits<char>, Alloc> {
public:
    using base_type      = std::basic_string<char, std::char_traits<char>, Alloc>;
    using allocator_type = typename base_type::allocator_type;

    using base_type::base_type;
    using base_type::operator=;

    key_buffer() = default;

    explicit key_buffer(key_view view) : base_type(view.data(), view.size())
    {}

    key_buffer(key_view view, const allocator_type& alloc) : base_type(view.data(), view.size(), alloc)
    {}

    explicit key_buffer(const allocator_type& alloc) : base_type(alloc)
    {}

    key_buffer(const allocator_type& alloc, key_view view) : base_type(view.data(), view.size(), alloc)
    {}

    key_buffer(const allocator_type& alloc, const key_buffer& other) : base_type(other, alloc)
    {}

    key_buffer(const allocator_type& alloc, key_buffer&& other) : base_type(std::move(other), alloc)
    {}

    constexpr operator key_view() const noexcept
    {
        return key_view(this->data(), this->size());
    }

    constexpr key_view view() const noexcept
    {
        return key_view(this->data(), this->size());
    }
};

// 便捷函数
inline key_buffer<> to_key_buffer(key_view view)
{
    return key_buffer<>(view);
}

template <typename Alloc>
key_buffer<Alloc> to_key_buffer(key_view view, const Alloc& alloc)
{
    return key_buffer<Alloc>(view, alloc);
}

template <typename Key1, typename Key2>
inline size_t longest_prefix(Key1 k1, Key2 k2)
{
    size_t max_len = std::min(k1.size(), k2.size());
    size_t i       = 0;
    for (; i < max_len; i++) {
        if (k1[i] != k2[i]) {
            break;
        }
    }
    return i;
}

/**
 * 判断一个键是否以另一个键为前缀
 * @param key 要检查的键
 * @param prefix 要检查的前缀
 * @return 如果prefix是key的前缀则返回true，否则返回false
 */
template <typename Key, typename Prefix>
inline bool has_prefix(Key key, Prefix prefix)
{
    // 如果前缀长度大于键长度，肯定不是前缀
    if (prefix.size() > key.size()) {
        return false;
    }

    // 检查前缀的每个字符是否与键的对应位置相同
    for (size_t i = 0; i < prefix.size(); i++) {
        if (key[i] != prefix[i]) {
            return false;
        }
    }

    return true;
}

} // namespace mc::im

#endif // MC_IM_PREFIX_H