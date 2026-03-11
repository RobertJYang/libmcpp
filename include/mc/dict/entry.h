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

#ifndef MC_DICT_ENTRY_H
#define MC_DICT_ENTRY_H

#include <mc/intrusive/intrusive.h>
#include <mc/variant/variant_base.h>
#include <string>
#include <string_view>

namespace mc {
// 前向声明
class dict;
} // namespace mc

namespace mc::dict_types {

/**
 * @brief 表示键值对的结构体，包含侵入式链表和哈希表的钩子
 */
struct entry : public mc::intrusive::list_base_hook<>,
               public mc::intrusive::unordered_set_base_hook<mc::intrusive::link_mode<mc::intrusive::safe_link>> {
    variant key;
    variant value;

    // 默认构造函数
    entry() = default;

    // 带参数的构造函数
    entry(variant k, variant v)
        : key(std::move(k)), value(std::move(v))
    {
    }

    // 拷贝构造函数 - 不拷贝钩子状态
    entry(const entry& other)
        : key(other.key),
          value(other.value)
    {
    }

    // 移动构造函数 - 不移动钩子状态
    entry(entry&& other) noexcept
        : key(std::move(other.key)), value(std::move(other.value))
    {
    }

    // 禁止赋值操作，因为钩子不支持赋值
    entry& operator=(const entry&) = delete;
    entry& operator=(entry&&)      = delete;

    // 相等比较
    friend bool operator==(const entry& a, const entry& b)
    {
        return a.key == b.key && a.value == b.value;
    }

    friend bool operator!=(const entry& a, const entry& b)
    {
        return !(a == b);
    }
};

// 定义哈希函数和相等比较函数
struct key_hash {
    std::size_t operator()(const entry& e) const
    {
        return e.key.hash();
    }
    std::size_t operator()(const std::string& key) const
    {
        return calculate_str_hash(key);
    }
    std::size_t operator()(std::string_view key) const
    {
        return calculate_str_hash(key);
    }
    std::size_t operator()(const char* key) const
    {
        return calculate_str_hash(key);
    }
    std::size_t operator()(const variant& key) const
    {
        return key.hash();
    }
};

struct key_equal {
    bool operator()(const entry& lhs, const entry& rhs) const
    {
        return lhs.key == rhs.key;
    }
    bool operator()(const std::string& key, const entry& e) const
    {
        return key == e.key;
    }
    bool operator()(const entry& e, const std::string& key) const
    {
        return e.key == key;
    }
    // bool operator()(std::string_view key, const entry& e) const {
    //     return key == e.key;
    // }
    bool operator()(const entry& e, std::string_view key) const
    {
        return e.key == key;
    }
    bool operator()(const char* key, const entry& e) const
    {
        return key == e.key;
    }
    bool operator()(const entry& e, const char* key) const
    {
        return e.key == key;
    }
    bool operator()(const variant& key, const entry& e) const
    {
        return key == e.key;
    }
    bool operator()(const entry& e, const variant& key) const
    {
        return e.key == key;
    }
};

// 定义侵入式容器类型
using entry_list = mc::intrusive::list<entry>;
using entry_set  = mc::intrusive::unordered_set<
     entry,
     mc::intrusive::hash<key_hash>,
     mc::intrusive::equal<key_equal>,
     mc::intrusive::constant_time_size<true>>;

// 定义迭代器结构体，继承自底层迭代器，方便 dict 中定义前向声明
struct iterator : public entry_list::iterator {
    using base_type         = entry_list::iterator;
    using iterator_category = typename base_type::iterator_category;
    using value_type        = typename base_type::value_type;
    using difference_type   = typename base_type::difference_type;
    using pointer           = typename base_type::pointer;
    using reference         = typename base_type::reference;

    iterator() = default;
    iterator(const base_type& iter)
        : base_type(iter)
    {
    }
    iterator(base_type&& iter)
        : base_type(std::move(iter))
    {
    }
    iterator(const iterator& other)            = default;
    iterator(iterator&& other)                 = default;
    iterator& operator=(const iterator& other) = default;
    iterator& operator=(iterator&& other)      = default;
};

struct reverse_iterator : public entry_list::reverse_iterator {
    using base_type         = entry_list::reverse_iterator;
    using iterator_category = typename base_type::iterator_category;
    using value_type        = typename base_type::value_type;
    using difference_type   = typename base_type::difference_type;
    using pointer           = typename base_type::pointer;
    using reference         = typename base_type::reference;

