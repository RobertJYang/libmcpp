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
 * @file set.h
 * @brief 封装 Boost 的侵入式有序集合容器
 */
#ifndef MC_INTRUSIVE_SET_H
#define MC_INTRUSIVE_SET_H

#include <boost/intrusive/set.hpp>
#include <functional>
#include <mc/intrusive/hook.h>

namespace mc {
namespace intrusive {

/**
 * @brief 比较器包装器
 */
template <typename Compare>
struct compare {
    Compare m_compare;

    compare(const Compare& comp) : m_compare(comp) {
    }

    template <typename T, typename U>
    bool operator()(const T& lhs, const U& rhs) const {
        return m_compare(lhs, rhs);
    }
};

/**
 * @brief 侵入式有序集合（set）
 *
 * @tparam T 元素类型
 * @tparam Compare 比较函数类型
 * @tparam HookOptions 钩子选项
 */
template <typename T, typename Compare = std::less<T>, typename... HookOptions>
class set {
private:
    using boost_set_type = boost::intrusive::set<T, boost::intrusive::compare<compare<Compare>>, 
                                                HookOptions...>;

public:
    // 类型定义
    using value_type      = typename boost_set_type::value_type;
    using pointer         = typename boost_set_type::pointer;
    using const_pointer   = typename boost_set_type::const_pointer;
    using reference       = typename boost_set_type::reference;
    using const_reference = typename boost_set_type::const_reference;
    using iterator        = typename boost_set_type::iterator;
    using const_iterator  = typename boost_set_type::const_iterator;
    using size_type       = typename boost_set_type::size_type;
    using difference_type = typename boost_set_type::difference_type;
    using key_compare     = Compare;
    using value_compare   = Compare;

    /**
     * @brief 构造函数
     *
     * @param comp 比较函数
     */
    set(const key_compare& comp = key_compare())
        : m_set(compare<Compare>(comp)) {
    }

    // 禁止拷贝和移动
    set(const set&)            = delete;
    set(set&&)                 = delete;
    set& operator=(const set&) = delete;
    set& operator=(set&&)      = delete;

    // 析构函数
    ~set() = default;

    // 迭代器
    iterator begin() {
        return m_set.begin();
    }
    const_iterator begin() const {
        return m_set.begin();
    }
    const_iterator cbegin() const {
        return m_set.cbegin();
    }
    iterator end() {
        return m_set.end();
    }
    const_iterator end() const {
        return m_set.end();
    }
    const_iterator cend() const {
        return m_set.cend();
    }

    // 容量
    bool empty() const {
        return m_set.empty();
    }
    size_type size() const {
        return m_set.size();
    }

    // 修改器
    std::pair<iterator, bool> insert(reference value) {
        return m_set.insert(value);
    }
    iterator insert(const_iterator hint, reference value) {
        return m_set.insert(hint, value);
    }
    void erase(const_iterator pos) {
        m_set.erase(pos);
    }
    size_type erase(const_reference value) {
        return m_set.erase(value);
    }
    void clear() {
        m_set.clear();
    }

    // 特殊操作
    template <typename Disposer>
    void clear_and_dispose(Disposer disposer) {
        m_set.clear_and_dispose(disposer);
    }

    // 查找
    iterator find(const_reference value) {
        return m_set.find(value);
    }
    const_iterator find(const_reference value) const {
        return m_set.find(value);
    }
    
    template <typename KeyType, typename KeyCompare>
    iterator find(const KeyType& key, const KeyCompare& comp) {
        return m_set.find(key, comp);
    }
    
    template <typename KeyType, typename KeyCompare>
    const_iterator find(const KeyType& key, const KeyCompare& comp) const {
        return m_set.find(key, comp);
    }

    // 上下界
    iterator lower_bound(const_reference value) {
        return m_set.lower_bound(value);
    }
    const_iterator lower_bound(const_reference value) const {
        return m_set.lower_bound(value);
    }
    
    iterator upper_bound(const_reference value) {
        return m_set.upper_bound(value);
    }
    const_iterator upper_bound(const_reference value) const {
        return m_set.upper_bound(value);
    }
    
    // 获取迭代器
    iterator iterator_to(reference value) {
        return m_set.iterator_to(value);
    }
    const_iterator iterator_to(const_reference value) const {
        return m_set.iterator_to(value);
    }

    // 比较器访问
    key_compare key_comp() const {
        return m_set.key_comp().m_compare;
    }
    value_compare value_comp() const {
        return m_set.value_comp().m_compare;
    }

private:
    boost_set_type m_set;
};

} // namespace intrusive
} // namespace mc

#endif // MC_INTRUSIVE_SET_H 