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

/**
 * @file unordered_set.h
 * @brief 封装 Boost 的侵入式哈希表容器
 */
#ifndef MC_INTRUSIVE_UNORDERED_SET_H
#define MC_INTRUSIVE_UNORDERED_SET_H

#include <boost/intrusive/unordered_set.hpp>
#include <functional>
#include <mc/intrusive/hook.h>
#include <vector>

namespace mc {
namespace intrusive {

/**
 * @brief 哈希函数包装器
 */
template <typename Hasher>
struct hash {
    Hasher m_hasher;

    hash(const Hasher& hasher) : m_hasher(hasher) {
    }

    template <typename T>
    std::size_t operator()(const T& value) const {
        return m_hasher(value);
    }
};

/**
 * @brief 相等比较函数包装器
 */
template <typename Equal>
struct equal {
    Equal m_equal;

    equal(const Equal& equal) : m_equal(equal) {
    }

    template <typename T, typename U>
    bool operator()(const T& lhs, const U& rhs) const {
        return m_equal(lhs, rhs);
    }
};

// 桶类型转换函数
template <typename NodeTraits>
inline boost::intrusive::bucket_impl<NodeTraits>* convert_bucket_ptr(void** ptr) {
    return reinterpret_cast<boost::intrusive::bucket_impl<NodeTraits>*>(ptr);
}

/**
 * @brief 侵入式哈希表容器
 *
 * @tparam T 元素类型
 * @tparam Hasher 哈希函数类型
 * @tparam Equal 相等比较函数类型
 * @tparam HookOptions 钩子选项
 */
template <typename T, typename Hasher, typename Equal, typename... HookOptions>
class unordered_set {
private:
    using boost_unordered_set_type =
        boost::intrusive::unordered_set<T, boost::intrusive::hash<hash<Hasher>>,
                                        boost::intrusive::equal<equal<Equal>>, HookOptions...>;

public:
    // 类型定义
    using value_type      = typename boost_unordered_set_type::value_type;
    using pointer         = typename boost_unordered_set_type::pointer;
    using const_pointer   = typename boost_unordered_set_type::const_pointer;
    using reference       = typename boost_unordered_set_type::reference;
    using const_reference = typename boost_unordered_set_type::const_reference;
    using iterator        = typename boost_unordered_set_type::iterator;
    using const_iterator  = typename boost_unordered_set_type::const_iterator;
    using size_type       = typename boost_unordered_set_type::size_type;
    using difference_type = typename boost_unordered_set_type::difference_type;
    using hasher          = Hasher;
    using key_equal       = Equal;

    /**
     * @brief 构造函数
     *
     * @param buckets 桶数组
     * @param bucket_count 桶数量
     * @param hasher 哈希函数
     * @param equal 相等比较函数
     */
    unordered_set(void** buckets, size_type bucket_count, const hasher& hash_func = hasher(),
                  const key_equal& equal_func = key_equal())
        : m_unordered_set(
              typename boost_unordered_set_type::bucket_traits(
                  convert_bucket_ptr<typename boost_unordered_set_type::node_traits>(buckets),
                  bucket_count),
              hash<hasher>(hash_func), equal<key_equal>(equal_func)) {
    }

    // 禁止拷贝和移动
    unordered_set(const unordered_set&)            = delete;
    unordered_set(unordered_set&&)                 = delete;
    unordered_set& operator=(const unordered_set&) = delete;
    unordered_set& operator=(unordered_set&&)      = delete;

    // 析构函数
    ~unordered_set() = default;

    // 迭代器
    iterator begin() {
        return m_unordered_set.begin();
    }
    const_iterator begin() const {
        return m_unordered_set.begin();
    }
    const_iterator cbegin() const {
        return m_unordered_set.cbegin();
    }
    iterator end() {
        return m_unordered_set.end();
    }
    const_iterator end() const {
        return m_unordered_set.end();
    }
    const_iterator cend() const {
        return m_unordered_set.cend();
    }

    // 容量
    bool empty() const {
        return m_unordered_set.empty();
    }
    size_type size() const {
        return m_unordered_set.size();
    }

    // 修改器
    std::pair<iterator, bool> insert(reference value) {
        return m_unordered_set.insert(value);
    }
    void erase(const_iterator pos) {
        m_unordered_set.erase(pos);
    }
    size_type erase(const_reference value) {
        return m_unordered_set.erase(value);
    }
    void clear() {
        m_unordered_set.clear();
    }

    // 特殊操作
    template <typename Disposer>
    void clear_and_dispose(Disposer disposer) {
        m_unordered_set.clear_and_dispose(disposer);
    }

    // 查找
    template <typename Key>
    iterator find(const Key& key) {
        return m_unordered_set.find(key, m_unordered_set.hash_function(), m_unordered_set.key_eq());
    }

    template <typename Key>
    const_iterator find(const Key& key) const {
        return m_unordered_set.find(key, m_unordered_set.hash_function(), m_unordered_set.key_eq());
    }

    // 获取迭代器
    iterator iterator_to(reference value) {
        return m_unordered_set.iterator_to(value);
    }
    const_iterator iterator_to(const_reference value) const {
        return m_unordered_set.iterator_to(value);
    }

private:
    boost_unordered_set_type m_unordered_set;
};

} // namespace intrusive
} // namespace mc

#endif // MC_INTRUSIVE_UNORDERED_SET_H