    reverse_iterator() = default;
    reverse_iterator(const base_type& iter)
        : base_type(iter)
    {
    }
    reverse_iterator(base_type&& iter)
        : base_type(std::move(iter))
    {
    }
    reverse_iterator(const reverse_iterator& other)            = default;
    reverse_iterator(reverse_iterator&& other)                 = default;
    reverse_iterator& operator=(const reverse_iterator& other) = default;
    reverse_iterator& operator=(reverse_iterator&& other)      = default;
};

struct const_iterator : public entry_list::const_iterator {
    // 继承所有迭代器特性类型
    using base_type         = entry_list::const_iterator;
    using iterator_category = typename base_type::iterator_category;
    using value_type        = typename base_type::value_type;
    using difference_type   = typename base_type::difference_type;
    using pointer           = typename base_type::pointer;
    using reference         = typename base_type::reference;

    // 默认构造函数
    const_iterator() = default;

    // 从基类构造函数构造
    const_iterator(const base_type& iter)
        : base_type(iter)
    {
    }
    const_iterator(base_type&& iter)
        : base_type(std::move(iter))
    {
    }

    // 拷贝和移动构造函数
    const_iterator(const const_iterator& other) = default;
    const_iterator(const_iterator&& other)      = default;

    // 赋值运算符
    const_iterator& operator=(const const_iterator& other) = default;
    const_iterator& operator=(const_iterator&& other)      = default;

    // 从 iterator 构造
    const_iterator(const iterator& iter)
        : base_type(static_cast<const entry_list::iterator&>(iter))
    {
    }
    const_iterator(iterator&& iter)
        : base_type(std::move(static_cast<entry_list::iterator&&>(iter)))
    {
    }
};

// 定义反向迭代器结构体
struct const_reverse_iterator : public entry_list::const_reverse_iterator {
    using base_type         = entry_list::const_reverse_iterator;
    using iterator_category = typename base_type::iterator_category;
    using value_type        = typename base_type::value_type;
    using difference_type   = typename base_type::difference_type;
    using pointer           = typename base_type::pointer;
    using reference         = typename base_type::reference;

    // 默认构造函数
    const_reverse_iterator() = default;

    // 从基类构造
    const_reverse_iterator(const base_type& iter)
        : base_type(iter)
    {
    }

    // 拷贝和移动构造函数
    const_reverse_iterator(const const_reverse_iterator& other) = default;
    const_reverse_iterator(const_reverse_iterator&& other)      = default;

    // 赋值运算符
    const_reverse_iterator& operator=(const const_reverse_iterator& other) = default;
    const_reverse_iterator& operator=(const_reverse_iterator&& other)      = default;

    // 析构函数
    ~const_reverse_iterator() = default;

    // 继承所有操作符，无需重新定义
    // operator*, operator->, operator++, operator==, operator!= 等都会自动继承
};

#ifndef MC_DICT_BUCKET_COUNT
#define MC_DICT_BUCKET_COUNT 8
#endif

// 完整的 data_t 定义
struct data_t : public mc::enable_shared_from_this<data_t> {
    // 哈希表桶数组
    entry_set::bucket_type buckets[MC_DICT_BUCKET_COUNT];
    // 有序链表
    entry_list entries;
    // 哈希表
    entry_set index;

    // 构造函数
    data_t()
        : index(entry_set::bucket_traits(buckets, MC_DICT_BUCKET_COUNT))
    {
    }

    // 析构函数，清理资源
    ~data_t()
    {
        // 先清空索引，这样钩子就不再链接到容器中
        index.clear();
        // 然后清空链表并释放内存
        entries.clear_and_dispose([](entry* p) {
            delete p;
        });
    }
};

} // namespace mc::dict_types

#endif // MC_DICT_ENTRY_H